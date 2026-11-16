#include <madrona/mw_gpu_entry.hpp>
#include "sim.hpp"
#include "level_gen.hpp"
#include "network/queue_system.hpp"
#include "network/congestion_control_system.hpp"
#include "network/packet_system.hpp"
#include "network/flow_system.hpp"
#include "network/nic_system.hpp"
#include "network/forward_system.hpp"
#include "network/transmit_system.hpp"
#include "network/network_config_system.hpp"
#include "sys/llm_system.hpp"
#include <algorithm>
#include <iostream>

// Macro for registering archetypes with name printing
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

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;
using namespace std;

#define INF 0x7FFFFFFF

namespace RenderingSystem = madrona::render::RenderingSystem;

namespace madEscape {

    void Sim::registerTypes(ECSRegistry &registry, const Config &cfg)
    {
        llm_system::registerTypes(registry);
        registry.registerComponent<EntityType>();
        registry.registerComponent<CurStep>();
        registry.registerComponent<Results>();
        registry.registerComponent<Results2>();
        registry.registerComponent<SimulationTime>();
        registry.registerComponent<MadronaEventsQueue>();
        registry.registerComponent<MadronaEvents>();
        registry.registerComponent<MadronaEventsResult>();
        registry.registerComponent<ProcessParams>();
        registry.registerComponent<TopoTensor>();
        registry.registerComponent<FibTensor>();
        //
        REGISTER_COMPONENT_WITH_NAME(registry, ChakraNodesData);  // Very large component!


        registry.registerSingleton<WorldReset>();
        registry.registerSingleton<LevelState>();


        REGISTER_ARCHETYPE_WITH_NAME(registry, Agent);
        //registry.registerArchetype<PhysicsEntity>();
        //registry.registerArchetype<DoorEntity>();
        //registry.registerArchetype<ButtonEntity>();


        registry.exportSingleton<WorldReset>(
                (uint32_t)ExportID::Reset);
//        registry.exportColumn<Agent, Action>(
//                (uint32_t)ExportID::Action);
//        registry.exportColumn<Agent, SelfObservation>(
//                (uint32_t)ExportID::SelfObservation);
//        registry.exportColumn<Agent, PartnerObservations>(
//                (uint32_t)ExportID::PartnerObservations);
//        registry.exportColumn<Agent, RoomEntityObservations>(
//                (uint32_t)ExportID::RoomEntityObservations);
//        registry.exportColumn<Agent, DoorObservation>(
//                (uint32_t)ExportID::DoorObservation);
//        registry.exportColumn<Agent, Lidar>(
//                (uint32_t)ExportID::Lidar);
//        registry.exportColumn<Agent, StepsRemaining>(
//                (uint32_t)ExportID::StepsRemaining);
//        registry.exportColumn<Agent, Reward>(
//                (uint32_t)ExportID::Reward);
//        registry.exportColumn<Agent, Done>(
//                (uint32_t)ExportID::Done);
//



        //
        registry.exportColumn<Agent, Results>((uint32_t)ExportID::Results);
        registry.exportColumn<Agent, Results2>((uint32_t)ExportID::Results2);
        registry.exportColumn<Agent, SimulationTime>((uint32_t)ExportID::SimulationTime);
        registry.exportColumn<Agent, MadronaEvents>((uint32_t)ExportID::MadronaEvents);
        registry.exportColumn<Agent, MadronaEventsResult>((uint32_t)ExportID::MadronaEventsResult);
        registry.exportColumn<Agent, ProcessParams>((uint32_t)ExportID::ProcessParams);
        registry.exportColumn<Agent, TopoTensor>((uint32_t)ExportID::TopoTensor);
        registry.exportColumn<Agent, FibTensor>((uint8_t)ExportID::FibTensor);
        registry.exportColumn<Agent, ChakraNodesData>((uint32_t)ExportID::ChakraNodesData);
        // for the router forwarding function, of GPU_acclerated DES
        registry.registerComponent<NICRate>();
        registry.registerComponent<HSLinkDelay>();
        registry.registerComponent<NextHopType>();
        registry.registerComponent<NextHop>();
        // registry.registerComponent<Pkt>();
        registry.registerComponent<PktBuf>();
        //pfc
        registry.registerComponent<PFCState>();
        //PktQueue is composed of multiple PktBuf and a PFCState array
        registry.registerComponent<PktQueue>();
        registry.registerComponent<SimTime>();
        registry.registerComponent<SimTimePerUpdate>();
        // components for send flow entity
        registry.registerComponent<FlowID>();
        registry.registerComponent<Src>();
        registry.registerComponent<Dst>();
        registry.registerComponent<L4Port>();
        registry.registerComponent<FlowSize>();
        registry.registerComponent<StartTime>();
        registry.registerComponent<StopTime>();
        registry.registerComponent<NIC_ID>();
        registry.registerComponent<SndServerID>();
        registry.registerComponent<RecvServerID>();
        registry.registerComponent<SndNxt>();
        registry.registerComponent<SndUna>();
        registry.registerComponent<FlowState>();
        registry.registerComponent<AckPktBuf>();
        registry.registerComponent<LastAckTimestamp>();
        registry.registerComponent<NxtPktEvent>();
        registry.registerComponent<CC_Para>();

        REGISTER_ARCHETYPE_WITH_NAME(registry, SndFlow);

        ///////////////////////////////////////////////////////
        // components for recv flow entity
        registry.registerComponent<LastCNPTimestamp>();
        registry.registerComponent<RecvBytes>();
        registry.registerComponent<RecvBitmapAndTriggerCondition>();
        REGISTER_ARCHETYPE_WITH_NAME(registry, RecvFlow);

        registry.registerComponent<NET_NPU_ID>();
        registry.registerComponent<SendFlows>();
        registry.registerComponent<NewFlowQueue>();
        registry.registerComponent<CompletedFlowQueue>();


        REGISTER_ARCHETYPE_WITH_NAME(registry, NET_NPU);

        ///////////////////////////////////////////////////////
        // components for NIC entity
        registry.registerComponent<BidPktBuf>();
        registry.registerComponent<TXHistory>();
        registry.registerComponent<Seed>();

        REGISTER_ARCHETYPE_WITH_NAME(registry, NIC);

        ///////////////////////////////////////////////////////
        // components for switch entity
        registry.registerComponent<SwitchID>();
        registry.registerComponent<SwitchType>();
        REGISTER_COMPONENT_WITH_NAME(registry, FIBTable);  // Large component!
        registry.registerComponent<QueueNumPerPort>();
        registry.registerComponent<StartPortID>();
        registry.registerComponent<StopPortID>();
        registry.registerComponent<PortNum>();

        REGISTER_ARCHETYPE_WITH_NAME(registry, Switch);

        ///////////////////////////////////////////////////////
        // components for egress and ingress entity
        registry.registerComponent<LinkRate>();
        registry.registerComponent<SSLinkDelay>();
        registry.registerComponent<SchedTrajType>();
        registry.registerComponent<PortType>();
        registry.registerComponent<LocalPortID>();
        registry.registerComponent<GlobalPortID>();
        registry.registerComponent<ForwardPlan>();

        REGISTER_ARCHETYPE_WITH_NAME(registry, IngressPort);
        REGISTER_ARCHETYPE_WITH_NAME(registry, EgressPort);
        //

    }

