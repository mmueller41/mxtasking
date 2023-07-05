#include "tasking_profiler.h"

#include <iostream>
#include <chrono>
#include <numeric>
#include <cxxabi.h>


//Numa 
#include <mx/tasking/runtime.h>
#include <mx/system/environment.h>
#include <mx/system/topology.h>

#include <base/log.h>

constexpr std::chrono::time_point<std::chrono::high_resolution_clock> TaskingProfiler::tinit;

class PrefetchTask : public mx::tasking::TaskInterface
{
public:
    PrefetchTask() {}
    ~PrefetchTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t core_id, const std::uint16_t /*channel_id*/) override
    {
        TaskingProfiler::task_info** task_data = TaskingProfiler::getInstance().getTaskData();
        TaskingProfiler::queue_info** queue_data = TaskingProfiler::getInstance().getQueueData();
        std::chrono::time_point<std::chrono::high_resolution_clock> tinit = TaskingProfiler::getInstance().getTinit();
        for(size_t j = mx::tasking::config::tasking_array_length(); j > 0; j--)
        {
            task_data[core_id][j] = {0, 0, NULL, tinit, tinit};
            __builtin_prefetch(&task_data[core_id][j], 1, 3);
        }
        
        for(size_t j = mx::tasking::config::tasking_array_length(); j > 0; j--)
        {
            queue_data[core_id][j] = {0, tinit};
            __builtin_prefetch(&queue_data[core_id][j], 1, 3);
        }

        return mx::tasking::TaskResult::make_remove();
    }
};

std::string printFloatUS(std::uint64_t ns)
{
    std::uint64_t remainder = ns % 1000;
    std::uint64_t front = ns / 1000;
    char strRemainder[4];
    
    for(int i = 2; i >= 0; i--)
    {
        strRemainder[i] = '0' + (remainder % 10);
        remainder /= 10;
    }
    strRemainder[3] = '\0';

    std::string message = std::to_string(front) + "." + strRemainder;
    return message;

}

void TaskingProfiler::init(std::uint16_t corenum)
{


    relTime = std::chrono::high_resolution_clock::now();

    corenum++;
    this->total_cores = corenum;
    uint16_t cpu_numa_node= 0;

    //create an array of pointers to task_info structs
    task_data = new task_info*[total_cores];
    
    for (std::uint8_t i = 0; i < total_cores; i++)
    {
        std::uint8_t numa_id = mx::system::topology::node_id(i);
        Topology::Numa_region const &node = mx::system::Environment::node(numa_id);
        void *cast_evade = static_cast<void*>(new Genode::Regional_heap(mx::system::Environment::ram(), mx::system::Environment::rm(), const_cast<Topology::Numa_region&>(node)));
        task_data[i] = static_cast<task_info*>(cast_evade);
        /*
        task_data[i] = new task_info[mx::tasking::config::tasking_array_length()];
        for(size_t j = mx::tasking::config::tasking_array_length(); j > 0; j--)
        {
            task_data[i][j] = {0, 0, NULL, tinit, tinit};
            __builtin_prefetch(&task_data[i][j], 1, 3);
        }
        */
    }

    //create an array of pointers to queue_info structs
    queue_data = new queue_info*[total_cores];
    for (std::uint16_t i = 0; i < total_cores; i++)
    {
        std::uint8_t numa_id = mx::system::topology::node_id(i);
        Topology::Numa_region const &node = mx::system::Environment::node(numa_id);
        void *cast_evade = static_cast<void*>(new Genode::Regional_heap(mx::system::Environment::ram(), mx::system::Environment::rm(), const_cast<Topology::Numa_region&>(node)));
        queue_data[i] = static_cast<queue_info*>(cast_evade);
        /*
        queue_data[i] = new queue_info[mx::tasking::config::tasking_array_length()];
        for(size_t j = mx::tasking::config::tasking_array_length(); j > 0; j--)
        {
            queue_data[i][j] = {0, tinit};
            __builtin_prefetch(&queue_data[i][j], 1, 3);
        }
        */
    }

    //make prefetch tasks for each cpu
    for(int i = 0; i < corenum; i++){
        auto *prefetchTask = mx::tasking::runtime::new_task<PrefetchTask>(i);
        //prefetchTask->annotate(i);
        mx::tasking::runtime::spawn(*prefetchTask);
    }
    

    task_id_counter = new std::uint64_t[total_cores]{0};
    queue_id_counter = new std::uint64_t[total_cores]{0};
}

std::uint64_t TaskingProfiler::startTask(std::uint16_t cpu_core, std::uint32_t type, const char* name)
{
    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
    const std::uint16_t cpu_id = cpu_core;
    const std::uint64_t tid = task_id_counter[cpu_id]++;
    task_info& ti = task_data[cpu_id][tid];

    ti.id = tid;
    ti.type = type;
    ti.name = name;
    ti._start = start;

    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
    overhead += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    return ti.id;
}

