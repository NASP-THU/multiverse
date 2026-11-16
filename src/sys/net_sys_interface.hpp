#pragma once

#include "../sim.hpp"
#include "../types.hpp"

namespace net_sys_interface
{
    // Import madrona types into current namespace
    using madEscape::Engine;

    // Import madEscape types into current namespace
    using madEscape::SysFlow;

    // network interface
    uint64_t getCurrentTime(Engine &ctx);

    bool isExistedFlow(Engine &ctx);

    void addSimtime(Engine &ctx, uint64_t t_ns);

    // void setFlow(Engine &ctx, uint64_t comm_src, uint64_t comm_dst, uint64_t comm_size, uint32_t flow_id);
    void setFlow(Engine &ctx, uint32_t npu_id,uint64_t comm_src, uint64_t comm_dst, uint64_t flow_size, uint32_t flow_id);

    uint32_t checkFlowFinish(Engine &ctx, uint32_t npu_id, SysFlow flows_finish[]);

}
