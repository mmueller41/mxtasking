#pragma once
#include <libc/component.h>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
private:
    /**
     * @return Genode environment capability
     * 
     */
    Libc::Env *_env;

public:
    Environment() = default;

    Libc::Env *getenv() { return _env; }
    void setenv(Libc::Env *env) { _env = env; }

    static void setenv(Libc::Env *env) { Environment::get_instance().setenv(env);  }

    static Environment& get_instance() { static Environment env;
        return env;
    }

    static Libc::Env *env() { return Environment::get_instance().getenv(); }

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