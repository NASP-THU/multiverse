#include "flow_system.hpp"
#include "../sys/net_sys_interface.hpp"
#include "../types/global_types.hpp"
#include "packet_system.hpp"
#include "queue_system.hpp"
#include <cstdio>
#include <cmath>

namespace madEscape {

    // create send flow entity and recv flow entity
    void create_flow(Engine &ctx, FlowEvent flow_event, uint64_t flow_id, int64_t sim_time) {
        //printf("Entering create_flow for src: %u\n", flow_event.src);

        // // Lock only around shared resource access
        // ctx.data().flow_lock.lock();

        // 4 type of entity for each flow:
        // 1) snd flow in sender side (send data pkt)
        // 2) recv flow in receiver side (receive data pkt)
        // 3) snd flow in receiver side (send ACK/NACK/CNP pkt)
        // 4) recv flow in sender side (receive ACK/NACK/CNP pkt)

        // only snd flows in receiver side change their src and dst.
        uint32_t hop_idx = flow_id % ctx.data().next_hop_num[flow_event.src][flow_event.dst]; // flow_id % nic_num
        uint32_t local_nic_idx = ctx.data().fib[flow_event.src][flow_event.dst][hop_idx];

        Entity nic_entt = ctx.data()._nics[flow_event.src][local_nic_idx];
        int64_t nic_rate = ctx.get<NICRate>(nic_entt).nic_rate;
        // printf("flow_event: src: %u, dst: %u, flow_id: %lu, hop_idx: %u, local_nic_idx: %u, nic_rate: %lu\n", flow_event.src, flow_event.dst, flow_id, hop_idx, local_nic_idx, nic_rate);


        //1) snd flow in sender side (send data pkt)
        Entity snd_flow_entt = ctx.makeEntity<SndFlow>();
        // printf("snd_flow_entt.id: %u, gen: %d\n", snd_flow_entt.id, snd_flow_entt.gen);

        ctx.get<FlowID>(snd_flow_entt).flow_id = flow_id;
        ctx.get<Src>(snd_flow_entt).src = flow_event.src;
        ctx.get<Dst>(snd_flow_entt).dst = flow_event.dst;
        ctx.get<L4Port>(snd_flow_entt).l4_port = flow_event.l4_port; //FIXME

        ctx.get<FlowSize>(snd_flow_entt).flow_size = flow_event.flow_size;
        ctx.get<StartTime>(snd_flow_entt).start_time = flow_event.start_time;
        ctx.get<StartTime>(snd_flow_entt).started = false;
        ctx.get<StopTime>(snd_flow_entt).stop_time = 0;

        // ctx.get<NIC_ID>(snd_flow_entt).nic_id = flow_event.src;
        ctx.get<NIC_ID>(snd_flow_entt).nic_id = local_nic_idx;
        // printf("snd_flow_entt.nic_id: %u\n", ctx.get<NIC_ID>(snd_flow_entt).nic_id);

        // ctx.get<SndServerID>(snd_flow_entt).snd_server_id = flow_events[i][6];
        // ctx.get<RecvServerID>(snd_flow_entt).recv_server_id = flow_events[i][7];

        ctx.get<SndNxt>(snd_flow_entt).snd_nxt = 0;
        ctx.get<SndUna>(snd_flow_entt).snd_una = 0;
        ctx.get<FlowState>(snd_flow_entt) = FlowState::UNCOMPLETE;

        ctx.get<LastAckTimestamp>(snd_flow_entt).last_ack_timestamp = 0; //1ms
        ctx.get<NxtPktEvent>(snd_flow_entt).nxt_pkt_event = INF;

        ctx.get<PFCState>(snd_flow_entt) = PFCState::RESUME;

        // ctx.get<CC_Para>(snd_flow_entt).current_algo = CCAlgo::DCTCP;
        ctx.get<CC_Para>(snd_flow_entt).current_algo = CCAlgo::DCQCN;
        ctx.get<CC_Para>(snd_flow_entt).m_rate = nic_rate;
        ctx.get<CC_Para>(snd_flow_entt).tar_rate = nic_rate;
        ctx.get<CC_Para>(snd_flow_entt).line_rate = nic_rate;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_Alpha = DCQCN_Alpha;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_G = DCQCN_G;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_AIRate = DCQCN_AIRate;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_HAIRate = DCQCN_HAIRate; 
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_RateDecreaseInterval = DCQCN_RateDecreaseInterval;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_RateIncreaseInterval = DCQCN_RateIncreaseInterval;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_AlphaUpdateInterval = DCQCN_AlphaUpdateInterval;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_IncreaseStageCount = DCQCN_IncreaseStageCount;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_RecoveryStageThreshold = DCQCN_RecoveryStageThreshold;

        ctx.get<CC_Para>(snd_flow_entt).dcqcn_RateDecreaseNextTime = INF;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_RateIncreaseNextTime = INF;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_AlphaUpdateNextTime = INF;
        ctx.get<CC_Para>(snd_flow_entt).dcqcn_RateIsDecreased = DCQCN_RateIsDecreased;
        ctx.get<CC_Para>(snd_flow_entt).CNPState = false;

        // we should set DCTCP parameters
        ctx.get<CC_Para>(snd_flow_entt).dctcp_ACKedBytes = DCTCP_ACKedBytes;
        ctx.get<CC_Para>(snd_flow_entt).dctcp_ECNMarkedBytes = DCTCP_ECNMarkedBytes;
        ctx.get<CC_Para>(snd_flow_entt).dctcp_Alpha = DCTCP_Alpha;
        ctx.get<CC_Para>(snd_flow_entt).dctcp_G = DCTCP_G;
        ctx.get<CC_Para>(snd_flow_entt).dctcp_RecvNextNumber = -1;
        ctx.get<CC_Para>(snd_flow_entt).dctcp_LastUpdateACKNumber = DCTCP_LastUpdateACKNumber;
        ctx.get<CC_Para>(snd_flow_entt).dctcp_RAIRate = DCTCP_RAIRate;


        // inititalize for the parameters of queue 
        ctx.get<PktBuf>(snd_flow_entt).head = 0;
        ctx.get<PktBuf>(snd_flow_entt).tail = 0;
        ctx.get<PktBuf>(snd_flow_entt).cur_num = 0;
        ctx.get<PktBuf>(snd_flow_entt).cur_bytes = 0;
        ctx.get<PktBuf>(snd_flow_entt).queue_bytes_on_dequeue = 0;
        ctx.get<PktBuf>(snd_flow_entt).queue_bytes_on_enqueue = 0;

        ctx.get<AckPktBuf>(snd_flow_entt).head = 0;
        ctx.get<AckPktBuf>(snd_flow_entt).tail = 0;
        ctx.get<AckPktBuf>(snd_flow_entt).cur_num = 0;
        ctx.get<AckPktBuf>(snd_flow_entt).cur_bytes = 0;


        ctx.get<NICRate>(snd_flow_entt).nic_rate = nic_rate; // 100 Gbps
        ctx.get<HSLinkDelay>(snd_flow_entt).HS_link_delay = HS_LINK_DELAY; // 1 us, 1000 ns 

        ctx.get<SimTime>(snd_flow_entt).sim_time = sim_time;
        ctx.get<SimTimePerUpdate>(snd_flow_entt).sim_time_per_update = LOOKAHEAD_TIME; //1000ns

        // Add bounds checking before array access
        // Lock around snd_flows map access to prevent race conditions  
        ctx.data().flow_lock.lock();
        ctx.data().snd_flows[flow_event.src][flow_id] = snd_flow_entt;
        ctx.data().flow_lock.unlock();


        // 2) recv flow in receiver side (receive data pkt)
        Entity rcv_flow_entt = ctx.makeEntity<RecvFlow>();

        ctx.get<FlowID>(rcv_flow_entt).flow_id = flow_id;
        ctx.get<Src>(rcv_flow_entt).src = flow_event.src;
        ctx.get<Dst>(rcv_flow_entt).dst = flow_event.dst;
        ctx.get<L4Port>(rcv_flow_entt).l4_port = flow_event.l4_port; //FIXME

        ctx.get<FlowSize>(rcv_flow_entt).flow_size = flow_event.flow_size;
        ctx.get<StartTime>(rcv_flow_entt).start_time = flow_event.start_time;
        ctx.get<StartTime>(rcv_flow_entt).started = false;
        ctx.get<StopTime>(rcv_flow_entt).stop_time = 0;

        ctx.get<NIC_ID>(rcv_flow_entt).nic_id = local_nic_idx;

        ctx.get<FlowState>(rcv_flow_entt) = FlowState::UNCOMPLETE;

        ctx.get<LastCNPTimestamp>(rcv_flow_entt).last_cnp_timestamp = 0; //1ms

        ctx.get<RecvBytes>(rcv_flow_entt).recv_bytes = 0;
        for (int i = 0; i < PKT_BUF_LEN; i++){
            ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).bitmap[i] = 0;
        }
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).head = 0;
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).tail = 0;
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).bitmap_length = 0;
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).bitmap_value = 0;
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).depth_interval_timer = 0; 
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).bubble_timer = 0;
        ctx.get<RecvBitmapAndTriggerCondition>(rcv_flow_entt).cache_hole_depth = 0;

        ctx.get<PktBuf>(rcv_flow_entt).head = 0;
        ctx.get<PktBuf>(rcv_flow_entt).tail = 0;
        ctx.get<PktBuf>(rcv_flow_entt).cur_num = 0;
        ctx.get<PktBuf>(rcv_flow_entt).cur_bytes = 0;

        ctx.get<NICRate>(rcv_flow_entt).nic_rate = nic_rate; // 100 Gbps  
        ctx.get<HSLinkDelay>(rcv_flow_entt).HS_link_delay = HS_LINK_DELAY; // 1 us, 1000 ns 

        ctx.get<SimTime>(rcv_flow_entt).sim_time = sim_time;
        ctx.get<SimTimePerUpdate>(rcv_flow_entt).sim_time_per_update = LOOKAHEAD_TIME; //1000ns

        // Lock only around shared resource access
        ctx.data().flow_lock.lock();
        ctx.data().recv_flows[flow_event.dst][flow_id] = rcv_flow_entt;
        ctx.data().flow_lock.unlock();


        // 3) snd flow in receiver side (send ACK/NACK/CNP pkt)
        Entity recv_snd_flow_entt = ctx.makeEntity<SndFlow>();

        ctx.get<FlowID>(recv_snd_flow_entt).flow_id = flow_id;
        ctx.get<Src>(recv_snd_flow_entt).src = flow_event.dst;
        ctx.get<Dst>(recv_snd_flow_entt).dst = flow_event.src;
        ctx.get<L4Port>(recv_snd_flow_entt).l4_port = flow_event.l4_port; //FIXME

        ctx.get<FlowSize>(recv_snd_flow_entt).flow_size = 0;
        ctx.get<StartTime>(recv_snd_flow_entt).start_time = flow_event.start_time;
        ctx.get<StopTime>(recv_snd_flow_entt).stop_time = 0;

        hop_idx = flow_id % ctx.data().next_hop_num[flow_event.dst][flow_event.src]; // flow_id % nic_num
        local_nic_idx = ctx.data().fib[flow_event.dst][flow_event.src][hop_idx];
        ctx.get<NIC_ID>(recv_snd_flow_entt).nic_id = local_nic_idx;
        // ctx.get<SndServerID>(recv_snd_flow_entt).snd_server_id = flow_events[i][6];
        // ctx.get<RecvServerID>(recv_snd_flow_entt).recv_server_id = flow_events[i][7];

        ctx.get<SndNxt>(recv_snd_flow_entt).snd_nxt = 0;
        ctx.get<SndUna>(recv_snd_flow_entt).snd_una = 0;
        ctx.get<FlowState>(recv_snd_flow_entt) = FlowState::UNCOMPLETE;

        ctx.get<LastAckTimestamp>(recv_snd_flow_entt).last_ack_timestamp = 0; //1ms
        ctx.get<NxtPktEvent>(recv_snd_flow_entt).nxt_pkt_event = 0;

        ctx.get<PFCState>(recv_snd_flow_entt) = PFCState::RESUME;

        ctx.get<CC_Para>(recv_snd_flow_entt).m_rate = nic_rate;
        ctx.get<CC_Para>(recv_snd_flow_entt).tar_rate = nic_rate;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_Alpha = DCQCN_Alpha;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_G = DCQCN_G;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_AIRate = DCQCN_AIRate;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_HAIRate = DCQCN_HAIRate; 
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_RateDecreaseInterval = DCQCN_RateDecreaseInterval;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_RateIncreaseInterval = DCQCN_RateIncreaseInterval;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_AlphaUpdateInterval = DCQCN_AlphaUpdateInterval;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_IncreaseStageCount = DCQCN_IncreaseStageCount;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_RecoveryStageThreshold = DCQCN_RecoveryStageThreshold;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_RateDecreaseNextTime = DCQCN_RateDecreaseNextTime;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_RateIncreaseNextTime = DCQCN_RateIncreaseNextTime;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_AlphaUpdateNextTime = DCQCN_AlphaUpdateNextTime;
        ctx.get<CC_Para>(recv_snd_flow_entt).dcqcn_RateIsDecreased = DCQCN_RateIsDecreased;
        ctx.get<CC_Para>(recv_snd_flow_entt).CNPState = false;

        // we should set DCTCP parameters
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_ACKedBytes = DCTCP_ACKedBytes;
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_ECNMarkedBytes = DCTCP_ECNMarkedBytes;
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_Alpha = DCTCP_Alpha;
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_G = DCTCP_G;
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_RecvNextNumber = -1;
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_LastUpdateACKNumber = DCTCP_LastUpdateACKNumber;
        ctx.get<CC_Para>(recv_snd_flow_entt).dctcp_RAIRate = DCTCP_RAIRate;

        ctx.get<PktBuf>(recv_snd_flow_entt).head = 0;
        ctx.get<PktBuf>(recv_snd_flow_entt).tail = 0;
        ctx.get<PktBuf>(recv_snd_flow_entt).cur_num = 0;
        ctx.get<PktBuf>(recv_snd_flow_entt).cur_bytes = 0;

        ctx.get<AckPktBuf>(recv_snd_flow_entt).head = 0;
        ctx.get<AckPktBuf>(recv_snd_flow_entt).tail = 0;
        ctx.get<AckPktBuf>(recv_snd_flow_entt).cur_num = 0;
        ctx.get<AckPktBuf>(recv_snd_flow_entt).cur_bytes = 0;

        ctx.get<NICRate>(recv_snd_flow_entt).nic_rate = nic_rate; // 100 Gbps
        ctx.get<HSLinkDelay>(recv_snd_flow_entt).HS_link_delay = HS_LINK_DELAY; // 1 us, 1000 ns 

        // ctx.get<SimTime>(recv_snd_flow_entt).sim_time = flow_event.start_time;
        ctx.get<SimTime>(recv_snd_flow_entt).sim_time = sim_time;
        // ctx.get<SimTime>(recv_snd_flow_entt).sim_time = 0;
        ctx.get<SimTimePerUpdate>(recv_snd_flow_entt).sim_time_per_update = LOOKAHEAD_TIME; //1000ns

        ctx.data().flow_lock.lock();
        ctx.data().recv_snd_flows[flow_event.dst][flow_id] = recv_snd_flow_entt;
        ctx.data().flow_lock.unlock();


        // 4) recv flow in sender side (receive ACK/NACK/CNP pkt)
        Entity snd_recv_flow_entt = ctx.makeEntity<RecvFlow>();

        ctx.get<FlowID>(snd_recv_flow_entt).flow_id = flow_id;
        //
        ctx.get<Src>(snd_recv_flow_entt).src = flow_event.dst;
        ctx.get<Dst>(snd_recv_flow_entt).dst = flow_event.src;
        //
        ctx.get<L4Port>(snd_recv_flow_entt).l4_port = flow_event.l4_port; //FIXME

        ctx.get<FlowSize>(snd_recv_flow_entt).flow_size = 0;
        ctx.get<StartTime>(snd_recv_flow_entt).start_time = flow_event.start_time;
        ctx.get<StopTime>(snd_recv_flow_entt).stop_time = 0;

        ctx.get<NIC_ID>(snd_recv_flow_entt).nic_id = local_nic_idx;

        ctx.get<FlowState>(snd_recv_flow_entt) = FlowState::UNCOMPLETE;

        ctx.get<LastCNPTimestamp>(snd_recv_flow_entt).last_cnp_timestamp = 0; //1ms

        ctx.get<RecvBytes>(snd_recv_flow_entt).recv_bytes = 0;

        ctx.get<PktBuf>(snd_recv_flow_entt).head = 0;
        ctx.get<PktBuf>(snd_recv_flow_entt).tail = 0;
        ctx.get<PktBuf>(snd_recv_flow_entt).cur_num = 0;
        ctx.get<PktBuf>(snd_recv_flow_entt).cur_bytes = 0;

        ctx.get<NICRate>(snd_recv_flow_entt).nic_rate = nic_rate; //  
        ctx.get<HSLinkDelay>(snd_recv_flow_entt).HS_link_delay = HS_LINK_DELAY; // 1 us, 1000 ns 

        ctx.get<SimTime>(snd_recv_flow_entt).sim_time = sim_time;
        ctx.get<SimTimePerUpdate>(snd_recv_flow_entt).sim_time_per_update = LOOKAHEAD_TIME; //1000ns

        // Lock only around shared resource access
        ctx.data().flow_lock.lock();
        ctx.data().snd_recv_flows[flow_event.src][flow_id] = snd_recv_flow_entt;
        ctx.data().flow_lock.unlock();

        #if PRINT_SYS_LOG
        printf("Exiting create_flow for src: %u, dst: %u\n", flow_event.src, flow_event.dst);
        #endif
    }

    // uint64_t flow_id = 0;
    void setFlow(Engine &ctx, uint64_t comm_src, uint64_t comm_dst, uint64_t flow_size, uint64_t flow_id) {

        Entity net_npu = ctx.data()._net_npus[comm_src];
        int64_t sim_time = ctx.get<SimTime>(net_npu).sim_time;
        FlowEvent flow_event = {flow_id, (uint32_t)comm_src, (uint32_t)comm_dst, 1, 0, flow_size, sim_time, 0, FlowState::UNCOMPLETE};

        ctx.data().flow_lock.lock();
        _enqueue_flow(ctx.get<NewFlowQueue>(net_npu), flow_event);
        ctx.data().flow_lock.unlock(); 

        // #if PRINT_SYS_LOG
        printf("sim_time: %ld, set flow id %ld: %ld->%ld %ld, \n", sim_time, flow_id, comm_src, comm_dst, flow_size);
        // #endif
    }

    void comm_set_flow(Engine &ctx, NET_NPU_ID &_net_npu_id,
            NewFlowQueue &_new_flow_queue, SimTime &_sim_time,  
            SimTimePerUpdate &_sim_time_per_update) {
        (void)_new_flow_queue;

        #if PRINT_SYS_LOG
            if (_net_npu_id.net_npu_id == 0) {
                printf("*********Enter into comm_set_flow, net_npu_id: %u, sim_time: %ld, enqueue, len = %d\n", \
                        _net_npu_id.net_npu_id, _sim_time.sim_time, get_queue_len(_new_flow_queue));
            }
        #endif

        if((_sim_time.sim_time != 0)) {return;}
        // if((_sim_time.sim_time % (20LL * 1000) != 0)) {return;}
        // if(_sim_time.sim_time % (30LL * 1000) != 0) {return;}
        

        // scenario: 1 to 2
        // if (_net_npu_id.net_npu_id == 0) {
        //     uint32_t src = _net_npu_id.net_npu_id;
        //     uint32_t dst = 8;
        //     uint64_t new_flow_size = 50LL*1000*1000; //1LL * 500* 1000 * 1000; //500*10000; 
        //     uint64_t flow_id = _net_npu_id.net_npu_id+0;
        //     setFlow(ctx, src, dst, new_flow_size, flow_id);


        //     dst = 128;
        //     new_flow_size = 50LL*1000*1000; //1LL * 500* 1000 * 1000; //500*10000; 
        //     flow_id = _net_npu_id.net_npu_id+1;
        //     setFlow(ctx, src, dst, new_flow_size, flow_id);

        //     // dst = 1;
        //     // new_flow_size = 50LL*1000*1000; //1LL * 500* 1000 * 1000; //500*10000; 
        //     // flow_id = _net_npu_id.net_npu_id+2;
        //     // setFlow(ctx, src, dst, new_flow_size, flow_id);

        // }

        // // scenario: 2 to 1
        // if (_net_npu_id.net_npu_id == 0 || _net_npu_id.net_npu_id == 1) {
        //     uint32_t src = _net_npu_id.net_npu_id;
        //     uint32_t dst = 12;
        //     uint64_t new_flow_size = 50LL*1000*1000; //1LL * 500* 1000 * 1000; //500*10000; 
        //     uint64_t flow_id = _net_npu_id.net_npu_id+0;
        //     setFlow(ctx, src, dst, new_flow_size, flow_id);
        // }

        // if (_net_npu_id.net_npu_id == 1016) {
        //     uint32_t src = _net_npu_id.net_npu_id;
        //     uint32_t dst = 0;
        //     uint64_t new_flow_size = 50LL*1000*1000; //1LL * 500* 1000 * 1000; //500*10000; 
        //     uint64_t flow_id = _net_npu_id.net_npu_id+2;
        //     setFlow(ctx, src, dst, new_flow_size, flow_id);
        // }

        // if (_net_npu_id.net_npu_id == (1024-128)) {
        //     uint32_t src = _net_npu_id.net_npu_id;
        //     uint32_t dst = 0;
        //     uint64_t new_flow_size = 50LL*1000*1000; //1LL * 500* 1000 * 1000; //500*10000; 
        //     uint64_t flow_id = _net_npu_id.net_npu_id+3;
        //     setFlow(ctx, src, dst, new_flow_size, flow_id);

        // }


        // ring allreduce
        if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
            // if (_net_npu_id.net_npu_id == 0) {
            //     printf("Entering test case 2\n");
            // }
            uint32_t src = _net_npu_id.net_npu_id;
            uint32_t dst = (_net_npu_id.net_npu_id+1)%(ctx.data().num_net_npu);
            uint64_t flow_size = 2 * 1000LL * 1000; //5LL * 1000 * 1000;
            uint64_t flow_id = _net_npu_id.net_npu_id;
            setFlow(ctx, src, dst, flow_size, flow_id);
        }

        // 3D parallelism
        // if (_net_npu_id.net_npu_id >= 130  &&  _net_npu_id.net_npu_id <= 134) {
        // if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
        //     if (_net_npu_id.net_npu_id == 0) {
        //         printf("Entering test case: 3D parallelism\n");
        //     }
        //     uint32_t src = _net_npu_id.net_npu_id;

        //     uint32_t dst = (_net_npu_id.net_npu_id+72)%(ctx.data().num_net_npu); // 72 is the size of data parallel group
        //     uint64_t flow_size = 200 * 1000LL * 1000; //5LL * 1000 * 1000;
        //     uint64_t flow_id = _net_npu_id.net_npu_id;
        //     setFlow(ctx, src, dst, flow_size, flow_id);
        // }

        //if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
        //    uint32_t src = _net_npu_id.net_npu_id;
        //    uint32_t dst = (_net_npu_id.net_npu_id+1)%(ctx.data().num_net_npu);
        //    uint64_t flow_size = 5LL * 1000 * 1000;
        //    uint64_t flow_id = _net_npu_id.net_npu_id;
        //    setFlow(ctx, src, dst, flow_size, flow_id);
        //}


        // // all to all (EP)
        // uint32_t batch_size = 32;
        // uint32_t token_size = 7*1024;
        // uint32_t top_k = 8;
        // uint64_t new_flow_size = batch_size * token_size * top_k / ctx.data().num_net_npu; // Unit is Byte
        // uint64_t new_flow_size = 2LL * 1000; // Unit is Byte
        // if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
        //     for (uint32_t dst = 0; dst < ctx.data().num_net_npu; dst++) {
        //         // if (dst >= ctx.data().max_flow_num) {break;}

        //         if (dst == _net_npu_id.net_npu_id) {continue;}

        //         uint32_t src = _net_npu_id.net_npu_id;
        //         uint64_t flow_id = _net_npu_id.net_npu_id*4096+dst;
        //         setFlow(ctx, src, dst, new_flow_size, flow_id);
        //     }
        // }

        #if PRINT_SYS_LOG
        if (_net_npu_id.net_npu_id == 0) {  
            printf("Exiting comm_set_flow for net_npu_id: %u\n", _net_npu_id.net_npu_id);
        }
        #endif
    }

    void setup_flow(Engine &ctx, NET_NPU_ID &_net_npu_id,
            NewFlowQueue &_new_flow_queue, SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update) {

        uint32_t flow_event_num = get_queue_len(_new_flow_queue);
        for (uint32_t i = 0; i < flow_event_num; i++) {
            FlowEvent flow_event;
            _dequeue_flow(_new_flow_queue, flow_event);


            // Lock only around shared resource access
            create_flow(ctx, flow_event, flow_event.flow_id, _sim_time.sim_time);
        }

        clear_queue(_new_flow_queue);
        _sim_time.sim_time += _sim_time_per_update.sim_time_per_update;

    }

    void flow_send(Engine &ctx, FlowID &_flow_id, Src &_src, Dst &_dst, L4Port &_l4_port, 
            FlowSize &_flow_size, StartTime &_start_time, StopTime &_stop_time,
            NIC_ID &_nic_id, SndServerID &_snd_server_id, RecvServerID &_recv_server_id,
            SndNxt &_snd_nxt, SndUna &_snd_una, FlowState &_flow_state, 
            LastAckTimestamp &_last_ack_timestamp, NxtPktEvent &_nxt_pkt_event, 
            PFCState &_pfc_state, CC_Para &_cc_para, 
            PktBuf &_snd_buf, AckPktBuf &_ack_buf, 
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay, 
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update) {
        (void)ctx;
        (void)_nic_id;
        (void)_snd_server_id;
        (void)_recv_server_id;
        (void)_HS_link_delay;
        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;
        #if PRINT_SYS_LOG
            // if (_flow_id.flow_id == 0 || _flow_id.flow_id == 1) {
            printf("\n*******Enter into flow send*********\n");
            printf("src: %d, dst: %d, flow_id: %lu, start _sim_time.sim_time: %ld, end_time: %ld\n", _src.src, _dst.dst, _flow_id.flow_id, _sim_time.sim_time, end_time);
            // printf("end_time: %ld\n", end_time);
            printf("cc target rate: %.3lf, cc cur rate: %.3lf\n", _cc_para.tar_rate/(1000*1000*1000.0), _cc_para.m_rate/(1000*1000*1000.0));
            // }
        #endif

        if (_sim_time.sim_time == _start_time.start_time) {
            clear_queue(_snd_buf);
            clear_queue(_ack_buf);
        }

        // uint16_t flow_id = 0;
        uint16_t header_len = 40; 
        uint8_t flow_priority = 0;
        uint32_t path[MAX_PATH_LEN];
        memset(path, 0, MAX_PATH_LEN*sizeof(uint32_t));
        Pkt pkt; 

        int counter=0;

        while(true){
            counter++;
            int event_type = -1;
            int64_t next_event_time = INF;

            // deal with received packet event
            Pkt ack_p;
            if(fetch_elem(_ack_buf, 0, ack_p)){
                if(ack_p.enqueue_time < next_event_time){
                    next_event_time = ack_p.enqueue_time;
                    event_type = 1;
                }
            }

            // deal with send packet event
            if(_nxt_pkt_event.nxt_pkt_event < next_event_time){
                next_event_time = _nxt_pkt_event.nxt_pkt_event;
                event_type = 2;
            }

            // deal with timer event
            // DCQCN alpha update timer
            if(_cc_para.dcqcn_AlphaUpdateNextTime < next_event_time && _cc_para.current_algo == CCAlgo::DCQCN){
                next_event_time = _cc_para.dcqcn_AlphaUpdateNextTime;
                event_type = 3;
            }
            // DCQCN rate increase timer
            if(_cc_para.dcqcn_RateIncreaseNextTime < next_event_time && _cc_para.current_algo == CCAlgo::DCQCN){
                next_event_time = _cc_para.dcqcn_RateIncreaseNextTime;
                event_type = 4;
            }
            // flow start event
            if (_start_time.start_time < next_event_time && _start_time.started == false) {
                next_event_time = _start_time.start_time;
                event_type = 5;
            }
            // retransmit timer
            if (_start_time.started && _last_ack_timestamp.last_ack_timestamp + RETRANSMIT_TIMER < next_event_time){
                next_event_time = _last_ack_timestamp.last_ack_timestamp + RETRANSMIT_TIMER;
                event_type = 6;
            }



            // if there is no event or next event time is larger than end of frame time, break the while loop
            // if flow size == 0, we are in the sender of receivers
            if(event_type == -1 || next_event_time > end_time || _flow_size.flow_size == 0){
                _sim_time.sim_time = end_time;
                break;
            }

             if(next_event_time < _sim_time.sim_time && event_type != 3) {
                 printf("!!! wrong time order: next_event_time: %ld, _sim_time.sim_time: %ld event_type: %d\n", next_event_time, _sim_time.sim_time, event_type);
                 printf("input parameters: _src.src: %u, _dst.dst: %u, _flow_id.flow_id: %lu, _flow_size.flow_size: %lu, _start_time.start_time: %ld, _stop_time.stop_time: %ld\n", _src.src, _dst.dst, _flow_id.flow_id, _flow_size.flow_size, _start_time.start_time, _stop_time.stop_time);   
                 // PrintPkt(ack_p, "ACK received by sender in flow send: ");
                 //assert(false);
            }
            _sim_time.sim_time = next_event_time > _sim_time.sim_time ? next_event_time : _sim_time.sim_time;

            //if(pkt.flow_id == 31) {
            //    printf("event type: %d,sim_time: %ld, input parameters: _src.src: %u, _dst.dst: %u, _flow_id.flow_id: %lu, _flow_size.flow_size: %lu, _start_time.start_time: %ld, _stop_time.stop_time: %ld\n", event_type, _sim_time.sim_time, _src.src, _dst.dst, _flow_id.flow_id, _flow_size.flow_size, _start_time.start_time, _stop_time.stop_time);
            //}

            if(event_type == 1){
                // new arrival of non-data(ACK/NACK/CNP) packet
                uint16_t ack_pkt_cnt = 0;

                _dequeue(_ack_buf, ack_p);

                if (ack_p.dst == _src.src) {// reaction for ACK/NACK packets
                    if (ack_p.pkt_type == PktType::ACK) { // ACK
                        _snd_una.snd_una = ack_p.sq_num;

                        if(_cc_para.current_algo == CCAlgo::DCTCP){
                            // if the first ack reset parameters
                            if (_cc_para.dctcp_LastUpdateACKNumber == -1){
                                _cc_para.dctcp_LastUpdateACKNumber = _snd_nxt.snd_nxt;
                                _cc_para.dctcp_ECNMarkedBytes = 0;
                                _cc_para.dctcp_ACKedBytes = 0;
                            }
                            // do alpha update for full RTT feed back
                            if(ack_p.sq_num > _cc_para.dctcp_LastUpdateACKNumber){
                               // this is no the ECN packet in this full feedback, we can increase the sending rate
                               if (_cc_para.dctcp_ECNMarkedBytes == 0){
                                       _cc_para.m_rate += _cc_para.dctcp_RAIRate;
                                       // we should limit the sending rate,
                                       if (_cc_para.m_rate > NIC_RATE){
                                           _cc_para.m_rate = NIC_RATE;
                                       }
                                       #if PRINT_CC_LOG
                                       printf("DCTCP increase rate to %ld \n",_cc_para.m_rate);
                                       #endif
                               }

                               double ECNMarkedPercent = _cc_para.dctcp_ECNMarkedBytes * 1.0 / _cc_para.dctcp_ACKedBytes;
                               _cc_para.dctcp_Alpha = (1.0 - _cc_para.dctcp_G) * _cc_para.dctcp_Alpha + _cc_para.dctcp_G * ECNMarkedPercent;


                               //reset parameter
                               _cc_para.dctcp_LastUpdateACKNumber = _snd_nxt.snd_nxt;
                               _cc_para.dctcp_ECNMarkedBytes = 0;
                               _cc_para.dctcp_ACKedBytes = 0;
                            }
                            _cc_para.dctcp_ACKedBytes += 1;
                        }
                        if (static_cast<uint64_t>(_snd_una.snd_una) >= _flow_size.flow_size) {
                            if (_flow_size.flow_size > 0 && _flow_state == FlowState::UNCOMPLETE) {
                                _sim_time.sim_time = _sim_time.sim_time > ack_p.enqueue_time ? _sim_time.sim_time : ack_p.enqueue_time;
                                _stop_time.stop_time = _sim_time.sim_time;
                                // #if PRINT_SEND_RECV_PKT_LOG
                                printf("Flow is finished: flow sender id: %d, flow receiver id: %d, flow id is %lu, flow size: %lu, start_time: %ld, stop_time: %ld\n",\
                                        _src.src, _dst.dst, _flow_id.flow_id, _flow_size.flow_size, _start_time.start_time, _stop_time.stop_time);
                                // #endif
                                    }
                            _flow_state = FlowState::COMPLETE;
                            break;
                        }
                        _last_ack_timestamp.last_ack_timestamp = _sim_time.sim_time;
                    }
                    else if (ack_p.pkt_type == PktType::NACK) { // NACK
                        _snd_nxt.snd_nxt = ack_p.sq_num;
                        _snd_una.snd_una = ack_p.sq_num;

                        // we received NACK, based on go back N method, we should restart packet transmit
                        _nxt_pkt_event.nxt_pkt_event = _sim_time.sim_time;

                        _last_ack_timestamp.last_ack_timestamp = _sim_time.sim_time;
                    }
                    else if (ack_p.pkt_type == PktType::SACK){
                        uint16_t payload_len = MTU-header_len;
                        uint16_t pkt_size = payload_len+header_len;
                        PktType pkt_type = PktType::DATA;
                        if(PKT_SPRAYING_PROTOCOL_ON){
                            uint64_t bitmap_start_offset = ack_p.bitmap_start_offset;
                            uint64_t bitmap_value = ack_p.bitmap_value;
                            printf("bitmap_value: %llu\n", bitmap_value);
                            int highest_bit = -1;
                            for(int i = 63; i >= 0; i--){
                                if((bitmap_value & (1ULL << i)) != 0){
                                    highest_bit = i;
                                    break;
                                }
                            }
                            for(uint8_t i = 0; i <= highest_bit; i++){
                                if((bitmap_value & (1ULL << i)) != 0){
                                    continue;  // already received
                                }
                                uint64_t sq_num = bitmap_start_offset + i * (MTU-header_len);
                                Pkt resend_pkt;
                                int64_t dequeue_time = _sim_time.sim_time + (pkt_size*8*1.0/_cc_para.line_rate)*(1000*1000*1000); // unit is ns
                                int64_t enqueue_time = dequeue_time;
                                int32_t cid = (sq_num / payload_len / PSP_CONTAINER_LEN) % PSP_PATH_NUM;
                                int32_t csn = (sq_num / payload_len / (PSP_CONTAINER_LEN * PSP_PATH_NUM)) * PSP_CONTAINER_LEN + (sq_num /payload_len)% PSP_CONTAINER_LEN;
                                create_pkt(resend_pkt, _src.src, _dst.dst, _l4_port.l4_port, header_len, payload_len, pkt_size, sq_num, _flow_id.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::NO, cid, csn);
                                _enqueue(_snd_buf, resend_pkt);
                                #if PRINT_SEND_RECV_PKT_LOG
                                PrintPkt(resend_pkt, "RESEND SACK DATA in flow send side");
                                #endif
                                _sim_time.sim_time = _sim_time.sim_time > dequeue_time ? _sim_time.sim_time : dequeue_time;
                            }
                        }
                        else{
                            uint64_t sq_num = ack_p.sq_num;
                            uint64_t snd_nxt = ack_p.sq_num;
                            for(uint8_t i = 0; i < ack_p.block_num; i++){
                                if(ack_p.sack_block[2*i] == 0 && ack_p.sack_block[2*i+1] == 0) break;
                                if(ack_p.sack_block[2*i] > ack_p.sack_block[2*i+1]) break;
                                //TO_DO: All lost packets should not be sent at once
                                while (snd_nxt < ack_p.sack_block[2*i]) {
                                    Pkt resend_pkt;
                                    int64_t dequeue_time = _sim_time.sim_time + (pkt_size*8*1.0/PCIE_BW)*(1000*1000*1000); // unit is ns
                                    int64_t enqueue_time = dequeue_time;
                                    sq_num = snd_nxt;
                                    create_pkt(resend_pkt, _src.src, _dst.dst, _l4_port.l4_port, header_len, payload_len, pkt_size, sq_num, _flow_id.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::NO);
                                    snd_nxt += payload_len;
                                    _enqueue(_snd_buf, resend_pkt);
                                    #if PRINT_SEND_RECV_PKT_LOG
                                    PrintPkt(resend_pkt, "RESEND SACK DATA in flow send side");
                                    #endif
                                    _sim_time.sim_time = _sim_time.sim_time > dequeue_time ? _sim_time.sim_time : dequeue_time;
                                }
                                snd_nxt = ack_p.sack_block[2*i+1];
                            }
                        }
                    }
                    else if (ack_p.pkt_type == PktType::CNP){ // CNP
                        #if PRINT_PKT_LOG
                        PrintPkt(ack_p, "Received CNP in flow send side");
                        #endif
                        if(_cc_para.current_algo == CCAlgo::DCQCN){
                            _cc_para.CNPState = true;
                            dcqcn_rate_decrease(_cc_para, ack_p.dst, ack_p.src, ack_p.flow_id, _sim_time.sim_time);
                            dcqcn_alpha_update(_cc_para);
                            _cc_para.CNPState = false;
                            //record the rate is decreased
                            //use it to determine whether up
                            _cc_para.dcqcn_RateIsDecreased = true;
                        }
                        // logic for DCTCP
                        else if(_cc_para.current_algo == CCAlgo::DCTCP){
                            // we are in the first CNP pakcet in this RTT interval
                            if(_cc_para.dctcp_ECNMarkedBytes == 0){
                                    _cc_para.m_rate = _cc_para.m_rate* (1.0 - _cc_para.dctcp_Alpha / 2.0);
                                    #if PRINT_CC_LOG
                                    printf("DCTCP decrease rate to %ld\n", _cc_para.m_rate); 
                                    #endif
                            }
                            _cc_para.dctcp_ECNMarkedBytes += 1;
                        }
                    }
                    else { //PFC pause/resume packet 
                        if (ack_p.pkt_type == PktType::PFC_PAUSE) {
                            _pfc_state = PFCState::PAUSE;
                        }
                        else {
                            _pfc_state = PFCState::RESUME;
                        }
                    }

                    ack_pkt_cnt += 1;

                }
            }
            // send packet event
            if(event_type ==2){
                if (_pfc_state == PFCState::PAUSE) {
                    printf("In send phase: PFCState::PAUSE\n");
                    continue;
                }

                // if the snd_buf is full, we should delay the next packet event time 
                if(get_queue_len(_snd_buf) >= PKT_BUF_LEN-1) {
                    _nxt_pkt_event.nxt_pkt_event = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;
                    continue;
                }
                if (_flow_state == FlowState::UNCOMPLETE && static_cast<uint64_t>(_snd_nxt.snd_nxt) < _flow_size.flow_size && _pfc_state == PFCState::RESUME) { // the flow is unfinished
                                                                                                                                                            // flow is not finished
                    uint64_t remain_data_len = (_flow_size.flow_size -_snd_nxt.snd_nxt);


                    int64_t payload_len = remain_data_len;
                    if (remain_data_len > (MTU-header_len)) {   
                        payload_len = MTU-header_len;
                    }
                    int64_t pkt_size = payload_len+header_len;
                    // simulate the bus to transmit(packet moved from transport buffer to NIC buffer)
                    int64_t send_interval = (pkt_size*8*1.0)/(_cc_para.line_rate*1.0/(1000*1000*1000));

                    int64_t dequeue_time = _sim_time.sim_time + send_interval; // unit is ns
                    int64_t enqueue_time = dequeue_time;

                    PktType pkt_type = PktType::DATA;

                    int64_t sq_num = _snd_nxt.snd_nxt;


                    if(PKT_SPRAYING_PROTOCOL_ON){
                        uint16_t pkt_data_len = MTU - header_len;
                        int32_t cid = (sq_num / pkt_data_len / PSP_CONTAINER_LEN) % PSP_PATH_NUM;
                        int32_t csn = (sq_num / pkt_data_len / (PSP_CONTAINER_LEN * PSP_PATH_NUM)) * PSP_CONTAINER_LEN + (sq_num /pkt_data_len)% PSP_CONTAINER_LEN;
                        create_pkt(pkt, _src.src, _dst.dst, _l4_port.l4_port, header_len, payload_len, pkt_size, sq_num, _flow_id.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::NO, cid, csn);
                    }
                    else {
                        create_pkt(pkt, _src.src, _dst.dst, _l4_port.l4_port, header_len, payload_len, pkt_size, sq_num, _flow_id.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::NO);
                    }
                    
                    // if(pkt.flow_id == 31) {
                    //     PrintPkt(pkt, "flow send by sender: ");
                    // }
                    #if PRINT_SEND_RECV_PKT_LOG
                        PrintPkt(pkt, "flow send by sender: ");
                    #endif

                    _enqueue(_snd_buf, pkt);
                    
                    _snd_nxt.snd_nxt += payload_len;
                    // nxt_pkt_event must be >= dequeue_time, because cur_rate(m_rate) must be <= nic_rate;
                    if (static_cast<uint64_t>(_snd_nxt.snd_nxt) < _flow_size.flow_size){
                        // if there are remained some bytes, we still need to send data packet
                        // _cc_para.m_rate = _cc_para.m_rate < 0.01 ? 0.01 : _cc_para.m_rate;
                        _nxt_pkt_event.nxt_pkt_event = _sim_time.sim_time + (pkt_size*8*1.0)/(_cc_para.m_rate*1.0/(1000*1000*1000));
                    }
                    else{
                        // there is no more bytes to send, set next packet time as INF;
                        _nxt_pkt_event.nxt_pkt_event = INF;
                    }

                    //_sim_time.sim_time = _sim_time.sim_time > dequeue_time ? _sim_time.sim_time : dequeue_time;
                }
            }

            // DCQCN timer function
            // the order should be: check if decrease, update alpha, increase send rate
            //dcqcn_timer_reset_if_decrease(_cc_para, _sim_time.sim_time);

            if(event_type == 3){
                dcqcn_timer_alpha_update(_cc_para, _sim_time.sim_time);
            }

            if(event_type == 4){
                dcqcn_timer_rate_increase(_cc_para, _src.src, _dst.dst, _flow_id.flow_id, _sim_time.sim_time);
                if (_nxt_pkt_event.nxt_pkt_event != INF) {
                    _nxt_pkt_event.nxt_pkt_event = _sim_time.sim_time + 10;
                }
            }

            if(event_type == 5){
                // the beggining of flow
                // TODO: we should set the next packet time and other dcqcn timer, to start sending flow
                if (_snd_nxt.snd_nxt == 0) { 
                    _nxt_pkt_event.nxt_pkt_event = _start_time.start_time+10;
                    _cc_para.dcqcn_AlphaUpdateNextTime = _start_time.start_time +  DCQCN_AlphaUpdateInterval;
                    _cc_para.dcqcn_RateIncreaseNextTime = _start_time.start_time +  DCQCN_RateIncreaseInterval;
                    _last_ack_timestamp.last_ack_timestamp = _start_time.start_time;
                }
                // since we start flow, we need to reset the flow start time
                _start_time.started = true;
                clear_queue(_snd_buf);
                clear_queue(_ack_buf);
            }

            if(event_type == 6){
                if ((_sim_time.sim_time - _last_ack_timestamp.last_ack_timestamp) >= RETRANSMIT_TIMER) {
                    _snd_nxt.snd_nxt = _snd_una.snd_una;
                    _nxt_pkt_event.nxt_pkt_event = _sim_time.sim_time;
                    _last_ack_timestamp.last_ack_timestamp = _sim_time.sim_time;
                    //TODO: we should set the next packet
                }
            }
        }
    }

    // receive system: 
    // 1) generate ACK/NACK/CNP; 
    // 2) or move them into the ack buffer of receiver's sender 
    void flow_receive(Engine &ctx, FlowID &_flow_id, Src &_src, Dst &_dst,PktBuf &_recv_queue,
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay, 
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update,
            RecvBytes &_recv_bytes, RecvBitmapAndTriggerCondition &_bitmap_and_trigger, L4Port &_l4_port,
            LastCNPTimestamp &_last_cnp_timestamp, StartTime &_start_time) {
        (void)_flow_id;
        (void)_HS_link_delay;

        if (_start_time.start_time == _sim_time.sim_time) {
            clear_queue(_recv_queue);
        }

        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;
        #if PRINT_SYS_LOG
            printf("\n*******Enter into flow receive*********\n");
            printf("start _sim_time.sim_time: %ld, end_time: %ld\n", _sim_time.sim_time, end_time);
        #endif

        Pkt pkt;
        uint8_t flow_priority = 0;
        uint16_t header_len = 40;

        while (_sim_time.sim_time < end_time) {

            if (PKT_SPRAYING_PROTOCOL_ON && _sim_time.sim_time >= _bitmap_and_trigger.bubble_timer && _bitmap_and_trigger.cache_hole_depth < MAX_CACHE_HOLE_DEPTH && _bitmap_and_trigger.cache_hole_depth > 0){
                Pkt sack_pkt;
                PktType pkt_type = PktType::SACK;
                int64_t dequeue_time = _sim_time.sim_time + 1;
                int64_t enqueue_time = dequeue_time;
                uint32_t path[MAX_PATH_LEN];
                uint32_t src = _dst.dst;
                uint32_t dst = _src.src;
                uint64_t flow_id = _flow_id.flow_id;
                memset(path, 0, MAX_PATH_LEN*sizeof(uint32_t));
                create_pkt(sack_pkt, src, dst, _l4_port.l4_port, header_len, 0, header_len, -1, flow_id, enqueue_time, dequeue_time, flow_priority, 
                    pkt_type, path, 0, ECN_MARK::NO); 
                sack_pkt.bitmap_start_offset = (_bitmap_and_trigger.bitmap[_bitmap_and_trigger.head] & 0x7FFFFFFF);
                sack_pkt.bitmap_value = _bitmap_and_trigger.bitmap_value;
                Entity recv_snd_flow_entt = ctx.data().recv_snd_flows[src][flow_id];
                if (recv_snd_flow_entt == Entity::none()) {return;}
                _enqueue(ctx.get<PktBuf>(recv_snd_flow_entt), sack_pkt);
                #if PRINT_SEND_RECV_PKT_LOG
                    PrintPkt(sack_pkt, "SACK generated by receiver's bubble_timer in pkt spraying protocol: ");
                #endif
                _bitmap_and_trigger.bubble_timer += _sim_time.sim_time + MAX_BUBBLE_TIMER;
            } 

            if(!fetch_elem(_recv_queue, 0, pkt)) break;

            if(pkt.enqueue_time >= end_time) break;
            _dequeue(_recv_queue, pkt);
            //PrintPkt(pkt, "ACK/NACK received by sender's receiver: ");
            

            #if ENABLE_PKT_PATH_RECORD
            #if PRINT_PKT_PATH_LOG
                PrintPath(pkt.path, pkt.path_len);
            #endif
            #endif


            if (pkt.pkt_type != PktType::DATA) { //ACK/NACK is received by sender's receiver
                Entity snd_flow_entt = ctx.data().snd_flows[pkt.dst][pkt.flow_id];
                if (snd_flow_entt == Entity::none()) {return;}

                _enqueue(ctx.get<AckPktBuf>(snd_flow_entt), pkt); //enqueue into the ack_pkt_buf

                #if PRINT_SEND_RECV_PKT_LOG
                    PrintPkt(pkt, "ACK/NACK/SACK received by sender's receiver: ");
                #endif
            }
            else { //DATA pkt is received by receiver
                uint16_t pkt_size = pkt.header_len + pkt.payload_len;

                _sim_time.sim_time = (_sim_time.sim_time > pkt.enqueue_time ? _sim_time.sim_time : pkt.enqueue_time)  +  (pkt_size*8*1.0/_nic_rate.nic_rate)*(1000*1000*1000);

                #if PRINT_SEND_RECV_PKT_LOG
                    PrintPkt(pkt, "DATA received by receiver: ");
                #endif
                uint32_t path[MAX_PATH_LEN];
                memset(path, 0, MAX_PATH_LEN*sizeof(uint32_t));


                Pkt ack_pkt;
                int64_t sq_num;

                uint32_t ack_src = pkt.dst; 
                uint32_t ack_dst = pkt.src; 

                int64_t dequeue_time = _sim_time.sim_time + 1;
                int64_t enqueue_time = dequeue_time;
                ack_pkt.block_num = 0;  
                memset(ack_pkt.sack_block, 0, sizeof(ack_pkt.sack_block));
                _sim_time.sim_time = _sim_time.sim_time > pkt.dequeue_time ? _sim_time.sim_time : pkt.dequeue_time;

                if(PKT_SPRAYING_PROTOCOL_ON){
                    update_bitmap(_bitmap_and_trigger, pkt);
                    // printf("update_bitmap done: cache_hole_depth: %u, bitmap_value: %lu, _bitmap_and_trigger.tail: %u, _bitmap_and_trigger.head: %u\n", _bitmap_and_trigger.cache_hole_depth, _bitmap_and_trigger.bitmap_value, _bitmap_and_trigger.tail, _bitmap_and_trigger.head);
                    if(_bitmap_and_trigger.cache_hole_depth >= MAX_CACHE_HOLE_DEPTH && _sim_time.sim_time >= _bitmap_and_trigger.depth_interval_timer){
                        Pkt sack_pkt;
                        PktType pkt_type = PktType::SACK;
                        create_pkt(sack_pkt, ack_src, ack_dst, _l4_port.l4_port, header_len, 0, header_len, -1, pkt.flow_id, enqueue_time, dequeue_time, flow_priority, 
                            pkt_type, path, 0, ECN_MARK::NO); 
                        sack_pkt.bitmap_start_offset = (_bitmap_and_trigger.bitmap[_bitmap_and_trigger.head] & 0x7FFFFFFF);
                        sack_pkt.bitmap_value = _bitmap_and_trigger.bitmap_value;
                        Entity recv_snd_flow_entt = ctx.data().recv_snd_flows[ack_src][pkt.flow_id];
                        if (recv_snd_flow_entt == Entity::none()) {return;}
                        _enqueue(ctx.get<PktBuf>(recv_snd_flow_entt), sack_pkt);
                        #if PRINT_SEND_RECV_PKT_LOG
                            PrintPkt(sack_pkt, "SACK generated by receiver's interval_timer in pkt spraying protocol: ");
                        #endif 
                        _bitmap_and_trigger.depth_interval_timer += _sim_time.sim_time + MAX_DEPTH_INTERVAL_TIMER;
                        _bitmap_and_trigger.bubble_timer += _sim_time.sim_time + MAX_BUBBLE_TIMER;
                    }
                    
                    if (pkt.sq_num < _recv_bytes.recv_bytes) {
                        // if receive old packet, drop it
                        continue;
                    } 
                    else if (pkt.sq_num == _recv_bytes.recv_bytes){
                        _recv_bytes.recv_bytes += pkt.payload_len;
                        sq_num = _recv_bytes.recv_bytes;
                        PktType pkt_type = PktType::ACK;
                        int bitmap_pos = pkt.sq_num / (MTU-pkt.header_len) % PKT_BUF_LEN;
                        for(int index = (bitmap_pos + 1) % PKT_BUF_LEN; index <= _bitmap_and_trigger.tail;  index =  (index + 1) % PKT_BUF_LEN){
                            if(((_bitmap_and_trigger.bitmap[index]& 0x80000000)>>31) == 1){
                                _recv_bytes.recv_bytes += (MTU-pkt.header_len);
                                sq_num = _recv_bytes.recv_bytes;
                            }
                            else{
                                break;
                            }
                        }
                        // simulate the latency of network path for ACK packet 
                        enqueue_time = enqueue_time + CTRL_PKT_NET_PATH_LATENCY;
                        dequeue_time = enqueue_time;
                        // we should set the ecn mark of ack packet to the same as the data packet
                        create_pkt(ack_pkt, ack_src, ack_dst, \
                                _l4_port.l4_port, header_len, 0, header_len, sq_num, \
                                pkt.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, pkt.ecn);
                        
                        Entity snd_recv_flow_entt = ctx.data().snd_recv_flows[ack_dst][pkt.flow_id];
                        if (snd_recv_flow_entt == Entity::none()) {return;}
                        _enqueue(ctx.get<PktBuf>(snd_recv_flow_entt), ack_pkt);
                        
                        #if PRINT_SEND_RECV_PKT_LOG
                            PrintPkt(ack_pkt, "ACK generated by receiver in pkt spraying protocol: ");
                        #endif
                    }

                }
                else{

                    if(SACK_ON){
                        update_bitmap(_bitmap_and_trigger, pkt);
                    }
                    if (pkt.sq_num < _recv_bytes.recv_bytes) {
                        // if receive old packet, drop it
                        continue;
                    }
                    else if (pkt.sq_num == _recv_bytes.recv_bytes) { // packet order is right: generate ACK
                        _recv_bytes.recv_bytes += pkt.payload_len;
                        sq_num = _recv_bytes.recv_bytes;
                        PktType pkt_type = PktType::ACK;
                        if(SACK_ON){
                            int bitmap_pos = pkt.sq_num / (MTU-pkt.header_len) % PKT_BUF_LEN;
                            for(int index = (bitmap_pos + 1) % PKT_BUF_LEN; index <= _bitmap_and_trigger.tail;  index =  (index + 1) % PKT_BUF_LEN){
                                if(((_bitmap_and_trigger.bitmap[index]& 0x80000000)>>31) == 1){
                                    _recv_bytes.recv_bytes += (MTU-pkt.header_len);
                                    sq_num = _recv_bytes.recv_bytes;
                                }
                                else{
                                    break;
                                }
                            }
                        }

                        // simulate the latency of network path for ACK packet 
                        // enqueue_time = _sim_time.sim_time + CTRL_PKT_NET_PATH_LATENCY;
                        // dequeue_time = enqueue_time;
                        // we should set the ecn mark of ack packet to the same as the data packet
                        create_pkt(ack_pkt, ack_src, ack_dst, \
                                _l4_port.l4_port, header_len, 0, header_len, sq_num, \
                                pkt.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, pkt.ecn);

                        // if the packets belongs to scale up network, direct send ack to sender's sender/receiver entt, bypassing network
                        Entity recv_snd_flow_entt = Entity::none();
                        ack_pkt.dequeue_time = dequeue_time + CTRL_PKT_NET_PATH_LATENCY;
                        ack_pkt.enqueue_time = ack_pkt.dequeue_time;

                        Entity snd_flow_entt = ctx.data().snd_flows[ack_dst][pkt.flow_id];
                        if (snd_flow_entt == Entity::none()) {return;}

                        _enqueue(ctx.get<AckPktBuf>(snd_flow_entt), ack_pkt);
                        

                        
                        #if PRINT_SEND_RECV_PKT_LOG
                            PrintPkt(ack_pkt, "ACK generated by receiver(PSP OFF): ");
                        #endif
                    }
                    else { // packets is out of order
                        if(SACK_ON) { // packets is out of order, use SACK to resend pkt 
                            sq_num = _recv_bytes.recv_bytes;
                            PktType pkt_type = PktType::SACK;
                            uint8_t block_num_max = 4;
                            uint16_t bipmap_length = _bitmap_and_trigger.tail - _bitmap_and_trigger.head + 1;
                            uint16_t i = 0;
                            bool fisrtFieldFound = false;
                            while (i < bipmap_length){
                               if(((_bitmap_and_trigger.bitmap[_bitmap_and_trigger.head +i] & 0x80000000)>>31) == 1){
                                    uint64_t left_edge = _bitmap_and_trigger.bitmap[_bitmap_and_trigger.head + i] & 0x7FFFFFFF;
                                    while (i < bipmap_length && (_bitmap_and_trigger.bitmap[_bitmap_and_trigger.head + i] & 0x80000000)>>31 == 1) {
                                        i++;
                                    }
                                    uint64_t right_edge = (_bitmap_and_trigger.bitmap[_bitmap_and_trigger.head + i - 1] & 0x7FFFFFFF) + (uint64_t)(MTU - pkt.header_len);
                                    if(fisrtFieldFound && block_num_max > 0){
                                        ack_pkt.block_num++;
                                        ack_pkt.sack_block[2*(4-block_num_max)] = left_edge;
                                        ack_pkt.sack_block[2*(4-block_num_max) + 1] = right_edge;
                                        block_num_max--;
                                    } else {
                                        fisrtFieldFound = true;
                                    }
                                    if(block_num_max == 0){
                                        break;
                                    }
                               }else{
                                    i++;
                               }
                            }
                            create_pkt(ack_pkt, ack_src, ack_dst, \
                                _l4_port.l4_port, header_len, 0, header_len, sq_num, \
                                pkt.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::NO); 
                            Entity recv_snd_flow_entt = ctx.data().recv_snd_flows[ack_src][pkt.flow_id];
                            if (recv_snd_flow_entt == Entity::none()) {return;}
                            _enqueue(ctx.get<PktBuf>(recv_snd_flow_entt), ack_pkt);
                            #if PRINT_SEND_RECV_PKT_LOG
                                PrintPkt(ack_pkt, "SACK generated by receiver: ");
                            #endif
                        } else { // packets is out of order, use NACK to resend pkt
                            sq_num = _recv_bytes.recv_bytes;
                            PktType pkt_type = PktType::NACK;

                            create_pkt(ack_pkt, ack_src, ack_dst, \
                                    _l4_port.l4_port, header_len, 0, header_len, sq_num, \
                                    pkt.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::NO);
                            
                            // // directly enqueue NACK into sender's snd_recv_flow, bypassing the nework path
                            // // simulate the latency of network path for NACK packet  
                            ack_pkt.dequeue_time = dequeue_time + CTRL_PKT_NET_PATH_LATENCY;
                            ack_pkt.enqueue_time = ack_pkt.dequeue_time;
                            
                            // direct send NACK pkt to sender's sender entt(ack buffer)
                            Entity snd_flow_entt = ctx.data().snd_flows[ack_dst][pkt.flow_id];
                            if (snd_flow_entt == Entity::none()) {return;}
                             _enqueue(ctx.get<AckPktBuf>(snd_flow_entt), ack_pkt);


                            #if PRINT_SEND_RECV_PKT_LOG
                                PrintPkt(ack_pkt, "SACK generated by receiver: ");
                            #endif
                        }
                    }
                }
                //if (pkt.ecn == ECN_MARK::YES && (_last_cnp_timestamp.last_cnp_timestamp == 0 || (_sim_time.sim_time - _last_cnp_timestamp.last_cnp_timestamp) >= CNP_DURATION)) { 
                if (pkt.ecn == ECN_MARK::YES) { 
                    Pkt cnp_pkt;
                    PktType pkt_type = PktType::CNP;

                    create_pkt(cnp_pkt, ack_src, ack_dst, \
                            _l4_port.l4_port, header_len, 0, header_len, sq_num, \
                            pkt.flow_id, enqueue_time, dequeue_time, flow_priority, pkt_type, path, 0, ECN_MARK::YES);
                    
                    // // directly enqueue CNP into sender's snd_recv_flow, bypassing the nework path
                    cnp_pkt.dequeue_time = dequeue_time + CTRL_PKT_NET_PATH_LATENCY;
                    cnp_pkt.enqueue_time = cnp_pkt.dequeue_time;
                    
                    // direct send CNP pkt to sender's sender entt(ack buffer)
                    Entity snd_flow_entt = ctx.data().snd_flows[ack_dst][pkt.flow_id];
                    if (snd_flow_entt == Entity::none()) {return;}
                    _enqueue(ctx.get<AckPktBuf>(snd_flow_entt), cnp_pkt);

                    #if PRINT_SEND_RECV_PKT_LOG
                        PrintPkt(cnp_pkt, "CNP generated by receiver: ");
                    #endif
                    _last_cnp_timestamp.last_cnp_timestamp = _sim_time.sim_time;
                }
            }
        }
        _sim_time.sim_time = end_time;
    }

    void check_flow_state(Engine &ctx, NET_NPU_ID &_net_npu_id, CompletedFlowQueue &_completed_flow_queue,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update) {
        (void)_sim_time;


            #if PRINT_SYS_LOG
                if (_net_npu_id.net_npu_id == 0) {
                    printf("*********Enter into check_flow_state:*********\n");
                }
            #endif
        // printf("_net_npu_id.net_npu_id: %u\n", _net_npu_id.net_npu_id);
        // printf("NPU_NUM: %u\n", NPU_NUM);

        // printf("1");
        uint64_t *tmp_flow_ids = ctx.data().tmp_flow_ids[_net_npu_id.net_npu_id];
        memset(tmp_flow_ids, 0, MAP_SIZE*sizeof(uint64_t));
        // printf("2");
        uint32_t cnt = 0; 

        Map<uint64_t, Entity>& all_snd_flow = ctx.data().snd_flows[_net_npu_id.net_npu_id];  // Use reference to avoid copying
        // printf("3");
        uint32_t totalFlowCount=0;
        Entity tmp_flow_entt_for_log[3];

        for (auto it = all_snd_flow.begin(); it != all_snd_flow.end(); ++it) {
            auto& entry = *it;
            Entity snd_flow_entt = entry.value;
            // printf("4");
            if(ctx.get<FlowState>(snd_flow_entt) == FlowState::UNCOMPLETE) {
                tmp_flow_entt_for_log[totalFlowCount++]=snd_flow_entt;
            }
            // printf("5");
            if (ctx.get<FlowState>(snd_flow_entt) == FlowState::COMPLETE) {
                FlowEvent flow_event = {ctx.get<FlowID>(snd_flow_entt).flow_id, ctx.get<Src>(snd_flow_entt).src, ctx.get<Dst>(snd_flow_entt).dst, \
                    ctx.get<L4Port>(snd_flow_entt).l4_port, ctx.get<NIC_ID>(snd_flow_entt).nic_id, \
                        ctx.get<FlowSize>(snd_flow_entt).flow_size, ctx.get<StartTime>(snd_flow_entt).start_time, \
                        ctx.get<StopTime>(snd_flow_entt).stop_time, ctx.get<FlowState>(snd_flow_entt)};
                _enqueue_flow(_completed_flow_queue, flow_event);
                // printf("check_flow_state: flow id %d: src %d-> dst %d %ld, start_time: %ld, stop_time: %ld, \n", \
                //     ctx.get<FlowID>(snd_flow_entt).flow_id, ctx.get<Src>(snd_flow_entt).src, ctx.get<Dst>(snd_flow_entt).dst, ctx.get<FlowSize>(snd_flow_entt).flow_size,
                //     ctx.get<StartTime>(snd_flow_entt).start_time, ctx.get<StopTime>(snd_flow_entt).stop_time);
                // printf("check_flow_state: _completed_flow_queue: %d\n", get_queue_len(_completed_flow_queue));

                uint64_t flow_id = ctx.get<FlowID>(snd_flow_entt).flow_id;
                tmp_flow_ids[cnt++] = flow_id;
                uint32_t src = ctx.get<Src>(snd_flow_entt).src;
                uint32_t dst = ctx.get<Dst>(snd_flow_entt).dst;

                Entity recv_flow_entt = ctx.data().recv_flows[dst][flow_id];
                ctx.destroyEntity(recv_flow_entt);
                // Lock only around shared resource access
                ctx.data().flow_lock.lock();
                ctx.data().recv_flows[dst].remove(flow_id);
                ctx.data().flow_lock.unlock();

                Entity recv_snd_flow_entt = ctx.data().recv_snd_flows[dst][flow_id];
                ctx.destroyEntity(recv_snd_flow_entt);
                // Lock only around shared resource access
                ctx.data().flow_lock.lock();
                ctx.data().recv_snd_flows[dst].remove(flow_id);
                ctx.data().flow_lock.unlock();

                Entity snd_recv_flow_entt = ctx.data().snd_recv_flows[src][flow_id];
                ctx.destroyEntity(snd_recv_flow_entt);       
                // Lock only around shared resource access
                ctx.data().flow_lock.lock();
                ctx.data().snd_recv_flows[src].remove(flow_id);
                ctx.data().flow_lock.unlock();

                ctx.destroyEntity(snd_flow_entt);
            }
        }

        // printf("6");

        if(totalFlowCount >= 0) {
            #if SYS_LOG
                if(SYS_LOG_TARGET_NODE == _net_npu_id.net_npu_id)
                {
                printf("[NET] [Flows] NPU [%d] has [%d] flows unfinish.\n",  _net_npu_id.net_npu_id, totalFlowCount);
                }
            #endif

            #if SIMPLE_LOG_MODE
                // if(SYS_LOG_TARGET_NODE == _net_npu_id.net_npu_id)
                // {
                    if(_sim_time.sim_time % (1000*1000)== 0) {

                        // ctx.get<SndNxt>(snd_flow_entt).snd_nxt = 0;
                        // ctx.get<SndUna>(snd_flow_entt).snd_una = 0;
                    // ctx.get<CC_Para>(snd_flow_entt).m_rate = nic_rate;
                    // ctx.get<CC_Para>(snd_flow_entt).tar_rate = nic_rate;

                    printf("simulation time: %ld, [NET] [Flows] NPU [%u] has [%u] flows unfinish.\n", _sim_time.sim_time, _net_npu_id.net_npu_id, totalFlowCount);
                    for(int i=0; i<totalFlowCount; i++) {
                        printf("simulation time: %ld, [NET] [Flows] NPU [%u], flow id %lu, src: %u, dst: %u, size: %lu, snd_next: %ld, snd_una: %ld, tar_rate: %lf Mps, m_rate: %lf Mps\n", 
                            _sim_time.sim_time,
                            _net_npu_id.net_npu_id, 
                            ctx.get<FlowID>(tmp_flow_entt_for_log[i]).flow_id, 
                            ctx.get<Src>(tmp_flow_entt_for_log[i]).src, 
                            ctx.get<Dst>(tmp_flow_entt_for_log[i]).dst, 
                            ctx.get<FlowSize>(tmp_flow_entt_for_log[i]).flow_size, 
                            ctx.get<SndNxt>(tmp_flow_entt_for_log[i]).snd_nxt, 
                            ctx.get<SndUna>(tmp_flow_entt_for_log[i]).snd_una, 
                            ctx.get<CC_Para>(tmp_flow_entt_for_log[i]).m_rate/(1000*1000.0), 
                            ctx.get<CC_Para>(tmp_flow_entt_for_log[i]).tar_rate/(1000*1000.0));
                    }
                    }
            // }
            #endif
        }
        // printf("7");
        for (uint32_t i = 0; i < cnt; i++) {
            ctx.data().snd_flows[_net_npu_id.net_npu_id].remove(tmp_flow_ids[i]);
        }
        // printf("8");
        if (DEV_MODE == 1)
        {
            clear_queue(_completed_flow_queue);       
        }
    } 

    void clear_finished_flow_queue(Engine &ctx, NET_NPU_ID &_net_npu_id, CompletedFlowQueue &_completed_flow_queue,
        SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update) {
        (void)_sim_time;


        #if PRINT_SYS_LOG
            if (_net_npu_id.net_npu_id == 0) {
                printf("*********Enter into clear_finished_flow_queue:*********\n");
            }
        #endif

        clear_queue(_completed_flow_queue);                
    }
}
