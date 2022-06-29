#pragma once

#include <fstream>
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
    Genode::Env &env;

    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
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