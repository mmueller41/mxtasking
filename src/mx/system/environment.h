#pragma once
#include <base/component.h>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
public:

    /**
     * @return Genode environment capability
     * 
     */
    static Genode::Env *env;

    

    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
        /* TODO: Find out if this infe can be aquired at runtime for Genode, maybe from the config */
        return true;
    }

    static constexpr auto is_sse2()
    {
#ifdef USE_SSE2
        return true;
#else
        return false;
#endif
    }
};
} // namespace mx::system