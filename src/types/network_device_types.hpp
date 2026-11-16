#pragma once

#include "common_types.hpp"
#include "simulation_types.hpp"
#include "packet_types.hpp"
#include "flow_types.hpp"

namespace madEscape {

    enum class NextHopType : uint8_t {
        HOST = 0,
        SWITCH = 1,
    };

    struct NextHop {
        uint32_t next_hop;
    };

    struct SendFlows {
        madrona::Entity flow[MAX_NET_NPU_FLOW_NUM];
        uint16_t head;
        uint16_t tail;
        uint16_t cur_num;
    };

    struct NET_NPU_ID {
        uint32_t net_npu_id;
    };

    struct NET_NPU : public madrona::Archetype<
                     NET_NPU_ID,  //
                     SimTime,
                     SimTimePerUpdate,
                     SendFlows,
                     NewFlowQueue,      // from Asstra-sim system layer
                     CompletedFlowQueue  // to Asstra-sim system layer
                     > {};

    struct BidPktBuf {
        PktBuf snd_buf;
        PktBuf recv_buf;
    };

    struct TXElem {
        int64_t dequeue_time;
        uint16_t ip_pkt_len;
    };

    struct TXHistory {
        TXElem pkts[TX_BUF_LEN];
        uint16_t head;
        uint16_t tail;
        uint16_t cur_num;
        uint32_t cur_bytes;
    };

    struct Seed {
        uint32_t seed;
    };

    struct NIC : public madrona::Archetype<
                 NIC_ID,
                 NET_NPU_ID,
                 NICRate,
                 HSLinkDelay,
                 SimTime,
                 SimTimePerUpdate,
                 BidPktBuf,
                 TXHistory,
                 NextHopType,
                 NextHop,
                 Seed
                 > {};

}
