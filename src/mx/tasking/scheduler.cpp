#include "scheduler.h"
#include <cassert>
#include <mx/memory/global_heap.h>
#include <mx/synchronization/synchronization.h>
#include <mx/system/topology.h>
#include <thread>
#include <vector>
#include <base/log.h>
#include <mx/tasking/runtime.h>
#include <internal/thread_create.h>
#include <base/affinity.h>
#include <base/thread.h>
#include <nova/syscalls.h>
#include <cstdlib>
#include <cmath>

using namespace mx::tasking;

std::uint64_t *volatile mx::tasking::runtime::_signal_page;

Scheduler::Scheduler(const mx::util::core_set &core_set, const std::uint16_t prefetch_distance,
                     memory::dynamic::Allocator &resource_allocator, std::uint64_t * volatile signal_page) noexcept
    : _core_set(core_set), _count_channels(core_set.size()), _vacant_channels(_count_channels), _worker({}), _channel_numa_node_map({0U}),
      _epoch_manager(core_set.size(), resource_allocator, _is_running), _statistic(_count_channels)
{
    this->_worker.fill(nullptr);
    this->_channel_numa_node_map.fill(0U);
    Genode::log("Initializing scheduler");
    for (auto worker_id = 0U; worker_id < mx::system::Environment::topo().global_affinity_space().total(); ++worker_id)
    {
        const auto core_id = worker_id;
        this->_channel_numa_node_map[worker_id] = system::topology::node_id(core_id);
        Genode::log("Creating worker ", worker_id, " at node ", this->_channel_numa_node_map[worker_id]);
        auto ptr = memory::GlobalHeap::allocate(this->_channel_numa_node_map[worker_id], sizeof(Worker));
        this->_worker[worker_id] =
            new (ptr)
                Worker(worker_id, core_id, this->_channel_numa_node_map[worker_id], signal_page, this->_is_running, _vacant_channels, _stealing_limit, _remainder_channel_count,
                       prefetch_distance, this->_epoch_manager[worker_id], this->_epoch_manager.global_epoch(),
                       this->_statistic);
        ptr = memory::GlobalHeap::allocate(this->_channel_numa_node_map[worker_id], sizeof(Channel));
        this->_channels[worker_id] =
            new (ptr) Channel(worker_id, this->_channel_numa_node_map[worker_id], prefetch_distance);
        _vacant_channels.push_back(_channels[worker_id]);
        Genode::log("Channel ", worker_id, " created at ", _channels[worker_id]);
    }
}

Scheduler::~Scheduler() noexcept
{
    for (auto *worker : this->_worker)
    {
        std::uint8_t node_id = worker->numa_id();
        worker->~Worker();
        memory::GlobalHeap::free(worker, sizeof(Worker), node_id);
    }

    for (auto *channel : this->_channels)
    {
        std::uint8_t node_id = channel->numa_node_id();
        channel->~Channel();
        memory::GlobalHeap::free(channel, sizeof(Channel), node_id);
    }
}

void Scheduler::start_and_wait()
{
    // Create threads for worker...
    /*std::vector<std::thread> worker_threads(this->_core_set.size() +
                                            static_cast<std::uint16_t>(config::memory_reclamation() != config::None));
    for (auto channel_id = 0U; channel_id < this->_core_set.size(); ++channel_id)
    {
        worker_threads[channel_id] = std::thread([this, channel_id] { this->_worker[channel_id]->execute(); });

        //system::thread::pin(worker_threads[channel_id], this->_worker[channel_id]->core_id());
    }*/
    Genode::Affinity::Space space = mx::system::Environment::topo().global_affinity_space();

    std::vector<pthread_t> worker_threads(space.total() +
                                          static_cast<std::uint16_t>(config::memory_reclamation() != config::None));

    Nova::mword_t start_cpu = 0;
    Nova::cpu_id(start_cpu);

    Genode::Trace::Timestamp start = Genode::Trace::timestamp();
    for (auto cpu = 1U; cpu < space.total(); ++cpu)
    {
        Genode::String<32> const name{"worker", cpu};
        Libc::pthread_create_from_session(&worker_threads[cpu], Worker::entry, _worker[cpu], 4 * 4096, name.string(),
                                          &mx::system::Environment::envp()->cpu(), space.location_of_index(cpu));
    }
    Genode::Trace::Timestamp end = Genode::Trace::timestamp();
    Genode::log("Worker started in ", (end - start), " cycles");

    Genode::log("Creating foreman thread on CPU ", start_cpu);
    
    Channel *qf = _vacant_channels.pop_front();
    _worker[0]->assign(qf);

    Libc::pthread_create_from_session(&worker_threads[0], Worker::entry, _worker[0], 4 * 4096, "foreman",
                                      &mx::system::Environment::envp()->cpu(), space.location_of_index(0) );

    /* Always assign the first channel to the foreman, so that it is guaranteed
    that channel 0 is always processed by worker 0 and, thus, always on the same CPU core.
    This is very useful for benchmarks relying on the TSC. Furthermore, this channel will always be processed. */

    Genode::log("Created foreman thread");

    Genode::log("Allocating ", _core_set.size(), " initial workers.");

    Nova::mword_t allocation = 0;
    Nova::core_allocation(allocation, false);
    Genode::log("Initial cores ", allocation);

    this->allocate_cores(_core_set.size()-1);

    // ... and epoch management (if enabled).
    if constexpr (config::memory_reclamation() != config::None)
    {
        // In case we enable memory reclamation: Use an additional thread.
        Libc::pthread_create_from_session(
        &worker_threads[space.total()], mx::memory::reclamation::EpochManager::enter, &this->_epoch_manager, 4 * 4096,
            "epoch_manager", &mx::system::Environment::cpu(), space.location_of_index(space.total()));
    }

    // Turning the flag on starts all worker threads to execute tasks.
    this->_is_running = true;

    // Wait for the worker threads to end. This will only
    // reached when the _is_running flag is set to false
    // from somewhere in the application.
    for (auto &worker_thread : worker_threads)
    {
        pthread_join(worker_thread, 0);
    }

    if constexpr (config::memory_reclamation() != config::None)
    {
        // At this point, no task will execute on any resource;
        // but the epoch manager has joined, too. Therefore,
        // we will reclaim all memory manually.
        this->_epoch_manager.reclaim_all();
    }
}

