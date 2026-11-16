#include "communication_system.hpp"
#include "net_sys_interface.hpp"
#include "../types/global_types.hpp"
#include "../sim.hpp"

using namespace madEscape;
using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

using net_sys_interface::checkFlowFinish;
using net_sys_interface::getCurrentTime;
using net_sys_interface::setFlow;

// Log formatting macros
#define LOG_SUBSEPARATOR "-------------------------------------------------------------------------------"

#if SYS_LOG
#define LOG_SYS_HEADER(sys_name, sys_desc) \
    printf("\n[SYS-%s] %s\n%s\n", sys_name, sys_desc, LOG_SUBSEPARATOR);
#define LOG_TASK_START(task_name) \
    printf("  > %s\n", task_name);
#define LOG_TASK_SUCCESS(task_name) \
    printf("  [SUCCESS] %s\n", task_name);
#define LOG_NODE_COMPLETE(node_id) \
    printf("    Node [%d] completed\n", node_id);
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
#define LOG_TASK_SUCCESS(task_name) \
    do                              \
    {                               \
    } while (0)
#define LOG_NODE_COMPLETE(node_id) \
    do                             \
    {                              \
    } while (0)
#define LOG_INFO(msg, ...) \
    do                     \
    {                      \
    } while (0)
#endif

namespace madEscape::llm_system
{

    // Get the number of communications (ring) per phase
    int get_ring_comm_count_per_phase(CollectiveCommType comm_type,
                                      CommImplementationType comm_implementation_type, int nodes_in_ring)
    {
        (void)comm_implementation_type;
        int stream_count = 0;
        switch (comm_type)
        {
        case CollectiveCommType::ALL_REDUCE:
            stream_count = 2 * (nodes_in_ring - 1);
            break;
        case CollectiveCommType::ALL_TO_ALL:
            stream_count = ((nodes_in_ring - 1) * nodes_in_ring) / 2;
            break;
        default:
            stream_count = nodes_in_ring - 1;
        }
        return stream_count;
    }

    // Get the flow size per phase and the final data amount per node
    void get_ring_comm_size_per_flow(CollectiveCommType comm_type,
                                     CommImplementationType comm_implementation_type, uint64_t data_size, int nodes_in_ring, uint64_t &msg_size, uint64_t &final_data_size)
    {
        (void)comm_implementation_type;
        msg_size = data_size;
        switch (comm_type)
        {
        case CollectiveCommType::ALL_REDUCE:
            final_data_size = data_size;
            msg_size = data_size / nodes_in_ring;
            break;
        case CollectiveCommType::ALL_GATHER:
            final_data_size = data_size * nodes_in_ring;
            msg_size = data_size;
            break;
        case CollectiveCommType::REDUCE_SCATTER:
            final_data_size = data_size / nodes_in_ring;
            msg_size = data_size / nodes_in_ring;
            break;
        case CollectiveCommType::ALL_TO_ALL:
            final_data_size = data_size;
            msg_size = data_size / nodes_in_ring;
            break;
        default:
            msg_size = data_size;
            break;
        }
    }

    Direction get_comm_Ring_Direction()
    {
        //      if ((backend != AstraNetworkAPI::BackendType::Garnet || level > 0) && queues.size() > 1 &&
        //     allocator >= (queues.size() / 2)) {
        //     dir = RingTopology::Direction::Anticlockwise;
        // } else {
        //     dir = RingTopology::Direction::Clockwise;
        // }
        return Direction::Clockwise;
    }

    bool setNextExecFlows(Engine &ctx, TaskFlows &taskFlows, NpuID &npu_id)
    {
        // SysFlow flows_exec[MAX_FLOW_NUM_PER_COMM_NODE];
        uint32_t flow_exec_count = taskFlows.getNextExecFlows(npu_id.flows_exec);
#if SYS_LOG
        if (SYS_LOG_TARGET_NODE == npu_id.value)
        {
            printf("  INFO: setNextExecFlows: flow_exec_count=%u\n", flow_exec_count);
        }
#endif
        if (flow_exec_count > 0)
        {
            for (size_t i = 0; i < flow_exec_count; i++)
            {
                setFlow(ctx, npu_id.value, npu_id.flows_exec[i].comm_src, npu_id.flows_exec[i].comm_dst, npu_id.flows_exec[i].comm_size, npu_id.flows_exec[i].id);
            }
            npu_id.flows_exec_init();
            return true;
        }
        else
        {
            return false;
        }
    }