void TaskingProfiler::endTask(std::uint16_t cpu_core, std::uint64_t id)
{
    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

    task_info& ti = task_data[cpu_core][id];
    ti._end = std::chrono::high_resolution_clock::now();

    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
    overhead += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

void TaskingProfiler::enqueue(std::uint16_t corenum){
    std::chrono::time_point<std::chrono::high_resolution_clock> timestamp = std::chrono::high_resolution_clock::now();
    
    const std::uint64_t qid = __atomic_add_fetch(&queue_id_counter[corenum], 1, __ATOMIC_SEQ_CST);

    queue_info& qi = queue_data[corenum][qid];
    qi.id = qid;
    qi.timestamp = timestamp;

    std::chrono::time_point<std::chrono::high_resolution_clock> endTimestamp = std::chrono::high_resolution_clock::now();
    taskQueueOverhead += std::chrono::duration_cast<std::chrono::nanoseconds>(endTimestamp - timestamp).count();
}

void TaskingProfiler::saveProfile()
{   
    Genode::log("Saving Profile");
    std::uint64_t overhead_ms = overhead/1000000;
    Genode::log("Overhead-Time: ", overhead_ms*1000000, "ns in ms: ", overhead_ms, "ms");
    std::uint64_t taskQueueOverhead_ms = taskQueueOverhead/1000000;
    Genode::log("TaskQueueOverhead-Time: ", taskQueueOverhead_ms*1000000, "ns in ms: ", taskQueueOverhead_ms, "ms");

    //get the number of tasks overal
    std::uint64_t tasknum = 0;
    for (std::uint16_t i = 0; i < total_cores; i++)
    {
        tasknum += task_id_counter[i];
    }
    Genode::log("Number of tasks: ", tasknum);
    Genode::log("Overhead-Time per Task: ", overhead/tasknum, "ns");

    bool first = false;
    std::uint64_t firstTime = 0;
    std::uint64_t throughput = 0;
    std::uint64_t duration = 0;
    std::uint64_t lastEndTime = 0;
    std::uint64_t taskQueueLength;

    std::uint64_t timestamp = 0;
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    char* name;
    std::string timestampString;

    std::uint64_t taskCounter = 0;
    std::uint64_t queueCounter = 0;

    Genode::log("--Save--");
    Genode::log("{\"traceEvents\":[");

    //Events
    for(std::uint16_t cpu_id = 0; cpu_id < total_cores; cpu_id++)
    {
        //Metadata Events for each core (CPU instead of process as name,...)
        Genode::log("{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":", cpu_id, ",\"tid\":", cpu_id, ",\"args\":{\"name\":\"CPU\"}},");
        Genode::log("{\"name\":\"process_sort_index\",\"ph\":\"M\",\"pid\":", cpu_id, ",\"tid\":", cpu_id, ",\"args\":{\"name\":", cpu_id, "}},");
        


        if (mx::tasking::config::use_task_queue_length()){
            taskQueueLength = 0;
            taskCounter = 0;
            queueCounter = 1;

            //Initial taskQueueLength is zero
            timestampString = printFloatUS(0);
            Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id, "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskQueueLength\":", taskQueueLength, "}},");

            //go through all tasks and queues
            while(taskCounter<task_id_counter[cpu_id] || queueCounter<queue_id_counter[cpu_id]){
                //get the next task and queue
                queue_info& qi = queue_data[cpu_id][queueCounter];
                task_info& ti = task_data[cpu_id][taskCounter];

                //get the time from the queue element if existing
                if(queueCounter < queue_id_counter[cpu_id]){
                    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(qi.timestamp - relTime).count();
                }

                //get the time's from the task element if existing
                if(taskCounter < task_id_counter[cpu_id]){
                    start = std::chrono::duration_cast<std::chrono::nanoseconds>(ti._start - relTime).count();
                    end = std::chrono::duration_cast<std::chrono::nanoseconds>(ti._end - relTime).count();
                    name = abi::__cxa_demangle(ti.name, 0, 0, 0);
                }

                //get the first time
                if(!first){
                    first = true;
                    if(timestamp < start){
                        firstTime = timestamp;
                    }
                    else{
                        firstTime = start;
                    }
                }
                //if the queue element is before the task element, it is an enqueue
                //if same time, we print queue first
                if(qi.timestamp <= ti._start && queueCounter <= queue_id_counter[cpu_id]){
                    queueCounter++;
                    taskQueueLength++;

                    if(timestamp - firstTime == 0){
                        timestampString = printFloatUS(10);
                    }
                    else{
                        if(timestamp - firstTime > 0){
                            timestampString = printFloatUS(timestamp-firstTime);
                        }
                        else{
                            timestampString = printFloatUS(10);
                        }
                    }
                    Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id,  "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskQueueLength\":", taskQueueLength, "}},");

                }
                //else we print the task itself and a dequeue
                else{
                    taskCounter++;
                    taskQueueLength--;

                    //taskQueueLength
                    if (start-firstTime > 0){
                        timestampString = printFloatUS(start-firstTime);
                    }
                    else{
                        timestampString = printFloatUS(10);
                    }

                    Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id, "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskQueueLength\":", taskQueueLength, "}},");

                    //if the endtime of the last task is too large (cannot display right)
                    if(taskCounter == task_id_counter[cpu_id] && ti._end == tinit){
                        end = start + 1000;
                    }
                    //Task itself
                    Genode::log("{\"pid\":", cpu_id, ",\"tid\":", cpu_id, ",\"ts\":", timestampString.c_str(), ",\"dur\":", printFloatUS(end-start).c_str(), ",\"ph\":\"X\",\"name\":\"", std::string(name).c_str(), "\",\"args\":{\"type\":", ti.type, "}},");
                    
                    //reset throughput
                    //Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id,  "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskThroughput\":", 0, "}},");

                    duration = end - start;
                    if (end - start < 0){
                        duration = 1000;
                    }

                    //Task Throughput
                    if (duration == 0){
                        //if duration is zero, we set throughput to zero
                        //ask what to do if duration is zero
                        duration = 1;
                        throughput = 0;
                    }
                    else{
                        throughput = 1000/duration;
                    }
                    Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id,  "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskThroughput\":", throughput, "}},");
                    lastEndTime = end;
                }
            }
        }
        else{
            for(std::uint32_t i = 0; i < task_id_counter[cpu_id]; i++){
                task_info& ti = task_data[cpu_id][i];

                // check if task is valid
                if(ti._end == tinit)
                {
                    continue;
                }
                start = std::chrono::duration_cast<std::chrono::nanoseconds>(ti._start - relTime).count();
                end = std::chrono::duration_cast<std::chrono::nanoseconds>(ti._end - relTime).count();
                name = abi::__cxa_demangle(ti.name, 0, 0, 0);

                //Task itself
                timestampString = printFloatUS(start-firstTime);
                Genode::log("{\"pid\":", cpu_id, ",\"tid\":", cpu_id, ",\"ts\":", timestampString.c_str(), ",\"dur\":", printFloatUS(end-start).c_str(), ",\"ph\":\"X\",\"name\":\"", std::string(name).c_str(), "\",\"args\":{\"type\":", ti.type, "}},");
                    
                //reset throughput if there is a gap of more than 1us
                if (start - lastEndTime > 1000){
                    timestampString = printFloatUS(lastEndTime - firstTime);
                    Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id,  "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskThroughput\":", 0, "}},");
                }
                duration = end - start;

                //Task Throughput
                timestampString = printFloatUS(start-firstTime);
                throughput = 1000/duration;
                Genode::log("{\"pid\":", cpu_id, ",\"name\":\"CPU", cpu_id, "\",\"ph\":\"C\",\"ts\":", timestampString.c_str(), ",\"args\":{\"TaskThroughput\":", throughput, "}},");
                lastEndTime = end;
            }
        }
        lastEndTime = 0;
    }
            //sample Task (so we dont need to remove the last comma)
            Genode::log("{\"name\":\"sample\",\"ph\":\"P\",\"ts\":0,\"pid\":5,\"tid\":0}");
            Genode::log("]}");
}

