#pragma once

#include <fstream>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
public:
    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
        std::ifstream numa_balancing_file("/proc/sys/kernel/numa_balancing");
        auto is_enabled = std::int32_t{};
        if (numa_balancing_file >> is_enabled)
        {
            return !(is_enabled == 0);
        }

        return true;
    }

    static uint64_t timestamp()
    {
        uint32_t lo, hi;
        __asm__ __volatile__("xorl %%eax,%%eax\n\t" /* provide constant argument to cpuid to reduce variance */
                             "cpuid\n\t"            /* synchronise, i.e. finish all preceeding instructions */
                             :
                             :
                             : "%rax", "%rbx", "%rcx", "%rdx");
        __asm__ __volatile__("rdtsc"
                             : "=a"(lo), "=d"(hi)
                             :
                             : "memory" /* prevent reordering of asm statements */
        );

        return (uint64_t)hi << 32 | lo;
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