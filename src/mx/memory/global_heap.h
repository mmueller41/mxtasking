#pragma once
#include "alignment_helper.h"
#include <cstdint>
#include <cstdlib>
#include <base/heap.h>
#include <mx/system/environment.h>

namespace mx::memory {
/**
 * The global heap represents the heap, provided by the OS.
 * TODO: Use Genode's interface here.
 */
class GlobalHeap
{
private:
    static Genode::Heap _heap;

public:
    static Genode::Heap &heap() { return _heap; }

    /**
     * Allocates the given size on the given NUMA node.
     *
     * @param numa_node_id ID of the NUMA node, the memory should allocated on.
     * @param size  Size of the memory to be allocated.
     * @return Pointer to allocated memory.
     */
    static void *allocate(const std::uint8_t numa_node_id, const std::size_t size)
    {
        /* TODO: Use component's heap */
        _heap.alloc(size);
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
        /* TODO: Use component's heap, as std::aligned_alloc might not be thread-safe */
        return _heap.alloc(alignment_helper::next_multiple(size, 64UL));
    }

    /**
     * Frees the given memory.
     *
     * @param memory Pointer to memory.
     * @param size Size of the allocated object.
     */
    static void free(void *memory, const std::size_t size) { /* TODO: Free via Genode component's heap */
        _heap.free(memory, size);
    }
};
} // namespace mx::memory