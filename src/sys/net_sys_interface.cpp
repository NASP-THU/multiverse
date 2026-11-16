#include "../types.hpp"
#include "../sim.hpp"
#include "../network/queue_system.hpp"
#include "../network/flow_system.hpp"
#include "../types/simulation_types.hpp"
#include "../types/global_types.hpp"

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

using madEscape::_dequeue_flow;
using madEscape::_enqueue_flow;
using madEscape::CompletedFlowQueue;
using madEscape::Engine;
using madEscape::Entity;
using madEscape::FlowEvent;
using madEscape::FlowState;
using madEscape::get_queue_len;
using madEscape::NewFlowQueue;
using madEscape::SimTime;
using madEscape::SysFlow;

namespace net_sys_interface
{

    // network interface
    // Get current simulation time
    // func
    uint64_t getCurrentTime(Engine &ctx)
    {
        if (DEV_MODE == 0)
        {
            return 0;
        }
        else
        {
            Entity net_npu = ctx.data()._net_npus[0];
            return ctx.get<SimTime>(net_npu).sim_time;
        }
    }

    // network interface
    // Indicate whether there are currently flows being sent
    // func
    bool isExistedFlow(Engine &ctx)
    {
        return false;
    }

    // network interface
    // Add simulation time: make all components increase simulation time
    // func
    void addSimtime(Engine &ctx, uint64_t t_ns)
    {
        (void)ctx;
        (void)t_ns;
        // ctx.get<SimTime>(ctx.data().timer_entity).time += t_ns;
    }

    void setFlow(Engine &ctx,uint32_t npu_id, uint64_t comm_src, uint64_t comm_dst, uint64_t flow_size, uint32_t flow_id)
    {

        if (DEV_MODE == 0)
        {
            // printf("ENABLE_TEST set flow id %u: %lu->%lu %lu, \n", flow_id, comm_src, comm_dst, flow_size);
        }
        else
        {
            #if SYS_LOG
            if (SYS_LOG_TARGET_NODE == npu_id) {
            printf("set flow id %u: %lu->%lu %lu, \n", flow_id, comm_src, comm_dst, flow_size);
            }
            #endif
            Entity net_npu = ctx.data()._net_npus[npu_id];
            int64_t sim_time = ctx.get<SimTime>(net_npu).sim_time;
            FlowEvent flow_event = {(uint64_t)flow_id, (uint32_t)comm_src, (uint32_t)comm_dst, 1, 0, flow_size, sim_time, 0, FlowState::UNCOMPLETE};

            ctx.data().flow_lock.lock();
            _enqueue_flow(ctx.get<NewFlowQueue>(net_npu), flow_event);
            ctx.data().flow_lock.unlock();

            // printf("set flow id %u: %lu->%lu %lu, \n", flow_id, comm_src, comm_dst, flow_size);
        }
    }

    // network interface
    // Detect/retrieve flow completion events: return flow completion count, and assign flow completion events to flows_finish
    // func
    // uint32_t total_flow_num_finish = 0;

    uint32_t checkFlowFinish(Engine &ctx, uint32_t npu_id, SysFlow flows_finish[])
    {
        Entity net_npu = ctx.data()._net_npus[npu_id];
        CompletedFlowQueue _completed_flow_queue = ctx.get<CompletedFlowQueue>(net_npu);
        uint32_t flow_num_finish = get_queue_len(_completed_flow_queue);
        for (uint32_t i = 0; i < flow_num_finish; i++)
        {
            FlowEvent flow_event;
            _dequeue_flow(_completed_flow_queue, flow_event);

            flows_finish[i].id = (uint32_t)flow_event.flow_id;
            flows_finish[i].comm_size = flow_event.flow_size;
            // flows_finish[i].comm_src = (uint32_t)flow_event.src;
            // flows_finish[i].comm_dst = (uint32_t)flow_event.dst;
            flows_finish[i].comm_src = (uint64_t)flow_event.src;
            flows_finish[i].comm_dst = (uint64_t)flow_event.dst;
            flows_finish[i].durationMicros = (uint32_t)(flow_event.stop_time - flow_event.start_time);

            // #if SYS_LOG
            if (SYS_LOG_TARGET_NODE == npu_id) {
                printf("[FCT] npu_id: %u, check flow finish: flow id %u: src %lu-> dst %lu, flow size: %lu, start_time: %ld, stop_time: %ld, durationMicros: %u \n",
                        npu_id, flows_finish[i].id, flows_finish[i].comm_src, flows_finish[i].comm_dst, flows_finish[i].comm_size,
                        flow_event.start_time, flow_event.stop_time, flows_finish[i].durationMicros);
            }
            // #endif
            
        }

        // ctx.data().flow_lock.lock();
        // total_flow_num_finish += flow_num_finish;
        // ctx.data().flow_lock.unlock();
        // if (flow_num_finish > 0) {
        //     printf("flow_finished_num: %d\n", total_flow_num_finish);
        // }

        return flow_num_finish;
    }

}
