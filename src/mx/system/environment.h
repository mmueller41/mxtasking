#pragma once
#include <libc/component.h>
#include <base/ram_allocator.h>
#include <topo_session/topo_session.h>
#include <base/affinity.h>
#include <topo_session/node.h>
#include <cstdint>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
private:
    /**
     * @brief Pointer to application's environment
     * @details The application's environment grants access to core services of EalánOS, such as thread creation and memory allocation.
     */
    Libc::Env *_env;

public:
    Environment() = default;

    /**
     * @brief Get Enviroment of current application
     * 
     * @return Libc::Env& environment
     */
    Libc::Env &getenv() { return *_env; }
    
    /**
     * @brief Set libc environment for MxTasking 
     * @details 
     * @param env pointer to libc enviroment object
     */
    void setenv(Libc::Env *env) { _env = env; }

    /**
     * @brief Get the instance object
     * 
     * @return Environment& singleton object of Environment
     */
    static Environment &get_instance() 
    {
        static Environment env;
        return env;
    }
    
    /**
    * @brief Quick access to libc environment
    * 
    * @return Libc::Env& 
    */
    static Libc::Env &env() { return Environment::get_instance().getenv(); }
    
    /**
     * @brief Set the libc env object
     * 
     * @details This method **must** be called before creating the MxTasking runtime, because the environment
     *          is vital for the initialization of the MxTasking runtime enviroment.
     * @param env pointer to application's environment
     */
    static void set_env(Libc::Env *env) {
        Environment::get_instance().setenv(env);
    }

    /*
     * Access methods to core sessions of the libc enviroment, MxTasking is running in
     */
    static Genode::Cpu_session &cpu() { return Environment::env().cpu(); }
    static Genode::Ram_allocator &ram() { return Environment::env().ram(); }
    static Genode::Region_map &rm() { return Environment::env().rm(); }
    static Genode::Topo_session &topo() { return Environment::env().topo(); }

    /**
     * @brief Convert integral core_id to location in the affinity space of the component
     * 
     * @param core_id The core_id to convert
     * @return Genode::Affinity::Location const& corresponding location in affinity space
     */
    static Genode::Affinity::Location location(std::uint16_t core_id) { return cpu().affinity_space().location_of_index(core_id); }
    
    /**
     * @brief Get NUMA node object for respective numa_id
     * 
     * @param numa_id of Node to return
     * @return Topology::Numa_region const& the corresponding node object
     */
    static Topology::Numa_region node(std::uint8_t numa_id) { return topo().node_at_id(numa_id); }
    
    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
        return false;
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