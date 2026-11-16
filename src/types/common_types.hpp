#pragma once

#include <madrona/components.hpp>
#include <madrona/math.hpp>
#include <madrona/physics.hpp>
#include <madrona/rand.hpp>
#include <madrona/render/ecs.hpp>

#include "../consts.hpp"



#define MAX_CONCURRENT_FLOW_NUM 7000
// #define MAX_SW_NUM  200  // 720 //1300 //1500/10
// #define MAX_NET_NPU_NUM  500  // 3500 //8200 //8500/10
// #define MAX_LINKS_NUM 4000  // 11000 //50000/10

#define MAX_SW_NUM  800LL
#define MAX_NET_NPU_NUM 3500LL
#define MAX_LINKS_NUM 21000LL


// #define MAX_SW_NUM  500LL
// #define MAX_NET_NPU_NUM 1024LL
// #define MAX_LINKS_NUM 10000LL

// #define MAX_SW_NUM  10
// #define MAX_NET_NPU_NUM 50
// #define MAX_LINKS_NUM 200

#define MAX_ONE_SW_PORT_NUM 32

#define MAX_NIC_NUM_PER_NET_NPU 3

// NET_NPU
#define MAX_NET_NPU_FLOW_NUM 10  // 1024 //max number of flows concurrent exist in 1 net_npu at most

#define MAX_ALL_PORT_NUM (MAX_SW_NUM*MAX_ONE_SW_PORT_NUM + MAX_NET_NPU_NUM*2)

// Topo tensor size calculation based on data structures:
#define TOPO_TENSOR_SIZE (20*MAX_LINKS_NUM)

// FIB tensor size calculation based on data structures:
// fib[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] +
// next_hop_num[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM]
// Since FIB data is now uint8_t but stored in int32_t array, divide by 4 to pack efficiently
#define FIB_TENSOR_SIZE ((MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM + \
                         (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM))
// #define FIB_TENSOR_SIZE (16LL*10*10*1000*1000)  // #(33LL*10*10*1000*1000)/10

#define NIC_RATE 1000LL*1000*1000 * 100  // 100 Gbps
#define PCIE_BW 1000LL*1000*1000 * 100  // 100 Gbps
#define NVL_BW 1000LL*1000*1000 * 450 * 8 // 3.6 Tbps

#define BW 100  // 100 Gbps
                // egress port
#define QUEUE_NUM 1

// packet buffer
#define PKT_BUF_LEN 900  // 300
#define TX_BUF_LEN PKT_BUF_LEN
#define MTU 1500  // 1500
#define BUF_SIZE PKT_BUF_LEN*MTU  // byte
#define PFC_HIGH_THR 80
#define PFC_LOW_THR 60

// ECN configuration
#define K_MAX 0.4 * BUF_SIZE  // 600*1500*0.4
#define K_MIN 0.1 * BUF_SIZE  // 600*1500*0.1
#define P_MAX 0.2

// link
#define SS_LINK_DELAY 1000  // 1000 //ns switch --> switch
#define HS_LINK_DELAY 1000  // 1000  //ns host --> switch

#define CTRL_PKT_NET_PATH_LATENCY 6000  // ns

#define LOOKAHEAD_TIME 1000  // SS_LINK_DELAY
#define PACKET_PROCESS_TIME 100  // 100 ns
#define TMP_BUF_LEN SS_LINK_DELAY/PACKET_PROCESS_TIME
#define MAX_NUM_NEXT_HOP MAX_ONE_SW_PORT_NUM
#define MAX_PATH_LEN 40  // 16 //8 hops (each hop includes device id and port id)

// Retransmit timer
#define RETRANSMIT_TIMER 3000*1000  // 100us

// DCQCN parameter
#define DCQCN_Alpha 1.0
#define DCQCN_CurrentRate NIC_RATE  // 100Gbps
#define DCQCN_TargetRate NIC_RATE  // 100Gbps
#define DCQCN_G 0.00390625
#define DCQCN_AIRate 20*1000*1000  // 20M bps  //50M bps
#define DCQCN_HAIRate 200*1000*1000  // 200 Mbps  //500M bps
#define DCQCN_AlphaUpdateInterval 1 * 1000  // 1us,  nanoseconds
#define DCQCN_RateDecreaseInterval 4 * 1000  // 4us,  nanoseconds
#define DCQCN_RateIncreaseInterval 300 * 1000  // 300us,  nanoseconds
#define DCQCN_IncreaseStageCount 0  // runtime variables, should set at runtime
#define DCQCN_RecoveryStageThreshold 1  // 5
#define DCQCN_RateDecreaseNextTime 0  // runtime variables, should set at runtime
#define DCQCN_RateIncreaseNextTime 0  // runtime variables, should set at runtime
#define DCQCN_AlphaUpdateNextTime 0  // runtime variables, should set at runtime
#define DCQCN_RateIsDecreased false
#define DCTCP_ACKedBytes 0
#define DCTCP_Alpha 1.0
#define DCTCP_ECNMarkedBytes 0
#define DCTCP_G 0.0625
#define DCTCP_LastUpdateACKNumber -1
#define DCTCP_RAIRate 1*1000*1000*1000L
#define CNP_DURATION 4*1000  // 50*1000 // 50*1000 //50us

// Map structure size
#define MAP_SIZE 2000
#define MESSAGE_BUFFER_LENGTH (1000*10)
#define PSP_PATH_NUM 8
#define PSP_CONTAINER_LEN 4
#define MAX_DEPTH_INTERVAL_TIMER 12000
#define MAX_BUBBLE_TIMER 20000
#define MAX_CACHE_HOLE_DEPTH 4
#define PKT_SPRAYING_PROTOCOL_ON false
#define SACK_ON false

namespace madEscape {

    // Include several madrona types into the simulator namespace for convenience
    using madrona::CountT;
    using madrona::Entity;
    using madrona::RandKey;
    using madrona::base::ObjectID;
    using madrona::base::Position;
    using madrona::base::Rotation;
    using madrona::base::Scale;
    using madrona::phys::ExternalForce;
    using madrona::phys::ExternalTorque;
    using madrona::phys::ResponseType;
    using madrona::phys::RigidBody;
    using madrona::phys::Velocity;
}
