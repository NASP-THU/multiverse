#include "time_management.hpp"
#include "net_sys_interface.hpp"
#include "../types/global_types.hpp"

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

using net_sys_interface::addSimtime;
using net_sys_interface::getCurrentTime;
using net_sys_interface::isExistedFlow;

// Log formatting macros
#define LOG_SUBSEPARATOR "-------------------------------------------------------------------------------"

#if SYS_LOG
#define LOG_SYS_HEADER(sys_name, sys_desc) \
    printf("\n[SYS-%s] %s\n%s\n", sys_name, sys_desc, LOG_SUBSEPARATOR);
#define LOG_TASK_START(task_name) \
    printf("  > %s\n", task_name);
#define LOG_INFO(msg, ...) \
    printf("  INFO: " msg "\n", ##__VA_ARGS__);
#else
#define LOG_SYS_HEADER(sys_name, sys_desc) \
    do                                     \
    {                                      \
    } while (0)
#define LOG_TASK_START(task_name) \
    do                            \
    {                             \
    } while (0)
#define LOG_INFO(msg, ...) \
    do                     \
    {                      \
    } while (0)
#endif

namespace madEscape::llm_system
{

    uint64_t usToNs(uint64_t microseconds)
    {
        return microseconds * 1000; // 1 us = 1,000 ns
    }


    void skipTime_add_time(Engine &ctx, uint64_t t_relative_ns)
    {
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; i++)
        {
            if (ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs[i] != 0)
            {
                ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs[i] = getCurrentTime(ctx) + t_relative_ns;
                break;
            }
        }
    }

    // Custom sorting function: bubble sort from large to small
    void customSortDescending(uint64_t *array, size_t length)
    {
        for (size_t i = 0; i < length - 1; ++i)
        {
            for (size_t j = 0; j < length - 1 - i; ++j)
            {
                if (array[j] < array[j + 1])
                { // Compare from large to small
                  // Swap two elements
                    uint64_t temp = array[j];
                    array[j] = array[j + 1];
                    array[j + 1] = temp;
                }
            }
        }
    }

    uint64_t skipTime_sort_time_rMin(Engine &ctx)
    {
        // Find the position of the first element equal to 0
        size_t zeroIndex = MAX_CHAKRA_NODES_PER_NPU; // Default: no 0 found

        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            if (ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs[i] == 0)
            {
                zeroIndex = i;
                break;
            }
        }

        // Sort elements before the first 0 from large to small
        if (zeroIndex > 0 && zeroIndex < MAX_CHAKRA_NODES_PER_NPU)
        {
            uint64_t *array = ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs;
            customSortDescending(array, zeroIndex);
        }

        return ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs[zeroIndex];
    }

    void skipTime_remove_time(Engine &ctx, uint64_t x_ns)
    {
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            if (ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs[i] != 0)
            {
                ctx.get<NextProcessTimes>(ctx.data().next_process_time_entity).times_abs[i] -= x_ns;
            }
            else
            {
                break;
            }
        }
    }

    uint16_t frame_skiptime = 0;
    void sys_checkSkipTime(Engine &ctx, NextProcessTimes &t)
    {
        (void)t;
        LOG_SYS_HEADER("4", "Time Skip Check System");
        LOG_TASK_START("Checking if simulation time skip is needed");

        frame_skiptime++;
        if (frame_skiptime / CHECK_SKIPTIME_INTERVAL_PER_FRAME == 0)
        {
            frame_skiptime = 0;
            if (!isExistedFlow(ctx))
            {
                uint64_t min_time = skipTime_sort_time_rMin(ctx);
                if (min_time != 0)
                {
                    addSimtime(ctx, min_time - getCurrentTime(ctx));
                    LOG_INFO("Time skipped to: %ld ns", getCurrentTime(ctx));
                    skipTime_remove_time(ctx, min_time);
                }
            }
        }
    }

}