void Scheduler::schedule(TaskInterface &task, const std::uint16_t current_channel_id) noexcept
{
    // Scheduling is based on the annotated resource of the given task.
    if (task.has_resource_annotated())
    {
        const auto annotated_resource = task.annotated_resource();
        const auto resource_channel_id = annotated_resource.channel_id();

        // For performance reasons, we prefer the local (not synchronized) queue
        // whenever possible to spawn the task. The decision is based on the
        // synchronization primitive and the access mode of the task (reader/writer).
        if (Scheduler::keep_task_local(task.is_readonly(), annotated_resource.synchronization_primitive(),
                                       resource_channel_id, current_channel_id))
        {
            this->_channels[current_channel_id]->push_back_local(&task);
            if constexpr (config::task_statistics())
            {
                this->_statistic.increment<profiling::Statistic::ScheduledOnChannel>(current_channel_id);
            }
        }
        else
        {
            this->_channels[resource_channel_id]->push_back_remote(&task,
                                                                           this->numa_node_id(current_channel_id));
            if constexpr (config::task_statistics())
            {
                this->_statistic.increment<profiling::Statistic::ScheduledOffChannel>(current_channel_id);
            }
        }
    }

    // The developer assigned a fixed channel to the task.
    else if (task.has_channel_annotated())
    {
        const auto target_channel_id = task.annotated_channel();

        // For performance reasons, we prefer the local (not synchronized) queue
        // whenever possible to spawn the task.
        if (target_channel_id == current_channel_id)
        {
            this->_channels[current_channel_id]->push_back_local(&task);
            if constexpr (config::task_statistics())
            {
                this->_statistic.increment<profiling::Statistic::ScheduledOnChannel>(current_channel_id);
            }
        }
        else
        {
            this->_channels[target_channel_id]->push_back_remote(&task, this->numa_node_id(current_channel_id));
            if constexpr (config::task_statistics())
            {
                this->_statistic.increment<profiling::Statistic::ScheduledOffChannel>(current_channel_id);
            }
        }
    }

    // The developer assigned a fixed NUMA region to the task.
    else if (task.has_node_annotated())
    {
        // TODO: Select random channel @ node, based on load
        assert(false && "NOT IMPLEMENTED: Task scheduling for node.");
    }

    // The task can run everywhere.
    else
    {
        this->_channels[current_channel_id]->push_back_local(&task);
        if constexpr (config::task_statistics())
        {
            this->_statistic.increment<profiling::Statistic::ScheduledOnChannel>(current_channel_id);
        }
    }

    if constexpr (config::task_statistics())
    {
        this->_statistic.increment<profiling::Statistic::Scheduled>(current_channel_id);
    }
}

void Scheduler::schedule(TaskInterface &task) noexcept
{
    if (task.has_resource_annotated())
    {
        const auto &annotated_resource = task.annotated_resource();
        this->_channels[annotated_resource.channel_id()]->push_back_remote(&task, 0U);
        if constexpr (config::task_statistics())
        {
            this->_statistic.increment<profiling::Statistic::ScheduledOffChannel>(annotated_resource.channel_id());
        }
    }
    else if (task.has_channel_annotated())
    {
        this->_channels[task.annotated_channel()]->push_back_remote(&task, 0U);
        if constexpr (config::task_statistics())
        {
            this->_statistic.increment<profiling::Statistic::ScheduledOffChannel>(task.annotated_channel());
        }
    }
    else if (task.has_node_annotated())
    {
        // TODO: Select random channel @ node, based on load
        assert(false && "NOT IMPLEMENTED: Task scheduling for node.");
    }
    else
    {
        assert(false && "NOT IMPLEMENTED: Task scheduling without channel.");
    }
}

void Scheduler::reset() noexcept
{
    this->_statistic.clear();
    this->_epoch_manager.reset();
}

void Scheduler::profile(const std::string &output_file)
{
    this->_profiler.profile(output_file);
    for (auto i = 0U; i < this->_count_channels; ++i)
    {
        Genode::log("Profiling channel ", i, " at ", this->_channels[i]);
        this->_profiler.profile(this->_is_running, *(this->_channels[i]));
    }
}

Channel* Scheduler::steal_for(Worker *thief)
{
}