    // Build the task graph
    void Sim::setupTasks(TaskGraphManager &taskgraph_mgr, const Config &cfg)
    {
        TaskGraphBuilder &builder = taskgraph_mgr.init(TaskGraphID::Step);
        if (DEV_MODE == 0)
        {
            printf("DEV_MODE == SYS\n");
            auto sys_llm_sys_init = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_init,
                 ChakraNodesData, ProcessParams>>({});
            auto sys_process_chakra_node = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_processChakraNodes,
                 NpuID, ChakraNodes, HardwareResource, ProcessingCompTask, ProcessingCommTasks,OneNPUFinishedFlag, ChakraNodesForNoDP>>({sys_llm_sys_init});
            auto sys_check_flow = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkFlow,
                NpuID, NodeID, TaskFlows>>({sys_process_chakra_node});
            auto sys_check_recv_flow = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkRecvFlow,
                NpuID, NodeID, RecvNodeFlag>>({sys_check_flow});
            // only sys layer should clear the finished flow queue                                        
            auto clear_finished_flow_queue_sys = builder.addToGraph<ParallelForNode<Engine, clear_finished_flow_queue, NET_NPU_ID, CompletedFlowQueue, \
               SimTime, SimTimePerUpdate>>({sys_check_recv_flow});
            auto sys_remove_chakra_node = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_removeChakraNodes,
                NpuID, ChakraNodes, HardwareResource, ProcessingCompTask, ProcessingCommTasks>>({clear_finished_flow_queue_sys});
            auto sys_skip_time = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkSkipTime, NextProcessTimes>>({sys_remove_chakra_node});
            auto sys_check_npu_finish = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkNpuFinish, CheckNpuFinishFlag>>({sys_skip_time});
            (void)sys_check_npu_finish;

            #ifdef MADRONA_GPU_MODE

                auto compact_process_e = builder.addToGraph<CompactArchetypeNode<ProcessComm_E>>({sys_check_npu_finish});
                auto compact_process_recv_comm_e = builder.addToGraph<CompactArchetypeNode<ProcessRecvComm_E>>({compact_process_e});
                // RecycleEntitiesNode is required on the GPU backend in order to reclaim
                // deleted entity IDs.
                auto recycle_sys = builder.addToGraph<RecycleEntitiesNode>({compact_process_recv_comm_e});
                (void)recycle_sys;
                #endif

            #if TEST_RING_TOPO_LOG_FOR_CPU_ONLY
               auto sys_test_ring_topo_dim = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_test_ring_topo_dim, NpuID, ChakraNodes, HardwareResource, ProcessingCompTask, ProcessingCommTasks,OneNPUFinishedFlag, ChakraNodesForNoDP>>({sys_check_npu_finish});
               (void)sys_test_ring_topo_dim;
            #endif


            // -----------------------------------------------------------------------------------
        }
        else if (DEV_MODE == 1)
        {
            printf("DEV_MODE == NET\n");
            // in the first step(frame 0), generate the network entities based on the tensor data
            auto pass_config_data_and_gen_network_entt_sys = builder.addToGraph<ParallelForNode<Engine, pass_config_data_and_gen_network_entt, TopoTensor, FibTensor>>({});
            auto get_flow_sys = builder.addToGraph<ParallelForNode<Engine, comm_set_flow, NET_NPU_ID, NewFlowQueue, \
                                SimTime, SimTimePerUpdate>>({pass_config_data_and_gen_network_entt_sys});
            auto setup_flow_sys = builder.addToGraph<ParallelForNode<Engine, setup_flow, NET_NPU_ID, NewFlowQueue, \
                                  SimTime, SimTimePerUpdate>>({get_flow_sys});
            auto nic_receive_sys = builder.addToGraph<ParallelForNode<Engine, nic_receive, NIC_ID, NET_NPU_ID, \
                                   BidPktBuf, NICRate, SimTime, SimTimePerUpdate>>({setup_flow_sys});
            auto flow_receive_sys = builder.addToGraph<ParallelForNode<Engine, flow_receive, FlowID, Src, Dst, PktBuf, \
                                    NICRate, HSLinkDelay, SimTime, SimTimePerUpdate, \
                                    RecvBytes, RecvBitmapAndTriggerCondition, L4Port, LastCNPTimestamp, StartTime>>({nic_receive_sys});
            auto flow_send_sys = builder.addToGraph<ParallelForNode<Engine, flow_send, FlowID, Src, Dst, \
                                 L4Port, \
                                 FlowSize, StartTime, StopTime,\
                                 NIC_ID, SndServerID, RecvServerID, \
                                 SndNxt, SndUna, FlowState,\
                                 LastAckTimestamp, NxtPktEvent, PFCState, CC_Para,\
                                 PktBuf, AckPktBuf,\
                                 NICRate, HSLinkDelay, SimTime, SimTimePerUpdate>>({flow_receive_sys});
            auto check_flow_state_sys = builder.addToGraph<ParallelForNode<Engine, check_flow_state, NET_NPU_ID, CompletedFlowQueue, \
                                        SimTime, SimTimePerUpdate>>({flow_send_sys});
            auto nic_forward_sys = builder.addToGraph<ParallelForNode<Engine, nic_forward, NIC_ID, \
                                   NET_NPU_ID, NICRate, SimTime, SimTimePerUpdate, \
                                   BidPktBuf>>({check_flow_state_sys});
            auto nic_transmit_sys = builder.addToGraph<ParallelForNode<Engine, nic_transmit, NIC_ID, NET_NPU_ID, \
                                    NICRate, HSLinkDelay, \
                                    SimTime, SimTimePerUpdate, \
                                    BidPktBuf, TXHistory, \
                                    NextHop, Seed>>({nic_forward_sys});
            auto set_forward_plan_sys = builder.addToGraph<ParallelForNode<Engine, set_forward_plan, \
                                        LocalPortID, GlobalPortID, \
                                        SwitchID, PktBuf, ForwardPlan, \
                                        SimTime, SimTimePerUpdate>>({nic_transmit_sys});
            auto forward_sys = builder.addToGraph<ParallelForNode<Engine, _forward, SchedTrajType, \
                               LocalPortID, GlobalPortID, PktQueue, SwitchID, \
                               SimTime, SimTimePerUpdate, LinkRate, Seed, PFCState>>({set_forward_plan_sys});
            auto transmit_sys = builder.addToGraph<ParallelForNode<Engine, transmit, SchedTrajType, \
                                PortType, LocalPortID, GlobalPortID, SwitchID, NextHop, NextHopType, PktQueue, TXHistory, \
                                SSLinkDelay, LinkRate, SimTime, SimTimePerUpdate, Seed, PFCState>>({forward_sys});
            auto remove_pkts_sys = builder.addToGraph<ParallelForNode<Engine, remove_pkts, \
                                   LocalPortID, GlobalPortID, PktBuf, ForwardPlan, \
                                   SimTime, SimTimePerUpdate, SwitchID>>({transmit_sys});
            // (void)remove_pkts_sys;

            // #ifdef MADRONA_GPU_MODE
            //     // RecycleEntitiesNode is required on the GPU backend in order to reclaim
            //     // deleted entity IDs.
            //     auto recycle_sys = builder.addToGraph<RecycleEntitiesNode>({remove_pkts_sys});
            //     (void)recycle_sys;
            // #endif

            #ifdef MADRONA_GPU_MODE

            auto compact_snd_flow = builder.addToGraph<CompactArchetypeNode<SndFlow>>({remove_pkts_sys});
            auto compact_recv_flow = builder.addToGraph<CompactArchetypeNode<RecvFlow>>({compact_snd_flow});
            
            // RecycleEntitiesNode is required on the GPU backend in order to reclaim
            // deleted entity IDs.
            auto recycle_sys = builder.addToGraph<RecycleEntitiesNode>({compact_recv_flow});
            // (void)recycle_sys;

            #endif

        }
        else if (DEV_MODE == 2)
        {
            printf("DEV_MODE == Mix\n");
               

            // in the first step(frame 0), generate the network entities based on the tensor data
            auto pass_config_data_and_gen_network_entt_sys = builder.addToGraph<ParallelForNode<Engine, pass_config_data_and_gen_network_entt, TopoTensor, FibTensor>>({});
            auto setup_flow_sys = builder.addToGraph<ParallelForNode<Engine, setup_flow, NET_NPU_ID, NewFlowQueue, \
                                  SimTime, SimTimePerUpdate>>({pass_config_data_and_gen_network_entt_sys});

            auto nic_receive_sys = builder.addToGraph<ParallelForNode<Engine, nic_receive, NIC_ID, NET_NPU_ID, \
                                   BidPktBuf, NICRate, SimTime, SimTimePerUpdate>>({setup_flow_sys});

            auto flow_receive_sys = builder.addToGraph<ParallelForNode<Engine, flow_receive, FlowID, Src, Dst, PktBuf, \
                                    NICRate, HSLinkDelay, SimTime, SimTimePerUpdate, \
                                    RecvBytes, RecvBitmapAndTriggerCondition, L4Port, LastCNPTimestamp, StartTime>>({nic_receive_sys});
            auto flow_send_sys = builder.addToGraph<ParallelForNode<Engine, flow_send, FlowID, Src, Dst, \
                                 L4Port, \
                                 FlowSize, StartTime, StopTime,\
                                 NIC_ID, SndServerID, RecvServerID, \
                                 SndNxt, SndUna, FlowState,\
                                 LastAckTimestamp, NxtPktEvent, PFCState, CC_Para,\
                                 PktBuf, AckPktBuf,\
                                 NICRate, HSLinkDelay, SimTime, SimTimePerUpdate>>({flow_receive_sys});
            auto check_flow_state_sys = builder.addToGraph<ParallelForNode<Engine, check_flow_state, NET_NPU_ID, CompletedFlowQueue, \
                                        SimTime, SimTimePerUpdate>>({flow_send_sys});
            auto nic_forward_sys = builder.addToGraph<ParallelForNode<Engine, nic_forward, NIC_ID, \
                                   NET_NPU_ID, NICRate, SimTime, SimTimePerUpdate, \
                                   BidPktBuf>>({check_flow_state_sys});
            auto nic_transmit_sys = builder.addToGraph<ParallelForNode<Engine, nic_transmit, NIC_ID, NET_NPU_ID, \
                                    NICRate, HSLinkDelay, \
                                    SimTime, SimTimePerUpdate, \
                                    BidPktBuf, TXHistory, \
                                    NextHop, Seed>>({nic_forward_sys});
            auto set_forward_plan_sys = builder.addToGraph<ParallelForNode<Engine, set_forward_plan, \
                                        LocalPortID, GlobalPortID, \
                                        SwitchID, PktBuf, ForwardPlan, \
                                        SimTime, SimTimePerUpdate>>({nic_transmit_sys});
            auto forward_sys = builder.addToGraph<ParallelForNode<Engine, _forward, SchedTrajType, \
                               LocalPortID, GlobalPortID, PktQueue, SwitchID, \
                               SimTime, SimTimePerUpdate, LinkRate, Seed, PFCState>>({set_forward_plan_sys});
            auto transmit_sys = builder.addToGraph<ParallelForNode<Engine, transmit, SchedTrajType, \
                                PortType, LocalPortID, GlobalPortID, SwitchID, NextHop, NextHopType, PktQueue, TXHistory, \
                                SSLinkDelay, LinkRate, SimTime, SimTimePerUpdate, Seed, PFCState>>({forward_sys});

            auto remove_pkts_sys = builder.addToGraph<ParallelForNode<Engine, remove_pkts, \
                                   LocalPortID, GlobalPortID, PktBuf, ForwardPlan, \
                                   SimTime, SimTimePerUpdate, SwitchID>>({transmit_sys});

        //     (void)transmit_sys;

             // ------------------sys--------------------------------------------------------------
    
            auto sys_llm_sys_init = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_init,
                ChakraNodesData, ProcessParams>>({remove_pkts_sys});

            auto sys_process_chakra_node = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_processChakraNodes,
                NpuID, ChakraNodes, HardwareResource, ProcessingCompTask, ProcessingCommTasks,OneNPUFinishedFlag, ChakraNodesForNoDP>>({sys_llm_sys_init});

            // ------------------sys--------------------------------------------------------------
            auto sys_check_flow = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkFlow,
                NpuID, NodeID, TaskFlows>>({sys_process_chakra_node});

            auto sys_check_recv_flow = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkRecvFlow,
                NpuID, NodeID, RecvNodeFlag>>({sys_check_flow});

            // only sys layer should clear the finished flow queue                                        
            auto clear_finished_flow_queue_sys = builder.addToGraph<ParallelForNode<Engine, clear_finished_flow_queue, NET_NPU_ID, CompletedFlowQueue, \
                SimTime, SimTimePerUpdate>>({sys_check_recv_flow});

            auto sys_remove_chakra_node = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_removeChakraNodes,
                NpuID, ChakraNodes, HardwareResource, ProcessingCompTask, ProcessingCommTasks>>({clear_finished_flow_queue_sys});

            auto sys_skip_time = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkSkipTime, NextProcessTimes>>({sys_remove_chakra_node});
            //  (void)sys_skip_time;
            auto sys_check_npu_finish = builder.addToGraph<ParallelForNode<Engine, llm_system::sys_checkNpuFinish, CheckNpuFinishFlag>>({sys_skip_time});
            // (void)sys_check_npu_finish;


            #ifdef MADRONA_GPU_MODE

            auto compact_snd_flow = builder.addToGraph<CompactArchetypeNode<SndFlow>>({sys_check_npu_finish});
            auto compact_recv_flow = builder.addToGraph<CompactArchetypeNode<RecvFlow>>({compact_snd_flow});
            auto compact_process_e = builder.addToGraph<CompactArchetypeNode<ProcessComm_E>>({compact_recv_flow});
            auto compact_process_recv_comm_e = builder.addToGraph<CompactArchetypeNode<ProcessRecvComm_E>>({compact_process_e});
            // RecycleEntitiesNode is required on the GPU backend in order to reclaim
            // deleted entity IDs.
            auto recycle_sys = builder.addToGraph<RecycleEntitiesNode>({compact_process_recv_comm_e});
            (void)recycle_sys;
            #endif

        // -----------------------------------------------------------------------------------
        }
    }

