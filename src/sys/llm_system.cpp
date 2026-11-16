#include <madrona/mw_gpu_entry.hpp>

#include "../sim.hpp"

#include "../level_gen.hpp"

#include "../types.hpp"

#include <algorithm>

#include "../collective/topo_logic.hpp"

#include "llm_system.hpp"

#include "net_sys_interface.hpp"

#include "system_init.hpp"

#include "chakra_node_processing.hpp"

#include "communication_system.hpp"

#include "time_management.hpp"

#include "../types/global_types.hpp"

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

#define PRINT_PKT_LOG 0

namespace RenderingSystem = madrona::render::RenderingSystem;

using net_sys_interface::addSimtime;
using net_sys_interface::checkFlowFinish;
using net_sys_interface::getCurrentTime;
using net_sys_interface::isExistedFlow;
using net_sys_interface::setFlow;

// Log formatting macros
#define LOG_SEPARATOR "==============================================================================="
#define LOG_SUBSEPARATOR "-------------------------------------------------------------------------------"

#if true
#define LOG_SYS_HEADER(sys_name, sys_desc) \
    printf("\n[SYS-%s] %s\n%s\n", sys_name, sys_desc, LOG_SUBSEPARATOR);
#define LOG_TASK_START(task_name) \
    printf("  > %s\n", task_name);
#define LOG_TASK_SUCCESS(task_name) \
    printf("  [SUCCESS] %s\n", task_name);
