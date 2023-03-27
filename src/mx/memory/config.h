#pragma once
#include <chrono>

namespace mx::memory {
class config
{
public:
    /**
     * @return Number of maximal provided NUMA regions.
     */
    static constexpr std::uint8_t max_numa_nodes() { return 8U; }

    /**
     * @return Interval of each epoch, if memory reclamation is used.
     */
    static constexpr std::chrono::milliseconds epoch_interval() { return std::chrono::milliseconds(50U); }

    /**
     * @return True, if garbage is removed local.
     */
    static constexpr bool local_garbage_collection() { return false; }
};
} // namespace mx::memory