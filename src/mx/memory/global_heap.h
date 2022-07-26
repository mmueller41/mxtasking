#pragma once
#include "alignment_helper.h"
#include <cstdint>
#include <cstdlib>
#include <base/heap.h>

namespace mx::memory {
/**
 * The global heap represents the heap, provided by the OS.
 */
class GlobalHeap
{
private:
    Genode::Heap _heap;

public:
    GlobalHeap() : _heap(Genode::Heap(system::Environment::env()->ram(), system::Environment::env()->rm())) {}

    Genode::Heap &get_heap() { return _heap; }

    static GlobalHeap &get_instance() {
        static GlobalHeap gheap;
        return gheap;
    }

    static Genode::Heap &heap() { return GlobalHeap::get_instance().get_heap(); }

    /**
     * Allocates the given size on the given NUMA node.
     *
     * @param numa_node_id ID of the NUMA node, the memory should allocated on.
     * @param size  Size of the memory to be allocated.
     * @return Pointer to allocated memory.
     */
    static void *allocate(const std::uint8_t numa_node_id, const std::size_t size)
    {
        return std::malloc(size);
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
        void *mem_chunk;
        posix_memalign(&mem_chunk, 64U, alignment_helper::next_multiple(size, 64UL));
        return mem_chunk;
    }

    /**
     * Frees the given memory.
     *
     * @param memory Pointer to memory.
     * @param size Size of the allocated object.
     */
    static void free(void *memory, const std::size_t size) {
        std::free(memory);
    }
};
} // namespace mx::memory