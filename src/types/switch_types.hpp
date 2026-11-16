#pragma once

#include "common_types.hpp"
#include "simulation_types.hpp"
#include "packet_types.hpp"
#include "network_device_types.hpp"

namespace madEscape {

    struct EgressPortID {
        uint32_t egress_port_id;
    };

    struct IngressPortID {
        uint32_t ingress_port_id;
    };

    enum class PortType : uint8_t {
        InPort = 0,
        EPort = 1,
    };

    // for the router forwarding system, of GPU_acclerated DES
    struct LocalPortID {
        uint32_t local_port_id;
    };

    struct GlobalPortID {
        uint32_t global_port_id;
    };

    enum class SchedTrajType : uint8_t {
        SP = 0,
        FIFO = 1,
    };

    enum class SwitchType : uint8_t {
        Edge = 0,
        Aggr = 1,
        Core = 2,
    };

    struct SwitchID {
        uint32_t switch_id;
    };

    #ifndef FORWARD_PLAN_WORDS
    #define FORWARD_PLAN_WORDS 2
    #endif

    struct ForwardPlan {
        static constexpr uint32_t wordCount = FORWARD_PLAN_WORDS;
        static constexpr uint32_t bitCount = wordCount * 64u;
        uint64_t forward_plan[MAX_ONE_SW_PORT_NUM][wordCount];
    };

    struct FIBTable {
        uint8_t fib_table[MAX_SW_NUM+MAX_NET_NPU_NUM][MAX_NUM_NEXT_HOP];
    };

    struct LinkRate {
        int64_t link_rate;
    };

    struct SSLinkDelay {
        int64_t SS_link_delay;
    };

    struct QueueNumPerPort {
        uint8_t queue_num_per_port;
    };

    struct StartPortID {
        uint32_t start_port_id;
    };

    struct StopPortID {
        uint32_t stop_port_id;
    };

    struct PortNum {
        uint32_t port_num;
    };

    struct Switch : public madrona::Archetype<
                    SwitchType,
                    SwitchID,
                    FIBTable,
                    QueueNumPerPort,
                    StartPortID,
                    StopPortID,
                    PortNum
                    > {};

    struct IngressPort : public madrona::Archetype<
                         PortType,
                         LocalPortID,
                         GlobalPortID,
                         SwitchID,
                         PktBuf,
                         ForwardPlan,
                         SimTime,
                         SimTimePerUpdate
                         > {};

    struct EgressPort : public madrona::Archetype<
                        SchedTrajType,
                        PortType,
                        LocalPortID,
                        GlobalPortID,
                        SwitchID,
                        PktQueue,
                        TXHistory,
                        NextHopType,
                        NextHop,
                        LinkRate,
                        SSLinkDelay,
                        SimTime,
                        SimTimePerUpdate,
                        Seed,
                        PFCState
                        > {};

}