    void sys_checkFlow(Engine &ctx, NpuID &npu_id, NodeID &node_id, TaskFlows &taskFlows)
    {
        #if SYS_LOG
        if (SYS_LOG_TARGET_NODE == npu_id.value)
        {
            LOG_SYS_HEADER("2", "Flow Status Check System");
            LOG_TASK_START("Checking network flow completion status");
        }
        #endif

        if (DEV_MODE == 0)
        {
            if (!taskFlows.is_exec)
            {
                taskFlows.is_exec = true;
                // first
                setNextExecFlows(ctx, taskFlows, npu_id);
            }
            else
            {
                taskFlows.setCurrentIndexFlowsFinish();

                // set next group flows
                if (!taskFlows.hasFlowWithCurrentIndex())
                {
                    if (!setNextExecFlows(ctx, taskFlows, npu_id))
                    {
                        if (taskFlows.areAllTasksDone())
                        {
                            #if SYS_LOG
                            if (SYS_LOG_TARGET_NODE == npu_id.value)
                            {
                                LOG_NODE_COMPLETE(node_id.value);
                            }
                            #endif

                            ctx.get<ProcessingCommTasks>(ctx.data().npu_entities[npu_id.value]).setFinish(node_id.value, getCurrentTime(ctx),npu_id.value);

                            ctx.destroyEntity(ctx.data().npus_chakra_exec_entity[npu_id.value][node_id.value]);
                        }
                    }
                }
            }
        }
        else
        {
            if (!taskFlows.is_exec)
            {
                taskFlows.is_exec = true;
                // first
                setNextExecFlows(ctx, taskFlows, npu_id);
            }
            else
            {
                // SysFlow flows_finish[MAX_FLOW_NUM_PER_COMM_NODE];
                uint32_t flow_finish_count = checkFlowFinish(ctx, npu_id.value, npu_id.flows_finish);

                if (flow_finish_count > 0)
                {
                    taskFlows.updateFlows(npu_id.flows_finish, flow_finish_count);

                    // set next group flows
                    if (!taskFlows.hasFlowWithCurrentIndex  ())
                    {
                        if (!setNextExecFlows(ctx, taskFlows, npu_id))
                        {
                            if (taskFlows.areAllTasksDone())
                            {
                                #if SYS_LOG
                                if (SYS_LOG_TARGET_NODE == npu_id.value)
                                {
                                    LOG_NODE_COMPLETE(node_id.value);
                                }
                                #endif

                                ctx.get<ProcessingCommTasks>(ctx.data().npu_entities[npu_id.value]).setFinish(node_id.value, getCurrentTime(ctx),npu_id.value);

                                ctx.destroyEntity(ctx.data().npus_chakra_exec_entity[npu_id.value][node_id.value]);
                            }
                        }
                    }
                    npu_id.flows_finish_init();
                }
            }
        }
    }

    void sys_checkRecvFlow(Engine &ctx, NpuID &npu_id, NodeID &node_id, RecvNodeFlag &recvNodeFlag)
    {
        #if SYS_LOG
        if (SYS_LOG_TARGET_NODE == npu_id.value)
        {
            LOG_SYS_HEADER("3", "Flow Status Check recv flow System");
            LOG_TASK_START("Checking network flow completion status");
        }
        #endif

        if (DEV_MODE == 0)
        {
            if(ctx.data().send_recv_map_recvend[recvNodeFlag.comm_src][recvNodeFlag.comm_dst]==2)
            {
                #if SIMPLE_LOG_MODE
                if (SYS_LOG_TARGET_NODE == recvNodeFlag.comm_dst) {
                   
                    printf("npus_chakra_exec_entity not none! npu_id[%d], node_id[%d]\n", npu_id.value, node_id.value);
                    printf("send_recv_map_recvend set 0, src(%lu),dst(%lu)\n", recvNodeFlag.comm_src, recvNodeFlag.comm_dst);
                }
                #endif
                ctx.data().send_recv_map_recvend[recvNodeFlag.comm_src][recvNodeFlag.comm_dst]=0;
                ctx.get<ProcessingCommTasks>(ctx.data().npu_entities[npu_id.value]).setFinish(node_id.value, getCurrentTime(ctx),npu_id.value);
                ctx.destroyEntity(ctx.data().npus_chakra_exec_entity[npu_id.value][node_id.value]);
            }

        }
        else
        {
            // SysFlow flows_finish[MAX_FLOW_NUM_PER_COMM_NODE];
            uint32_t flow_finish_count = checkFlowFinish(ctx, recvNodeFlag.comm_src, npu_id.recv_node_flows_finish);
            if (flow_finish_count > 0)
            {
                for (size_t i = 0; i < flow_finish_count; i++)
                {
                    if(npu_id.recv_node_flows_finish[i].comm_src == recvNodeFlag.comm_src && npu_id.recv_node_flows_finish[i].comm_dst == recvNodeFlag.comm_dst)
                    {
                        #if SIMPLE_LOG_MODE
                        if (SYS_LOG_TARGET_NODE == recvNodeFlag.comm_dst) {
                           
                            printf("npus_chakra_exec_entity not none! npu_id[%d], node_id[%d]\n", npu_id.value, node_id.value);
                            printf("send_recv_map_recvend set 0, src(%lu),dst(%lu)\n", recvNodeFlag.comm_src, recvNodeFlag.comm_dst);
                        }
                        #endif
                        ctx.data().send_recv_map_recvend[recvNodeFlag.comm_src][recvNodeFlag.comm_dst]=0;
                        ctx.get<ProcessingCommTasks>(ctx.data().npu_entities[npu_id.value]).setFinish(node_id.value, getCurrentTime(ctx),npu_id.value);
                        ctx.destroyEntity(ctx.data().npus_chakra_exec_entity[npu_id.value][node_id.value]);
                        break;
                    }
                }
                npu_id.recv_node_flows_finish_init();
            }
        }
    }

}