//Destructor
void TaskingProfiler::free()
{
    std::uint16_t total_cores = TaskingProfiler::getInstance().getTotalCores();
    for(std::uint16_t cpu_id = 0; cpu_id < total_cores; cpu_id++)
    {
        delete[] task_data[cpu_id];
    }
    delete[] task_data;
    delete[] TaskingProfiler::getInstance().getTaskIdCounter();
    delete[] TaskingProfiler::getInstance().getQueueIdCounter();
}


//Code for the TaskingProfiler::printTP function
/*
void TaskingProfiler::printTP(std::uint64_t start, std::uint64_t end)
{
    std::uint64_t tp[total_cores]{0};

    for(std::uint16_t cpu_id = 0; cpu_id < total_cores; cpu_id++)
    {
        // get the task_info array for the current core
        task_info* core_data = task_data[cpu_id];

        // get the id counter for the current core
        for(std::uint64_t i = 0; i < task_id_counter[cpu_id]; i++)
        {
            task_info& ti = core_data[i];
            const std::uint64_t tstart = std::chrono::duration_cast<std::chrono::nanoseconds>(ti._start - relTime).count();
            const std::uint64_t tend = std::chrono::duration_cast<std::chrono::nanoseconds>(ti._end - relTime).count();            
            if(tstart > start && tend < end) {
                tp[cpu_id]++;
            }
        }

        LOG_INFO("TP " << cpu_id << " " << tp[cpu_id]);
    }
    LOG_INFO("TP " << "total " << std::accumulate(tp, tp + total_cores, 0));
}
*/
