#include "system_init.hpp"
#include "chakra_node_processing.hpp"
#include "../collective/topo_logic.hpp"
#include "../types/global_types.hpp"

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

namespace madEscape {
    // Definition of the global flows_id_lock variable
    madrona::SpinLock flows_id_lock;
}

// Log formatting macros
#define LOG_SUBSEPARATOR "-------------------------------------------------------------------------------"

#if SYS_LOG
#define LOG_SYS_HEADER(sys_name, sys_desc) \
    printf("\n[SYS-%s] %s\n%s\n", sys_name, sys_desc, LOG_SUBSEPARATOR);
#define LOG_INFO(msg, ...) \
    printf("  INFO: " msg "\n", ##__VA_ARGS__);
#else
#define LOG_SYS_HEADER(sys_name, sys_desc) \
    do                                     \
    {                                      \
    } while (0)
#define LOG_INFO(msg, ...) \
    do                     \
    {                      \
    } while (0)
#endif

namespace madEscape::llm_system
{

    #define REGISTER_ARCHETYPE_WITH_NAME(registry, ArchetypeT) \
    do { \
        registry.registerArchetype<ArchetypeT>(); \
        printf("Registered archetype: %s -> ID %u\n", #ArchetypeT, \
               TypeTracker::typeID<ArchetypeT>()); \
    } while(0)

// Macro for registering components with name printing  
#define REGISTER_COMPONENT_WITH_NAME(registry, ComponentT) \
    do { \
        registry.registerComponent<ComponentT>(); \
        printf("Registered component: %s -> ID %u\n", #ComponentT, \
               TypeTracker::typeID<ComponentT>()); \
    } while(0)


    void registerTypes(ECSRegistry &registry)
    {
        registry.registerComponent<NpuID>();
        registry.registerComponent<ChakraNodes>();
        registry.registerComponent<HardwareResource>();
        registry.registerComponent<ProcessingCompTask>();
        registry.registerComponent<ProcessingCommTasks>();
        registry.registerComponent<OneNPUFinishedFlag>();
        registry.registerComponent<ChakraNodesForNoDP>();
        // registry.registerArchetype<NpuNode>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, NpuNode);

        registry.registerComponent<NextProcessTimes>();
        // registry.registerArchetype<NextProcessTimeE>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, NextProcessTimeE);

        registry.registerComponent<CommModel>();
        // registry.registerArchetype<SysConfig>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, SysConfig);

        registry.registerComponent<NodeID>();
        registry.registerComponent<TaskFlows>();
        // registry.registerArchetype<ProcessComm_E>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, ProcessComm_E);

        // Register ring parameters component and archetype
        registry.registerComponent<RingParams>();
        // registry.registerArchetype<RingConfigEntity>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, RingConfigEntity);

        // Register ring topology component and archetype
        registry.registerComponent<RingTopology>();
        // registry.registerArchetype<RingTopoEntity>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, RingTopoEntity);

        registry.registerComponent<CheckNpuFinishFlag>();
        // registry.registerArchetype<CheckNpuFinishE>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, CheckNpuFinishE);


        registry.registerComponent<RecvNodeFlag>(); 
        // registry.registerArchetype<ProcessRecvComm_E>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, ProcessRecvComm_E);
    }

    void init(Engine &ctx)
    {
        ctx.data().ring_config_entity = Entity::none();
        ctx.data().sys_chakra_entities_created = false;
        // next process time entity
        Entity nextProcessTimeE = ctx.makeEntity<NextProcessTimeE>();
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; i++)
        {
            ctx.get<NextProcessTimes>(nextProcessTimeE).times_abs[i] = 0;
        }
        ctx.data().next_process_time_entity = nextProcessTimeE;

        // // Sys_config
        // Entity sc = ctx.makeEntity<SysConfig>();
        // ctx.get<CommModel>(sc).all_reduce_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).all_gather_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).reduce_scatter_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).all_to_all_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).reduce_scatter_block_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).barrier_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).gather_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).scatter_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).broadcast_implementation = CommImplementationType::Ring;
        // ctx.get<CommModel>(sc).reduce_implementation = CommImplementationType::Ring;
        // ctx.data().sys_config_entity = sc;

//         // collective topo
//         RingTopology ring_topo(Dimension::Local, 2, 4, 1, 2);

// #if SYS_LOG
//         printf("ring_topo.id_to_index[0]: %d\n", ring_topo.id_to_index[0]);
//         printf("ring_topo.index_to_id[1]: %d\n", ring_topo.index_to_id[1]);
//         printf("ring_topo.index_to_id[2]: %d\n", ring_topo.index_to_id[2]);
//         printf("ring_topo.id_to_index[3]: %d\n", ring_topo.id_to_index[3]);
// #endif


        Entity e = ctx.makeEntity<CheckNpuFinishE>();
        ctx.get<CheckNpuFinishFlag>(e) = CheckNpuFinishFlag();
        ctx.data().check_npu_finish_entity = e;

        for (size_t i = 0; i < NPU_NUM; i++)
        {
            for (size_t j = 0; j < NPU_NUM; j++)
            {
                ctx.data().send_recv_map_recvend[i][j] = 0;
            }
        }

    }

    void sys_init(Engine &ctx, ChakraNodesData &chakra_nodes_data, ProcessParams &processParams)
    {
        if(ctx.data().sys_chakra_entities_created)
        {
            return;
        }
        
#if SYS_LOG
        printf("\n[SYS-0] Initialize NPUs\n%s\n", LOG_SUBSEPARATOR);
#endif
        // Get number of rows and columns
        size_t rows = sizeof(chakra_nodes_data.data) / sizeof(chakra_nodes_data.data[0]);       // Total size / single row size
        size_t cols = sizeof(chakra_nodes_data.data[0]) / sizeof(chakra_nodes_data.data[0][0]); // Single row size / single element size
        size_t npu_nums = rows;
        size_t npu_data_num = cols;
#if SYS_LOG
        printf("  INFO: chakra_nodes_data bytes: total=%lu, per_row=%lu\n",
               (unsigned long)sizeof(chakra_nodes_data.data), (unsigned long)sizeof(chakra_nodes_data.data[0]));
        printf("  INFO: npu_count=%u, npu_data_len=%u\n", (unsigned int)npu_nums, (unsigned int)npu_data_num);
#endif

        for (size_t i = 0; i < npu_nums; i++)
        {
            Entity npuNode = ctx.makeEntity<NpuNode>();
            ctx.get<NpuID>(npuNode).value = i;
            ctx.get<NpuID>(npuNode).test_dim = false;
            ctx.get<NpuID>(npuNode).test_collective_comm_ring = false;
            int nodeCount = 0;
            parseChakraNodes(chakra_nodes_data, i, ctx.get<ChakraNodes>(npuNode).nodes, nodeCount);
            
            // Save nodeCount immediately
            const int savedNodeCount = nodeCount;
            if (savedNodeCount == 0)
            {
                ctx.destroyEntity(npuNode);
                // 打印：不是有效的node数据
                printf("  INFO: Not valid node data\n");
                return;
            }
            ctx.get<HardwareResource>(npuNode) = HardwareResource();
            ctx.get<ProcessingCompTask>(npuNode) = ProcessingCompTask();
            ctx.get<ProcessingCommTasks>(npuNode) = ProcessingCommTasks();
            ctx.get<ProcessingCommTasks>(npuNode).flow_id = i * FLOW_ID_MAX_LENGTH;
            ctx.get<OneNPUFinishedFlag>(npuNode) = OneNPUFinishedFlag();

#if SYS_LOG
            // Use %u instead of %zu to avoid GPU printf parameter misalignment bug
            printf("  INFO: NPU %u initialized with %d nodes\n", (unsigned int)i, savedNodeCount);
#endif
            ctx.data().npu_entities[i] = npuNode;



// #if SYS_LOG_SPECIAL
//             printf("####  checke NPU %zu ####\n", i);
//             uint32_t independentNodes[100];
//             int count = checkRecvNodeDependencies(ctx.get<ChakraNodes>(npuNode), independentNodes, 100);

//             if (count == 1) {
//               
//             } else {
//                 printf("ERROR： %d Independent RECV NODE\n", count);
//                 for(int j=0; j<count; j++)
//                 {
//                     printf("  INFO: Independent RECV node %d\n", independentNodes[j]);
//                 }
//             }
// #endif

        }

 
        // Only destroy init_entity if it exists
        if (!ctx.data().sys_chakra_entities_created)
        {

            // ctx.destroyEntity(ctx.data().init_entity);
            ctx.data().sys_chakra_entities_created=true;
      

            // Create ring configuration entity and set ring parameters
            Entity ring_entity = ctx.makeEntity<RingConfigEntity>();
            ctx.get<RingParams>(ring_entity).ring_dim0 = static_cast<uint32_t>(processParams.params[0]);
            ctx.get<RingParams>(ring_entity).ring_dim1 = static_cast<uint32_t>(processParams.params[1]);
            ctx.get<RingParams>(ring_entity).ring_dim2 = static_cast<uint32_t>(processParams.params[2]);
            ctx.get<RingParams>(ring_entity).chunks_num = static_cast<uint32_t>(processParams.params[3]);
            ctx.get<RingParams>(ring_entity).npu_num = static_cast<uint32_t>(processParams.params[100]);
            ctx.data().ring_config_entity = ring_entity;


            printf("  INFO: Ring parameters initialized: dim0=%u, dim1=%u, dim2=%u, chunks=%u, npu_num=%u\n",
                ctx.get<RingParams>(ring_entity).ring_dim0,
                ctx.get<RingParams>(ring_entity).ring_dim1,
                ctx.get<RingParams>(ring_entity).ring_dim2,
                ctx.get<RingParams>(ring_entity).chunks_num,
                ctx.get<RingParams>(ring_entity).npu_num);
            printf("%s\n", LOG_SUBSEPARATOR);

            // Create and initialize ring topology entities for three dimensions
            // Dimension 0: Local (offset=1)
            Entity ring_topo0_entity = ctx.makeEntity<RingTopoEntity>();
            ctx.get<RingTopology>(ring_topo0_entity).initialize(
                Dimension::Local, 
                ctx.get<RingParams>(ring_entity).ring_dim0, 
                1);
            ctx.data().ring_topo_entity[0] = ring_topo0_entity;

            // Dimension 1: Horizontal (offset=ring_dim0)
            Entity ring_topo1_entity = ctx.makeEntity<RingTopoEntity>();
            ctx.get<RingTopology>(ring_topo1_entity).initialize(
                Dimension::Horizontal, 
                ctx.get<RingParams>(ring_entity).ring_dim1, 
                ctx.get<RingParams>(ring_entity).ring_dim0);
            ctx.data().ring_topo_entity[1] = ring_topo1_entity;

            // Dimension 2: Vertical (offset=ring_dim0 * ring_dim1)
            Entity ring_topo2_entity = ctx.makeEntity<RingTopoEntity>();
            ctx.get<RingTopology>(ring_topo2_entity).initialize(
                Dimension::Vertical, 
                ctx.get<RingParams>(ring_entity).ring_dim2, 
                ctx.get<RingParams>(ring_entity).ring_dim0 * ctx.get<RingParams>(ring_entity).ring_dim1);
            ctx.data().ring_topo_entity[2] = ring_topo2_entity;

            printf("  INFO: Ring topology entities initialized for 3 dimensions\n");
            printf("%s\n", LOG_SUBSEPARATOR);

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
            // Sys_config
            Entity sc = ctx.makeEntity<SysConfig>();
            ctx.get<CommModel>(sc).all_reduce_implementation = (CommImplementationType)processParams.params[200];
            ctx.get<CommModel>(sc).reduce_implementation = (CommImplementationType)processParams.params[201];
            ctx.get<CommModel>(sc).all_gather_implementation = (CommImplementationType)processParams.params[202];
            ctx.get<CommModel>(sc).gather_implementation = (CommImplementationType)processParams.params[203];
            ctx.get<CommModel>(sc).scatter_implementation = (CommImplementationType)processParams.params[204];
            ctx.get<CommModel>(sc).broadcast_implementation = (CommImplementationType)processParams.params[205];
            ctx.get<CommModel>(sc).all_to_all_implementation = (CommImplementationType)processParams.params[206];
            ctx.get<CommModel>(sc).reduce_scatter_implementation = (CommImplementationType)processParams.params[207];
            ctx.get<CommModel>(sc).reduce_scatter_block_implementation = (CommImplementationType)processParams.params[208];
            ctx.get<CommModel>(sc).barrier_implementation = (CommImplementationType)processParams.params[209];
            
            
            ctx.data().sys_config_entity = sc;

            for(size_t i=0; i<NPU_NUM; i++)
            {
                for(size_t j=0; j<MAX_CHAKRA_NODES_PER_NPU; j++)
                {
                    ctx.data().npus_chakra_exec_entity[i][j] = Entity::none();

                }
            }


            for (uint32_t i = 0; i < NPU_NUM; i++) {
                for (uint32_t j = 0; j < MAX_CONCURRENT_CHAKRA_NODES_PER_NPU; j++) {
                    ctx.data().npus_chakra_exec_entity[i][j] = ctx.makeEntity<ProcessComm_E>();
                }
            }
            for (uint32_t i = 0; i < NPU_NUM; i++) {
                for (uint32_t j = 0; j < MAX_CONCURRENT_CHAKRA_NODES_PER_NPU; j++) {
                    ctx.destroyEntity(ctx.data().npus_chakra_exec_entity[i][j]);
                }
            }
            

        }
        
    }

}