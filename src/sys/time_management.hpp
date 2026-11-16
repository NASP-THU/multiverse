#pragma once

#include "../sim.hpp"
#include "../types.hpp"

namespace madEscape::llm_system
{
    // Import madrona types into current namespace
    using madrona::ECSRegistry;

    // Time management functions
    uint64_t usToNs(uint64_t microseconds);
    void skipTime_add_time(Engine &ctx, uint64_t t_relative_ns);
    uint64_t skipTime_sort_time_rMin(Engine &ctx);
    void skipTime_remove_time(Engine &ctx, uint64_t x_ns);
    void sys_checkSkipTime(Engine &ctx, NextProcessTimes &t);

    // Custom sorting function for time arrays
    void customSortDescending(uint64_t *array, size_t length);
}
