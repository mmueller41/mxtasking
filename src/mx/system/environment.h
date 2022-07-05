#pragma once
#include <base/component.h>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
private:
    Genode::Env &_env;

public:

    Environment(Genode::Env &env) : _env(env) {}

    /**
     * @return Genode environment capability
     * 
     */
    Genode::Env &env() { return _env; }

    static Environment get_instance(Genode::Env &env) 
    { 
        static Environment e(env);
        return e;
    }

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