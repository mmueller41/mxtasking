#pragma once
#include "alignment_helper.h"
#include <cstdint>
#include <cstdlib>
#include <array>
#include <cstring>

#include <base/regional_heap.h>
#include <mx/system/topology.h>
#include <mx/system/environment.h>
#include <mx/memory/config.h>

#include <base/log.h>
namespace mx::memory {
/**
 * The global heap represents the heap, provided by the OS.
 */
class GlobalHeap
{
private:
    alignas(64) std::array<Genode::Regional_heap *, config::max_numa_nodes()> heaps{nullptr};

public:

    GlobalHeap() 
    {
        for (unsigned numa_id = 0; numa_id <= system::topology::max_node_id(); numa_id++) {
            Topology::Numa_region const &node = system::Environment::node(numa_id);
            heaps[numa_id] = new Genode::Regional_heap(system::Environment::ram(), system::Environment::rm(), const_cast<Topology::Numa_region&>(node));
        }
    }

    ~GlobalHeap()
    {
        for (auto heap : heaps)
            delete heap;
    }

    inline void *local_allocate(const std::uint8_t numa_node_id, const std::size_t size)
    {
        return heaps[numa_node_id]->alloc(size);
    }

    inline void local_free(void *ptr, const std::size_t size, std::uint8_t node_id)
    {
        bool success = false;
        std::uint8_t next_try = 0;

        do {
        try
        {
            heaps[node_id]->free(ptr, size);
            success = true;
        }
        catch (Genode::Region_map::Invalid_dataspace)
        {
            success = false;
            node_id = (node_id == next_try) ? (next_try + 1) % (mx::system::topology::max_node_id()+1) : next_try;
            next_try = (next_try + 1) % (mx::system::topology::max_node_id()+1);
        }
        } while (next_try != 0);

        if (!success)
            std::free(ptr);
    }

    static GlobalHeap &myself()
    {
        static GlobalHeap gheap;
        return gheap;
    }



    /**
     * Allocates the given size on the given NUMA node.
     *
     * @param numa_node_id ID of the NUMA node, the memory should allocated on.
     * @param size  Size of the memory to be allocated.
     * @return Pointer to allocated memory.
     */
    static void *allocate(const std::uint8_t numa_node_id, const std::size_t size)
    {
        return myself().local_allocate(numa_node_id, size);
    }

    /**
     * Allocates the given memory aligned to the cache line
     * with a multiple of the alignment as a size.
     * The allocated memory is not NUMA aware.
     * @param size Size to be allocated.
     * @return Allocated memory
     */
    static void *allocate_cache_line_aligned(const std::size_t size)
    {
        void *ptr = nullptr;
        posix_memalign(&ptr, 64, alignment_helper::next_multiple(size, 64UL));
        return ptr;
    }


    /**
     * Frees the given memory.
     *
     * @param memory Pointer to memory.
     * @param size Size of the allocated object.
     */
    static void free(void *memory, const std::size_t size, std::uint8_t node_id)  { myself().local_free(memory, size, node_id); }
};
} // namespace mx::memory