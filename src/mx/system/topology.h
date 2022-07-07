#pragma once

#include <algorithm>
#include <cstdint>
#include "environment.h"

namespace mx::system {
/**
 * Encapsulates methods for retrieving information
 * about the hardware landscape. */
 /* TODO: Get topology information from Genode. This is a *huge* task because as of now Genode does not even know about NUMA. */
 
class topology
{
public:
    /**
     * @return Core where the caller is running.
     */
    static std::uint16_t core_id() { return std::uint16_t(0); } // no way of getting CPU id yet }

    /**
     * Reads the NUMA region identifier of the given core.
     *
     * @param core_id Id of the core.
     * @return Id of the NUMA region the core stays in.
     */
    static std::uint8_t node_id(const std::uint16_t core_id) {
        return 0; // no NUMA support yet
    }

    /**
     * @return The greatest NUMA region identifier.
     */
    static std::uint8_t max_node_id() { return std::uint8_t(1); }

    /**
     * @return Number of available cores.
     */
    static std::uint16_t count_cores() { return std::uint16_t(Environment::env->cpu().affinity_space().total());
     }
};
} // namespace mx::system