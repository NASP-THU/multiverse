#pragma once
#include "../types.hpp"
#include "queue_system.hpp"
#include "congestion_control_system.hpp"
#include "packet_system.hpp"
#include "../sim.hpp"
#include <cstdio>
#include <cstring>

#define INF 0x7FFFFFFF

namespace madEscape {
    // Flow creation and management functions
    void create_flow(Engine &ctx, FlowEvent flow_event, uint64_t flow_id, int64_t sim_time);
    void comm_set_flow(Engine &ctx, NET_NPU_ID &_net_npu_id,
            NewFlowQueue &_new_flow_queue, SimTime &_sim_time,  
            SimTimePerUpdate &_sim_time_per_update);

    void setup_flow(Engine &ctx, NET_NPU_ID &_net_npu_id,
            NewFlowQueue &_new_flow_queue, SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update);

    // Flow send and receive functions
    void flow_send(Engine &ctx, FlowID &_flow_id, Src &_src, Dst &_dst, L4Port &_l4_port, 
            FlowSize &_flow_size, StartTime &_start_time, StopTime &_stop_time,
            NIC_ID &_nic_id, SndServerID &_snd_server_id, RecvServerID &_recv_server_id,
            SndNxt &_snd_nxt, SndUna &_snd_una, FlowState &_flow_state, 
            LastAckTimestamp &_last_ack_timestamp, NxtPktEvent &_nxt_pkt_event, 
            PFCState &_pfc_state, CC_Para &_cc_para, 
            PktBuf &_snd_buf, AckPktBuf &_ack_buf, 
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay, 
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update);

    void flow_receive(Engine &ctx, FlowID &_flow_id, Src &_src, Dst &_dst, PktBuf &_recv_queue,
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay, 
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update,
            RecvBytes &_recv_bytes, RecvBitmapAndTriggerCondition &_bitmap_and_trigger, L4Port &_l4_port,
            LastCNPTimestamp &_last_cnp_timestamp, StartTime &_start_time);

    // Flow state management
    void check_flow_state(Engine &ctx, NET_NPU_ID &_net_npu_id, CompletedFlowQueue &_completed_flow_queue,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update);

    void clear_finished_flow_queue(Engine &ctx, NET_NPU_ID &_net_npu_id, CompletedFlowQueue &_completed_flow_queue,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update);

    // Utility function
    void clear_q(Engine &ctx, FlowID &_flow_id, Src &_src, Dst &_dst, L4Port &_l4_port, 
            FlowSize &_flow_size, StartTime &_start_time, StopTime &_stop_time,
            NIC_ID &_nic_id, SndServerID &_snd_server_id, RecvServerID &_recv_server_id,
            SndNxt &_snd_nxt, SndUna &_snd_una, FlowState &_flow_state, 
            LastAckTimestamp &_last_ack_timestamp, NxtPktEvent &_nxt_pkt_event, 
            PFCState &_pfc_state, CC_Para &_cc_para, 
            PktBuf &_snd_buf, AckPktBuf &_ack_buf, 
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay, 
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update); 
}