#define LOG_TASK_FAIL(task_name) \
    printf("  [FAILED] %s\n", task_name);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define LOG_INFO(msg, ...) \
    printf("  INFO: " msg "\n", ##__VA_ARGS__);
#pragma GCC diagnostic pop
#define LOG_WARNING(msg, ...) \
    printf("  WARNING: " msg "\n", ##__VA_ARGS__);
#define LOG_ERROR(msg, ...) \
    printf("  ERROR: " msg "\n", ##__VA_ARGS__);
#define LOG_NODE_PROCESS(npu_id, node_id, node_type) \
    printf("  [NODE] Processing NPU [%d] node [%d] type: %s\n", npu_id, node_id, node_type);
#define LOG_FLOW_CREATE(flow_id, src, dst, size) \
    printf("    Created flow [ID:%u] %lu->%lu size:%lu\n", flow_id, src, dst, size);
#define LOG_NODE_FLOW_CREATE(flow_id, src, dst, size, exec_index) \
    printf("      Created flow [ID:%u] %lu->%lu size:%lu exec_index:%d\n", flow_id, src, dst, size, exec_index);
#define LOG_NODE_RECV_FLOW_CREATE(src, dst) \
    printf("      Recv flow  %lu->%lu \n", src, dst);

#define LOG_NODE_INFO(msg, ...) \
    printf("      INFO: " msg "\n", ##__VA_ARGS__);
#define LOG_NODE_COMPLETE(node_id) \
    printf("    Node [%d] completed\n", node_id);
#define LOG_NODE_RELEASE(node_id,process_time) \
    printf("    Released node [%d] process time: %ld ns\n", node_id, process_time);
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
#define LOG_TASK_FAIL(task_name) \
    do                           \
    {                            \
    } while (0)
#define LOG_INFO(msg, ...) \
    do                     \
    {                      \
    } while (0)
#define LOG_WARNING(msg, ...) \
    do                        \
    {                         \
    } while (0)
#define LOG_ERROR(msg, ...) \
    do                      \
    {                       \
    } while (0)
#define LOG_NODE_PROCESS(npu_id, node_id, node_type) \
    do                                               \
    {                                                \
    } while (0)
#define LOG_FLOW_CREATE(flow_id, src, dst, size) \
    do                                           \
    {                                            \
    } while (0)
#define LOG_NODE_FLOW_CREATE(flow_id, src, dst, size, exec_index) \
    do                                                            \
    {                                                             \
    } while (0)
#define LOG_NODE_RECV_FLOW_CREATE(src, dst) \
    do                                                            \
    {                                                             \
    } while (0)
#define LOG_NODE_INFO(msg, ...) \
    do                          \
    {                           \
    } while (0)
#define LOG_NODE_COMPLETE(node_id) \
    do                             \
    {                              \
    } while (0)
#define LOG_NODE_RELEASE(node_id,process_time) \
    do                            \
    {                             \
    } while (0)
#endif

namespace madEscape::llm_system {
    // Forward declarations from sub-modules are now included via headers
    // Other system-related implementations

    // -----------------------------------------------------------------------------------

    // Node processing functions moved to chakra_node_processing.cpp

    // Time management functions moved to time_management.cpp

    // System initialization functions moved to system_init.cpp

    // Communication functions moved to communication_system.cpp

    void sys_processChakraNodes(Engine & ctx,
        NpuID & id,
        ChakraNodes & chakraNodes,
        HardwareResource & hardwareResource,
        ProcessingCompTask & processingCompTask,
        ProcessingCommTasks & processingCommTasks,
        OneNPUFinishedFlag & oneNPUFinishedFlag,
        ChakraNodesForNoDP & chakraNodesForNoDP) {

        #if SYS_LOG
        if (SYS_LOG_TARGET_NODE == id.value) {
            LOG_SYS_HEADER("1", "Chakra Node Processing System");
            LOG_TASK_START("Scanning no-dependency nodes and assigning tasks");
        }
        #endif

        // append nodes
        if (hardwareResource.one_task_finish) {
            // ChakraNode current_exec_nodes[CURRENT_EXEC_NODES_MAX];
            bool is_none_node = true;
            int node_count = filterNoDependencyNodes(chakraNodes, chakraNodesForNoDP.current_exec_nodes, is_none_node);
            
            #if SIMPLE_LOG_MODE
                // if (id.value >=392 && id.value <= 417) {
                if (getCurrentTime(ctx) % (100*1000) == 0) {
                // if (id.value == 706 || id.value == 714 || id.value == 722) {
                    LOG_INFO("Simulation time: %ld, npu id [%d] found %d no-dependency nodes", getCurrentTime(ctx), id.value, node_count);
                    if(node_count>0) {
                        for(int i=0; i<node_count; i++) {
                            // LOG_INFO("no-dependency node %d", current_exec_nodes[i].id);
                            printf("Simulation time: %ld, npu id [%d]: no-dependency node %d\n", getCurrentTime(ctx), id.value, chakraNodesForNoDP.current_exec_nodes[i].id);
                        }
                    }
                    if (is_none_node) {
                        LOG_INFO("npu id [%d], the operators of all chakra nodes are finished at time: %ld!", id.value, getCurrentTime(ctx)); 
                    }
                }
                // }
            #endif

            if (is_none_node) {
                oneNPUFinishedFlag.is_finished = true;
            }
            // process no np nodes
            for (size_t i = 0; i < static_cast < size_t > (node_count); i++) {

                
                ChakraNode &node = chakraNodesForNoDP.current_exec_nodes[i];

                // Skip if already being processed to avoid duplicate lookups
                if (processingCommTasks.containsNodeId(node.id)) {
                    #if SYS_LOG
                        if (SYS_LOG_TARGET_NODE == id.value) {
                            LOG_INFO("npu[%d] processingCommTasks containsNodeId: %d", id.value, node.id);
                        }
                    #endif
                    continue;
                }
                #if SYS_LOG
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        printf("#######process no-dependency node: %d, node_type: %d\n", node.id, node.type);
                    }
                #endif

                #if TEST_RING_TOPO_LOG_FOR_CPU_ONLY
                if (id.test_dim) {
                    printf("[TEST DIM] process no-dependency node: %d, node_type: %d\n", node.id, node.type);
                }
                #endif

                #if TEST_CHAKRA_NODE_LOG_FOR_CPU_ONLY
                if (SYS_LOG_TARGET_NODE == id.value) {
                    printf("[TEST CHAKRA NODE] process no-dependency node: %d, node_type: %d\n", node.id, node.type);
                }
                #endif
                
            
                switch (node.type) {
                    case NodeType::COMP_NODE: {
                        if (!hardwareResource.comp_ocupy) {

                            // processingCompTask.time_finish_ns = getCurrentTime(ctx) + usToNs(node.durationMicros);
                            uint64_t dtns=usToNs(node.durationMicros);
                            #if SYS_LOG_ENABLED_LIMIT_COMM_SIZE_DURATION
                            // if (dtns > 30000) {
                            //     dtns = 30000;
                            // }
                            dtns=dtns/SYS_LOG_LIMIT_COMM_SIZE;
                            // if (usToNs(node.durationMicros) > 10000*1000) {
                            //     printf("big computation node: node.id: %d, original dtns: %ld ns, limited dtns: %ld ns\n", node.id, usToNs(node.durationMicros), dtns);
                            // }

                            // dtns = 3000;

                            #endif
                            processingCompTask.time_finish_ns = getCurrentTime(ctx) + dtns;
                            #if SYS_LOG
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                LOG_NODE_PROCESS(id.value, node.id, "Computation Node");
                                node.printName();
                                LOG_NODE_INFO("Computation task estimated completion time: %ld ns", processingCompTask.time_finish_ns);
                            }
                            #endif
                            #if SIMPLE_LOG_MODE
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                LOG_NODE_PROCESS(id.value, node.id, "Computation Node");
                                node.printName();
                                LOG_NODE_INFO("Current time: %ld ns, Computation task estimated completion time: %ld ns, dtns: %ld ns", getCurrentTime(ctx), processingCompTask.time_finish_ns, dtns);
                            }
                            #endif
                            processingCompTask.state = TaskState::START;
                            processingCompTask.node_id = node.id;

                            // set flag
                            hardwareResource.comp_ocupy = true;

                            // set skip time - add time.
                            skipTime_add_time(ctx, processingCompTask.time_finish_ns);
                        } else {
                            #if SYS_LOG
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                LOG_WARNING("Computation hardware resource occupied, skipping node %d", node.id);
                            }
                            #endif
                        }
                        break;
                    }
                    case NodeType::COMM_SEND_NODE: {
                        // uint64_t src = node.comm_src;
                        // to fix chakra bugs.
                        uint64_t src = id.value;
                        uint64_t dst = node.comm_dst;

                        int npu_num = ctx.get < RingParams > (ctx.data().ring_config_entity).npu_num;

                        if (npu_num > 0) {
                            if (src >= npu_num) {
                                src = 0;
                                printf("[change] src from %lu to %lu.\n", node.comm_src, src);
                            }
                            if (dst >= npu_num) {
                                dst = 15;
                                printf("[change] dst from %lu to %lu.\n", node.comm_dst, dst);
                            }
                            if(src == dst) {
                                printf("[change 2] send npu_id [%u] npu_num =%lu, src =%lu, dst =%lu, node.comm_src =%lu, node.comm_dst =%lu.\n", id.value, npu_num, src, dst, node.comm_src, node.comm_dst);
                            }
                        }

                        if(ctx.data().send_recv_map_recvend[src][dst]==1) 
                        {

                            ctx.data().send_recv_map_recvend[src][dst]=2;
                            #if SYS_LOG
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                printf("send_recv_map_recvend set 2, src(%lu),dst(%lu)\n", src, dst);
                            }
                            #endif
                            ProcessingCommTask &processingCommTask = processingCommTasks.getNextFreeTask();
                            processingCommTask.time_start_ns = getCurrentTime(ctx);
                            #if SYS_LOG
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                LOG_NODE_PROCESS(id.value, node.id, "Communication Send Node");
                                LOG_NODE_INFO("Send: %lu -> %lu", src, dst);
                                node.printName();
                            }
                            #endif
                            #if SIMPLE_LOG_MODE
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                LOG_NODE_PROCESS(id.value, node.id, "Communication Send Node");
                                node.printName();
                                LOG_NODE_INFO("Send: %lu -> %lu", src, dst);
                                
                            }
                            #endif

                            #if TEST_ALL_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                            if(SYS_LOG_TARGET_NODE==id.value)
                            {
                                printf("[TEST_ALL_COLLECTIVE]: NPU[%u],chakraNode[%u],comm_size: %lu, comm_type: %d\n", id.value, node.id, node.comm_size, node.comm_type);
                            }
                            #endif
                            
                            processingCommTask.state = TaskState::START;
                            processingCommTask.node_id = node.id;
                            // processingCommTasks.addTask(processingCommTask);

                            
                            // // Create ring configuration entity and set ring parameters
                            // Entity ring_entity = ctx.makeEntity<RingConfigEntity>();
                            // ctx.get<RingParams>(ring_entity).ring_dim0 = static_cast<uint32_t>(processParams.params[0]);
                            // ctx.get<RingParams>(ring_entity).ring_dim1 = static_cast<uint32_t>(processParams.params[1]);
                            // ctx.get<RingParams>(ring_entity).ring_dim2 = static_cast<uint32_t>(processParams.params[2]);
                            // ctx.get<RingParams>(ring_entity).chunks_num = static_cast<uint32_t>(processParams.params[3]);
                            // ctx.get<RingParams>(ring_entity).npu_num = static_cast<uint32_t>(processParams.params[4]);
                            // ctx.data().ring_config_entity = ring_entity;

                            

                            uint64_t comm_size = node.comm_size;
                            #if SYS_LOG_ENABLED_LIMIT_COMM_SIZE_DURATION
                            // if (comm_size > SYS_LOG_LIMIT_COMM_SIZE) {
                            //     comm_size = SYS_LOG_LIMIT_COMM_SIZE;
                            // }
                            comm_size=comm_size/SYS_LOG_LIMIT_COMM_SIZE;
                            #endif

                            // create comm entity
                            Entity process_e = ctx.makeEntity < ProcessComm_E > ();
                            // Manually initialize TaskFlows to avoid temporary object creation
                            TaskFlows &task_flows = ctx.get < TaskFlows > (process_e);
                            task_flows.is_exec = false;
                            task_flows.current_index = -1;
                            // Only initialize flows[0], not all 900 flows
                            // task_flows.flows[0].init();
                            for(size_t i=0; i<MAX_FLOW_NUM_PER_COMM_NODE; i++)
                            {
                                ctx.get < TaskFlows > (process_e).flows[i].init();
                            }
                            ctx.get < NpuID > (process_e).value = id.value;
                            ctx.get < NodeID > (process_e).value = node.id;
                            uint32_t flow_id = processingCommTasks.getFlowId();
                            ctx.get < TaskFlows > (process_e).flows[0].id = flow_id;
                            ctx.get < TaskFlows > (process_e).flows[0].comm_size = comm_size;
                            ctx.get < TaskFlows > (process_e).flows[0].comm_src = src;
                            ctx.get < TaskFlows > (process_e).flows[0].comm_dst = dst;
                            ctx.get < TaskFlows > (process_e).flows[0].state = TaskState::START;
                            ctx.get < TaskFlows > (process_e).flows[0].is_send = true;

                            ctx.data().npus_chakra_exec_entity[id.value][node.id] = process_e;

                            // setFlow(ctx, src, dst, node.comm_size, flow_id);
                            #if SIMPLE_LOG_MODE
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                printf("create process entity:npu_id[%u], node_id[%u]\n", id.value, node.id);
                                LOG_NODE_FLOW_CREATE(flow_id,
                                    // ctx.get < TaskFlows > (process_e).flows[0].comm_src,
                                    src,
                                    // ctx.get < TaskFlows > (process_e).flows[0].comm_dst,
                                    dst,
                                    comm_size,
                                    0);
                            }
                            #endif

                            #if TEST_ALL_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                
                                printf("[FLOW] flow info: NPU[%u],chakraNode[%u], flow_id=%u, comm_src=%lu, comm_dst=%lu, comm_size=%lu\n", id.value, node.id, flow_id, src, dst, comm_size);
                            }
                        #endif
                        }
                        else
                        {
                            #if SIMPLE_LOG_MODE
                            if (SYS_LOG_TARGET_NODE == id.value) {
                                printf("src(%lu),dst(%lu) not found in send_recv_map_recvend\n", src, dst);
                            }
                            #endif
                        }
                        break;
                    }
                    case NodeType::COMM_RECV_NODE: {

                        uint64_t src = node.comm_src;
                        // uint64_t dst = node.comm_dst;
                        // to fix chakra bugs.
                        uint64_t dst = id.value;

                        int npu_num = ctx.get < RingParams > (ctx.data().ring_config_entity).npu_num;

                        if (npu_num > 0) {
                            if (src >= npu_num) {
                                src = 0;
                                printf("[change] src from %lu to %lu.\n", node.comm_src, src);
                            }
                            if (dst >= npu_num) {
                                dst = 15;
                                printf("[change] dst from %lu to %lu.\n", node.comm_dst, dst);
                            }
                            if(src == dst) {
                                printf("[change 2] recv npu_id [%u] npu_num =%lu, src =%lu, dst =%lu, node.comm_src =%lu, node.comm_dst =%lu.\n", id.value, npu_num, src, dst, node.comm_src, node.comm_dst);
                                src = 0;
                                dst = 1;
                            }
                        }
                        #if SIMPLE_LOG_MODE
                            if (SYS_LOG_TARGET_NODE == dst) {
                                printf("in receive node :src =%lu, dst =%lu, id.value =%u,node.id =%u\n", src, dst, id.value, node.id);
                            }
                        #endif
                        ProcessingCommTask &processingCommTask = processingCommTasks.getNextFreeTask();
                        processingCommTask.time_start_ns = getCurrentTime(ctx);
                        #if SYS_LOG
                        if (SYS_LOG_TARGET_NODE == dst) {
                            LOG_NODE_PROCESS(id.value, node.id, "Other Communication Receive Node");
                            node.printName();
                            LOG_NODE_INFO("Receive: %lu -> %lu", src, dst);
                            
                        }
                        #endif
                        #if SIMPLE_LOG_MODE
                        if (SYS_LOG_TARGET_NODE == dst) {
                            LOG_NODE_PROCESS(id.value, node.id, "Other Communication Receive Node");
                            node.printName();
                            LOG_NODE_INFO("Receive: %lu -> %lu", src, dst);
                        }
                        #endif
                        processingCommTask.state = TaskState::START;
                        processingCommTask.node_id = node.id;
                        // processingCommTasks.addTask(processingCommTask);

                        if(ctx.data().send_recv_map_recvend[src][dst]!=0)
                        {
                            printf("!!!!!mistake in receive node :src =%lu, dst =%lu, npu =%u,chakraNode.id =%u,value=%d\n", src, dst, id.value, node.id,ctx.data().send_recv_map_recvend[src][dst]);
                            break;
                        }  
                        if(ctx.data().send_recv_map_recvend[src][dst]==0)
                        {
                            ctx.data().send_recv_map_recvend[src][dst]=1;
                        }
                        
                        #if SIMPLE_LOG_MODE
                            if (SYS_LOG_TARGET_NODE == dst) {
                                printf("send_recv_map_recvend set 1, src(%lu),dst(%lu)\n", src, dst);
                                
                            }
                        #endif


                        // create comm entity
                        Entity process_e = ctx.makeEntity < ProcessRecvComm_E > ();
                        ctx.get < NpuID > (process_e).value = id.value;
                        ctx.get < NodeID > (process_e).value = node.id;
                        ctx.get < RecvNodeFlag > (process_e).comm_src = src;
                        ctx.get < RecvNodeFlag > (process_e).comm_dst = dst;
                        ctx.data().npus_chakra_exec_entity[id.value][node.id] = process_e;
                        // setFlow(ctx, src, dst, node.comm_size, flow_id);
                        #if SIMPLE_LOG_MODE
                        if (SYS_LOG_TARGET_NODE == dst) {
                            LOG_NODE_RECV_FLOW_CREATE(src,dst);
                        }
                        #endif

                        break;
                    }
                    case NodeType::COMM_COLL_NODE: {

                        // // Create ring configuration entity and set ring parameters
                        // Entity ring_entity = ctx.makeEntity<RingConfigEntity>();
                        // ctx.get<RingParams>(ring_entity).ring_dim0 = static_cast<uint32_t>(processParams.params[0]);
                        // ctx.get<RingParams>(ring_entity).ring_dim1 = static_cast<uint32_t>(processParams.params[1]);
                        // ctx.get<RingParams>(ring_entity).ring_dim2 = static_cast<uint32_t>(processParams.params[2]);
                        // ctx.get<RingParams>(ring_entity).chunks_num = static_cast<uint32_t>(processParams.params[3]);

                        // ctx.data().ring_config_entity = ring_entity;

                        // NPUID*10000+FLOWID
                        #if SYS_LOG
                        if (SYS_LOG_TARGET_NODE == id.value) {
                            LOG_NODE_PROCESS(id.value, node.id, "Collective Communication Node");
                            node.printName();
                            LOG_NODE_INFO("Communication type: %s (%d)", CollectiveCommTypeToString(node.comm_type), (int) node.comm_type);
                            LOG_NODE_INFO("Creating collective communication entity");
                        }
                        #endif

                        #if SIMPLE_LOG_MODE
                        if (SYS_LOG_TARGET_NODE == id.value) {
                            LOG_NODE_PROCESS(id.value, node.id, "Collective Communication Node");
                            node.printName();
                            LOG_NODE_INFO("Communication type: %s (%d)", CollectiveCommTypeToString(node.comm_type), (int) node.comm_type);
                            LOG_NODE_INFO("Creating collective communication entity");
                        }
                        #endif


                        #if TEST_ALL_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                        if(SYS_LOG_TARGET_NODE==id.value)
                        {
                            printf("[TEST_ALL_COLLECTIVE]: npu_id[%u], node_id[%u],comm_size: %lu, comm_type: %d\n", id.value, node.id, node.comm_size, node.comm_type);
                        }
                        #endif

                        uint64_t comm_size = node.comm_size;
                        #if SYS_LOG_ENABLED_LIMIT_COMM_SIZE_DURATION
                        // if (comm_size > SYS_LOG_LIMIT_COMM_SIZE) {
                        //     comm_size = SYS_LOG_LIMIT_COMM_SIZE;
                        // }
                        comm_size=comm_size/SYS_LOG_LIMIT_COMM_SIZE;
                        #endif
                        if (ctx.data().npus_chakra_exec_entity[id.value][node.id] != Entity::none()) {
                            // printf("ProcessComm_E entity already exists for npu %lu node %d, reusing\n", 
                            //        id.value, node.id);
                            ctx.destroyEntity(ctx.data().npus_chakra_exec_entity[id.value][node.id]);
                        }
                        // create comm entity
                        Entity process_e = ctx.makeEntity < ProcessComm_E > ();
                        // Manually initialize TaskFlows to avoid temporary object creation
                        TaskFlows &task_flows = ctx.get < TaskFlows > (process_e);
                        task_flows.is_exec = false;
                        task_flows.current_index = -1;
                        for(size_t i=0; i<MAX_FLOW_NUM_PER_COMM_NODE; i++)
                        {
                            ctx.get < TaskFlows > (process_e).flows[i].init();
                        }
                        ctx.get < NpuID > (process_e).value = id.value;
                        ctx.get < NodeID > (process_e).value = node.id;
                        ctx.data().npus_chakra_exec_entity[id.value][node.id] = process_e;
                        int flow_exec_index = 0;
                        int flow_current_count = 0;

                        // enum CollectiveCommType : uint64_t
                        // {
                        //     ALL_REDUCE = 0,
                        //     REDUCE = 1,
                        //     ALL_GATHER = 2,
                        //     GATHER = 3,
                        //     SCATTER = 4,
                        //     BROADCAST = 5,
                        //     ALL_TO_ALL = 6,
                        //     REDUCE_SCATTER = 7,
                        //     REDUCE_SCATTER_BLOCK = 8,
                        //     BARRIER = 9
                        // };

                        CommImplementationType comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).all_reduce_implementation;
                        switch (node.comm_type) {
                            case CollectiveCommType::ALL_REDUCE:
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).all_reduce_implementation;
                                break;
                            case CollectiveCommType::REDUCE:
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).reduce_implementation;
                                break;
                            case CollectiveCommType::ALL_GATHER:
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).all_gather_implementation;
                                break;
                            case CollectiveCommType::GATHER:
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).gather_implementation;
                                break;
                            case CollectiveCommType::SCATTER:
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).scatter_implementation;
                                break;
                            case CollectiveCommType::BROADCAST:
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).broadcast_implementation;
                                break;
                            default: {
                                comm_implementation_type = ctx.get < CommModel > (ctx.data().sys_config_entity).all_reduce_implementation;
                                break;
                            }
                        }

                        switch (comm_implementation_type) {
                            case CommImplementationType::Ring:
                                if (!id.test_collective_comm_ring) {
                                    #if TEST_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                                        if(SYS_LOG_TARGET_NODE==id.value)
                                        {
                                            printf("[TEST_RING_COLLECTIVE]: npu_id[%u], node_id[%u],comm_size: %lu, comm_implementation_type: %d, comm_type: %d, dims: [%d, %d, %d], involved_dim_1: %d, involved_dim_2: %d, involved_dim_3: %d\n", id.value, node.id, comm_size, comm_implementation_type, node.comm_type, ctx.get < RingParams > (ctx.data().ring_config_entity).ring_dim0, ctx.get < RingParams > (ctx.data().ring_config_entity).ring_dim1, ctx.get < RingParams > (ctx.data().ring_config_entity).ring_dim2, node.involved_dim_1, node.involved_dim_2, node.involved_dim_3);
                                            printf("[TEST_RING_COLLECTIVE] flow start:\n");
                                        }
                                    #endif
                                }

                                if (ctx.data().ring_config_entity == Entity::none()) {
                                    printf("ERROR: ring_config_entity not initialized when processing ALL_GATHER node\n");
                                    return;
                                }
                                // ring params
                                uint32_t dims[] = {
                                    ctx.get < RingParams > (ctx.data().ring_config_entity).ring_dim0,
                                    ctx.get < RingParams > (ctx.data().ring_config_entity).ring_dim1,
                                    ctx.get < RingParams > (ctx.data().ring_config_entity).ring_dim2
                                };
                                uint32_t chunks_num = ctx.get < RingParams > (ctx.data().ring_config_entity).chunks_num;
                                uint64_t chunk_size = comm_size / chunks_num;
                                uint64_t remain_size = comm_size % chunks_num;
                                if (remain_size > 0) {
                                    chunks_num++;
                                }
                                #if SYS_LOG
                                if (SYS_LOG_TARGET_NODE == id.value) {
                                    LOG_NODE_INFO("Topology dimensions: [%d, %d, %d]", dims[0], dims[1], dims[2]);
                                    LOG_NODE_INFO("Chunk num: %d", chunks_num);
                                    LOG_NODE_INFO("Data chunking: chunks=%d, chunk_size=%lu, remain_size=%lu", chunks_num, chunk_size, remain_size);
                                }
                                #endif
                                // topo info for temp 3d
                                // demotion excute info for temp
                                bool dim_1d_enable = node.involved_dim_1;
                                bool dim_2d_enable = node.involved_dim_2;
                                bool dim_3d_enable = node.involved_dim_3;
                                #if SYS_LOG
                                if (SYS_LOG_TARGET_NODE == id.value) {
                                    LOG_NODE_INFO("dim_1d_enable=%d", dim_1d_enable);
                                    LOG_NODE_INFO("dim_2d_enable=%d", dim_2d_enable);
                                    LOG_NODE_INFO("dim_3d_enable=%d", dim_3d_enable);
                                }
                                #endif
                                for (size_t i = 0; i < static_cast < size_t > (chunks_num); i++) {
                                    #if SYS_LOG
                                    if (SYS_LOG_TARGET_NODE == id.value) {
                                        LOG_NODE_INFO("Processing data chunk %zu/%d", i + 1, chunks_num);
                                    }
                                    #endif
                                    uint64_t data_size_current = chunk_size;
                                    if (remain_size > 0 && i == static_cast < size_t > (chunks_num) - 1) {
                                        data_size_current = remain_size;
                                    }
                                    #if SYS_LOG
                                    if (SYS_LOG_TARGET_NODE == id.value) {
                                        LOG_NODE_INFO("Current chunk size: %lu bytes", data_size_current);
                                    }
                                    #endif
                                    

                                    // create comm task
                                    ProcessingCommTask &processingCommTask = processingCommTasks.getNextFreeTask();
                                    processingCommTask.time_start_ns = getCurrentTime(ctx);
                                    processingCommTask.state = TaskState::START;
                                    processingCommTask.node_id = node.id;
                                    // processingCommTasks.addTask(processingCommTask);

                                    

                                    int dim_current = 0;
                                    int offset = 1;
                                    while (dim_current < 3) {
                                        bool excute = false;
                                        int total_nodes_in_ring = dims[0];
                                        Dimension dimension = Dimension::Local;

                                        if (dim_current == 0 && dim_1d_enable) {
                                            excute = true;
                                        }
                                        if (dim_current == 1 && dim_2d_enable) {
                                            total_nodes_in_ring = dims[1];
                                            dimension = Dimension::Horizontal;
                                            excute = true;
                                        }
                                        if (dim_current == 2 && dim_3d_enable) {
                                            total_nodes_in_ring = dims[2];
                                            dimension = Dimension::Vertical;
                                            excute = true;
                                        }

                                        if (excute) {
                                            #if SYS_LOG
                                            if (SYS_LOG_TARGET_NODE == id.value) {
                                                LOG_NODE_INFO("Dimension %d: %s, ring_nodes=%d",
                                                    dim_current,
                                                    DimensionToString(dimension),
                                                    total_nodes_in_ring);
                                            }
                                            #endif
                                        }

                                        if (excute) {
                                            if (true) {
                                                int node_id = id.value;

                                                // Access global ring topology entity for current dimension
                                                RingTopology& ring_topo = ctx.get<RingTopology>(ctx.data().ring_topo_entity[dim_current]);

                                                #if SYS_LOG
                                                if (SYS_LOG_TARGET_NODE == id.value) {
                                                    int index = ring_topo.get_index_from_node_id(node_id);
                                                    LOG_NODE_INFO("Ring topology: node=%d, topo_index=%d, offset=%d",
                                                        node_id, index, ring_topo.offset);
                                                }
                                                #endif

                                                Direction dir = get_comm_Ring_Direction();
                                                int flow_count = get_ring_comm_count_per_phase(node.comm_type, comm_implementation_type, total_nodes_in_ring);
                                                uint64_t msg_size = 0;
                                                uint64_t final_data_size = 0;
                                                get_ring_comm_size_per_flow(node.comm_type, comm_implementation_type, data_size_current, total_nodes_in_ring, msg_size, final_data_size);

                                                #if SYS_LOG
                                                if (SYS_LOG_TARGET_NODE == id.value) {
                                                    LOG_NODE_INFO("Flow config: flow_count=%d, msg_size=%lu, final_data=%lu",
                                                        flow_count, msg_size, final_data_size);
                                                }
                                                #endif
  
                                                for (size_t j = 0; j < static_cast < size_t > (flow_count); j++) {
                                                    uint32_t flow_id = processingCommTasks.getFlowId();
                                                    // ctx.get < TaskFlows > (process_e).flows[flow_current_count].init();
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].id = flow_id;
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_size = msg_size;
                                                    // todo：only send flow，recv flow will be checked by sender node.
                                                    // ctx.get<TaskFlows>(process_e).flows[flow_current_count].comm_src = ring_topo.get_sender(node_id, dir);
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_src = node_id;
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_dst = ring_topo.get_receiver(node_id, dir);
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].state = TaskState::START;
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].is_send = true;
                                                    ctx.get < TaskFlows > (process_e).flows[flow_current_count].exec_index = flow_exec_index;

                                                    #if SYS_LOG
                                                    if (SYS_LOG_TARGET_NODE == id.value) {
                                                        // Print flow-related information
                                                        LOG_NODE_FLOW_CREATE(flow_id,
                                                            ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_src,
                                                            ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_dst,
                                                            msg_size,
                                                            flow_exec_index);
                                                    }
                                                    #endif

                                                    if (!id.test_collective_comm_ring) {
                                                        #if TEST_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                                                            if (SYS_LOG_TARGET_NODE == id.value) {
                                                                printf("[TEST_RING_COLLECTIVE] flow info: flow_id=%u, comm_src=%u, comm_dst=%u, msg_size=%lu, exec_index=%d\n", flow_id, ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_src, ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_dst, msg_size, flow_exec_index);
                                                            }
                                                        #endif
                                                    }

                                                    #if TEST_ALL_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                                                        if (SYS_LOG_TARGET_NODE == id.value) {
                                                            printf("[FLOW] flow info: NPU[%u],chakraNode[%u], flow_id=%u, comm_src=%lu, comm_dst=%lu, msg_size=%lu, exec_index=%d\n", id.value, node.id, flow_id, ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_src, ctx.get < TaskFlows > (process_e).flows[flow_current_count].comm_dst, msg_size, flow_exec_index);
                                                        }
                                                    #endif

                                                    flow_current_count++;
                                                    flow_exec_index++;
                                                    
                                                    
                                                }
                                            }
                                        }
                                        offset *= dims[dim_current];
                                        dim_current++;
                                    }
                                }

                                if(!id.test_collective_comm_ring) {
                                    
                                    #if TEST_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY
                                    if (SYS_LOG_TARGET_NODE == id.value) {
                                        printf("[TEST_RING_COLLECTIVE] flow end.\n");
                                    }
                                    #endif
                                   
                                    id.test_collective_comm_ring = true;
                                }
                                break;
                        }
                        // setFlow(Engine &ctx, uint64_t comm_src, uint64_t comm_dst, uint64_t comm_size, uint32_t flow_id)
                        // setFlow(ctx, 0, 1, 1000, flow_id);
                        break;
                    }
                    default:
                        break;
                }
            }

            // set flag
            hardwareResource.one_task_finish = false;
        }
    }

    // sys_checkFlow moved to communication_system.cpp

    // sys_checkSkipTime moved to time_management.cpp

    void sys_removeChakraNodes(Engine & ctx,
        NpuID & id,
        ChakraNodes & chakraNodes,
        HardwareResource & hardwareResource,
        ProcessingCompTask & processingCompTask,
        ProcessingCommTasks & processingCommTasks) {
        #if SYS_LOG
        if (SYS_LOG_TARGET_NODE == id.value) {
            LOG_SYS_HEADER("5", "Node Removal and Resource Recycling System");
            LOG_TASK_START("Checking completed tasks and recycling resources");
            LOG_INFO("Checking computation task status");
        }
        #endif
        // process comp
        if (processingCompTask.state == TaskState::START ) {

            if(DEV_MODE == 0)
            {
                removeNode(chakraNodes, processingCompTask.node_id);
                #if TEST_CHAKRA_NODE_LOG_FOR_CPU_ONLY
                if (SYS_LOG_TARGET_NODE == id.value) {
                    printf("[TEST CHAKRA NODE] remove node: %d\n", processingCompTask.node_id);
                }
                #endif
                #if SYS_LOG
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        LOG_NODE_RELEASE(processingCompTask.node_id, processingCompTask.time_finish_ns);
                    }
                    #endif
                #if SIMPLE_LOG_MODE
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        LOG_NODE_RELEASE(processingCompTask.node_id, processingCompTask.time_finish_ns);
                    }
                #endif
                // reset flag.
                hardwareResource.one_task_finish = true;
                processingCompTask.state = TaskState::INIT;
                hardwareResource.comp_ocupy = false;
            }
            else if(static_cast < uint64_t > (processingCompTask.time_finish_ns) <= getCurrentTime(ctx))
            {
                removeNode(chakraNodes, processingCompTask.node_id);
                #if TEST_CHAKRA_NODE_LOG_FOR_CPU_ONLY
                if (SYS_LOG_TARGET_NODE == id.value) {
                    printf("[TEST CHAKRA NODE] remove node: %d\n", processingCompTask.node_id);
                }
                #endif
                #if SYS_LOG
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        LOG_NODE_RELEASE(processingCompTask.node_id, processingCompTask.time_finish_ns);
                    }
                    #endif
                #if SIMPLE_LOG_MODE
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        LOG_NODE_RELEASE(processingCompTask.node_id, processingCompTask.time_finish_ns);
                    }
                #endif
                // reset flag.
                hardwareResource.one_task_finish = true;
                processingCompTask.state = TaskState::INIT;
                hardwareResource.comp_ocupy = false;
            }


        }
        #if SYS_LOG
        if (SYS_LOG_TARGET_NODE == id.value) {
            LOG_INFO("Checking communication task status");
        }
        #endif
        // process comm
        if (processingCommTasks.has_task()) {
            // if (SYS_LOG && id.value == 0)
            // {
            //     printf("node id %d -> processingCommTasks.has_task() .\n", id.value);
            // }

            int64_t t = getCurrentTime(ctx);
            // ProcessingCommTask result[MAX_COMM_TASK_PER_NPU];
            int task_count = processingCommTasks.dequeueTasksByTime(t, id.result, MAX_COMM_TASK_PER_NPU);

            if (task_count > 0) {
                #if SYS_LOG
                if (SYS_LOG_TARGET_NODE == id.value) {
                    LOG_INFO("NPU [%u] completed %d communication tasks", id.value, task_count);
                }
                #endif
            }

            if (task_count > 0) {
                for (size_t i = 0; i < static_cast < size_t > (task_count); i++) {
                    // if (SYS_LOG && id.value == 0)
                    // {
                    //     printf("processingCompTask over.\n");
                    // }
                    // release node.

                    removeNode(chakraNodes, id.result[i].node_id);
                    #if TEST_CHAKRA_NODE_LOG_FOR_CPU_ONLY
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        printf("[TEST CHAKRA NODE] remove node: %d\n", processingCompTask.node_id);
                    }
                    #endif
                    #if SYS_LOG
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        LOG_NODE_RELEASE(id.result[i].node_id, id.result[i].time_finish_ns);
                    }
                    #endif
                    #if SIMPLE_LOG_MODE
                    if (SYS_LOG_TARGET_NODE == id.value) {
                        LOG_NODE_RELEASE(id.result[i].node_id, id.result[i].time_finish_ns);
                    }
                    #endif
                }
                id.result_init();
            }

            // reset flag.
            hardwareResource.one_task_finish = true;
        }
    };

    void sys_checkNpuFinish(Engine &ctx, CheckNpuFinishFlag &checkNpuFinishFlag) {
        // printf("xx1\n");
        checkNpuFinishFlag.counter++;
        // printf("xx2\n");
        #if SYS_LOG
            if(checkNpuFinishFlag.counter%100==0)
            {
                LOG_SYS_HEADER("6", "checkNpuFinish System");
            }
        #endif

        //printf("ctx.data().ring_config_entity!=Entity::none()= %d\n", ctx.data().ring_config_entity!=Entity::none());

        if(ctx.data().ring_config_entity!=Entity::none())
        {
                
            
            // int npu_num=128;
            int npu_num = ctx.get<RingParams>(ctx.data().ring_config_entity).npu_num;
            bool is_finish=true;
            for (int i = 0; i < npu_num; i++) {
                if (!ctx.get<OneNPUFinishedFlag>(ctx.data().npu_entities[i]).is_finished) {
                    is_finish=false;
                    break;
                }
            }
            if(is_finish)
            {
                ctx.get<ProcessParams>(ctx.data().init_entity).params[999]=1;
                printf("all npu tasks done!\n");
            }
        }
        
    };
    


    void sys_test_ring_topo_dim(Engine & ctx,
        NpuID & id,
        ChakraNodes & chakraNodes,
        HardwareResource & hardwareResource,
        ProcessingCompTask & processingCompTask,
        ProcessingCommTasks & processingCommTasks,
        OneNPUFinishedFlag & oneNPUFinishedFlag,
        ChakraNodesForNoDP & chakraNodesForNoDP){
        
        if(!id.test_dim) {
            for(int i=0; i<3; i++) {
                RingTopology& ring_topo = ctx.get<RingTopology>(ctx.data().ring_topo_entity[i]);
                ring_topo.print_ring_nodes(id.value);
            }
        }
        else
        {
            id.test_dim=true;
        }

    }
}
