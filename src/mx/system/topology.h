#pragma once

#include <topo_session/topo_session.h>
#include <base/affinity.h>
#include <base/thread.h>

#include <algorithm>
#include <cstdint>
#include "environment.h"


namespace mx::system {
/**
 * Encapsulates methods for retrieving information
 * about the hardware landscape.
 */
class topology
{
public:
    /**
     * @return Core where the caller is running.
     */
    static std::uint16_t core_id() 
    {
        Genode::Affinity::Location loc = Genode::Thread::myself()->affinity();
        unsigned width = Environment::topo().global_affinity_space().total();

        return std::uint16_t(loc.xpos() + loc.ypos() * width);
    }

    /**
     * Reads the NUMA region identifier of the given core.
     *
     * @param core_id Id of the core.
     * @return Id of the NUMA region the core stays in.
     */
    static std::uint8_t node_id(const std::uint16_t core_id) { return std::uint8_t(Environment::topo().node_affinity_of(Environment::location(core_id)).id()); }

    /**
     * @return The greatest NUMA region identifier.
     */
    static std::uint8_t max_node_id() { return 7; /*std::uint8_t(Environment::topo().node_count()-1);*/ }

    /**
     * @return Number of available cores.
     */
    static std::uint16_t count_cores() { return std::uint16_t(Environment::topo().global_affinity_space().total()); }
};
} // namespace mx::system