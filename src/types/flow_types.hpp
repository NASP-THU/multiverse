#pragma once

#include "common_types.hpp"
#include "simulation_types.hpp"
#include "packet_types.hpp"
#include <cstdint>

namespace madEscape {

    enum class FlowState : uint8_t {
        UNCOMPLETE = 0,
        COMPLETE = 1,
    };

    struct FlowID {
        uint64_t flow_id;
    };

    // support multiple flow for a host/nic
    // api for Astra-Sim system layer
    struct FlowEvent {
        uint64_t flow_id;
        uint32_t src;
        uint32_t dst;
        uint16_t l4_port;
        uint32_t nic;  // nic id
        uint64_t flow_size;
        int64_t start_time;
        int64_t stop_time;
        FlowState flow_state;
    };

    struct CompletedFlowQueue {
        FlowEvent flow[MAX_NET_NPU_FLOW_NUM];
        uint16_t head;
        uint16_t tail;
        uint16_t cur_num;  // flow_num
    };

    struct NewFlowQueue {
        FlowEvent flow[MAX_NET_NPU_FLOW_NUM];
        uint16_t head;
        uint16_t tail;
        uint16_t cur_num;  // flow_num
    };

    struct Src {
        uint32_t src;
    };

    struct Dst {
        uint32_t dst;
    };

    struct L4Port {
        uint16_t l4_port;
    };

    struct FlowSize {
        uint64_t flow_size;
    };

    struct StartTime {
        int64_t start_time;
        bool started;
    };

    struct StopTime {
        int64_t stop_time;
    };

    struct NIC_ID {
        uint32_t nic_id;
    };

    struct SndServerID {
        uint32_t snd_server_id;
    };

    struct RecvServerID {
        uint32_t recv_server_id;
    };

    struct SndNxt {
        int64_t snd_nxt;
    };

    struct SndUna {
        int64_t snd_una;
    };

   
    struct LastAckTimestamp {
        int64_t last_ack_timestamp;
    };

    struct NxtPktEvent {
        int64_t nxt_pkt_event;
    };

    enum CCAlgo {
        DCQCN,
        DCTCP,
        TIMELY,
        HPCC
    };

    struct CC_Para {
        CCAlgo current_algo;
        int64_t m_rate;    // current rate/pbs
        int64_t tar_rate;  // target rate/bps
        int64_t line_rate; // line rate/bps

        // dcqcn parameters    
        double dcqcn_Alpha;                      // the alpha parameter in DCQCN
        double dcqcn_G;                          // the G parameter in DCQCN
        int64_t dcqcn_AIRate;                    // the increase rate in additive increase stage
        int64_t dcqcn_HAIRate;                   // the increase rate in hyper additive increase stage
        uint32_t dcqcn_AlphaUpdateInterval;      // the timer interval to update alpha
        uint32_t dcqcn_RateDecreaseInterval;     // the timer interval to decrease target rate
        uint32_t dcqcn_RateIncreaseInterval;     // the timer interval to increase target rate
        uint32_t dcqcn_IncreaseStageCount;       // record the count increase stage
        uint32_t dcqcn_RecoveryStageThreshold;   // the threshshold of reconvery stage
        int64_t dcqcn_AlphaUpdateNextTime;       // the next time (nanoseconds) to update alpha parameter
        int64_t dcqcn_RateDecreaseNextTime;      // the next time (nanoseconds) to decrease target rate
        int64_t dcqcn_RateIncreaseNextTime;      // the next time (nanoseconds) to increase target rate
        bool dcqcn_RateIsDecreased;              // the flag to indicate rate is decreased when received ECE flag
        bool CNPState; 

        // dctcp parameters
        int64_t dctcp_ACKedBytes;
        int64_t dctcp_ECNMarkedBytes;
        double dctcp_Alpha;
        double dctcp_G;
        int64_t dctcp_RecvNextNumber;
        int64_t dctcp_LastUpdateACKNumber;
        int64_t dctcp_RAIRate;
    };

    struct HSLinkDelay {
        int64_t HS_link_delay;
    };

    struct NICRate {
        int64_t nic_rate;
    };

    struct SndFlow : public madrona::Archetype<
                     FlowID,
                     Src,  // src net_npu
                     Dst,  // dst net_npu
                     L4Port,  // layer 4 port

                     FlowSize,
                     StartTime,
                     StopTime,

                     NIC_ID,  // nic id
                     SndServerID,  // host pair id
                     RecvServerID,  // host pair id

                     // SqNum,
                     SndNxt,
                     SndUna,
                     FlowState,

                     // cc
                     LastAckTimestamp,
                     NxtPktEvent,
                     PFCState,
                     CC_Para,
                     PktBuf,
                     AckPktBuf,
                     NICRate,
                     HSLinkDelay,
                     SimTime,
                     SimTimePerUpdate
                         > {};

    struct LastCNPTimestamp {
        int64_t last_cnp_timestamp;
    };

    struct RecvBytes {
        int64_t recv_bytes;
    };

    struct RecvBitmapAndTriggerCondition {
        uint32_t bitmap[PKT_BUF_LEN];
        uint32_t bitmap_length;
        uint64_t bitmap_value;
        uint16_t head;
        uint16_t tail;
        uint32_t depth_interval_timer;
        uint32_t bubble_timer;
        uint32_t cache_hole_depth;        
    };

    struct RecvFlow : public madrona::Archetype<
                      FlowID,
                      Src,  // src net_npu
                      Dst,  // dst net_npu
                      L4Port,  // layer 4 port
                      FlowSize,
                      StartTime,
                      StopTime,
                      NIC_ID,  // nic id
                      SndServerID,  // host pair id
                      RecvServerID,  // host pair id
                      FlowState,
                      LastCNPTimestamp,
                      RecvBytes,
                      RecvBitmapAndTriggerCondition,
                      PktBuf,
                      NICRate,
                      HSLinkDelay,
                      SimTime,
                      SimTimePerUpdate    
                          > {};
}