    Sim::Sim(Engine &ctx,
            const Config &cfg,
            const WorldInit &)
        : WorldBase(ctx)
    {

        llm_system::init(ctx);
        // Currently the physics system needs an upper bound on the number of
        // entities that will be stored in the BVH. We plan to fix this in
        // a future release.
        printf("\n***********enter Sim construction function!\n");
        // constexpr CountT max_total_entities = consts::numAgents +
        //     consts::numRooms * (consts::maxEntitiesPerRoom + 3) +
        //     4; // side walls + floor

        // Original limit was too small (32 entities) for network topology
        // Network topology requires:
        // - Switches: MAX_SW_NUM (320)
        // - NPUs: MAX_NET_NPU_NUM (1024)
        // - NICs: MAX_NET_NPU_NUM*2 (2048)
        // - Ingress Ports: MAX_ALL_PORT_NUM (12288)
        // - Egress Ports: MAX_ALL_PORT_NUM+100000 (112288)
        // - Agent: 1
        // Total: ~125,000+ entities
        // constexpr CountT max_total_entities = 150000; // Increased to accommodate network topology

        // phys::PhysicsSystem::init(ctx, cfg.rigidBodyObjMgr,
        //         consts::deltaT, consts::numPhysicsSubsteps, -9.8f * math::up,
        //         max_total_entities);

        // initRandKey = cfg.initRandKey;
        // autoReset = cfg.autoReset;

        // enableRender = cfg.renderBridge != nullptr;

        // if (enableRender) {
        //     RenderingSystem::init(ctx, cfg.renderBridge);
        // }

        curWorldEpisode = 0;
        ctx.data().max_flow_num = cfg.kAray;
        ctx.data().max_equivalent_path_num = cfg.maxEquivalentPathNum;
        // Create Agent entity early to enable tensor access from Python
        // This must be done before Python tries to access tensors
        create_agent_for_tensor_access(ctx);
        // Initialize ctx.data to default empty state
        // All data will come from tensor transfers only
        ctx.data().num_link = 0;
        ctx.data().num_net_npu = 0;
        ctx.data().num_switch = 0;
        ctx.data().sw_port_num = 0;
        ctx.data().net_npu_port_num = 0;
        ctx.data().entities_created = false;
        // for the router forwarding function, of GPU_acclerated DES
        numInPort = 0;
        numEPort = 0;
        num_nic = 0;
        for (int i = 0; i < MAX_NET_NPU_NUM; i++) {
            flow_cnt[i] = 0;
        }
    }
    // This declaration is needed for the GPU backend in order to generate the
    // CUDA kernel for world initialization, which needs to be specialized to the
    // application's world data type (Sim) and config and initialization types.
    // On the CPU it is a no-op.
    MADRONA_BUILD_MWGPU_ENTRY(Engine, Sim, Sim::Config, Sim::WorldInit);
}
