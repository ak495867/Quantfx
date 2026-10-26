#pragma once

#include <cstdint>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

namespace qfx {

inline bool set_cpu_affinity(int cpu_id) noexcept {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)cpu_id;
    return false;
#endif
}

inline bool set_realtime_priority(int priority = 50) noexcept {
#if defined(__linux__)
    struct sched_param param;
    param.sched_priority = priority;
    return sched_setscheduler(0, SCHED_FIFO, &param) == 0;
#else
    (void)priority;
    return false;
#endif
}

inline bool lock_memory() noexcept {
#if defined(__linux__)
    return mlockall(MCL_CURRENT | MCL_FUTURE) == 0;
#else
    return false;
#endif
}

inline bool enable_hugepages(void* addr, size_t size) noexcept {
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    return madvise(addr, size, MADV_HUGEPAGE) == 0;
#else
    (void)addr;
    (void)size;
    return false;
#endif
}

}
