#pragma once

#include "common_types.hpp"
#include "global_types.hpp"

namespace madEscape {

    enum class PktType : uint8_t {
        DATA = 0,
        ACK = 1,
        PFC_PAUSE = 2,
        PFC_RESUME = 3,
        NACK = 4,
        CNP = 5,
        SACK = 6,
    };
    enum class PFCState : uint8_t {
        PAUSE = 2,
        RESUME = 3,
    };

    enum class ECN_MARK : uint8_t {
        NO = 0,
        YES = 1,
    };

    // struct Pkt {
#ifdef __CUDACC__
    struct __align__(8) Pkt {
#else
        struct alignas(8) Pkt {
#endif
            uint32_t src;
            uint32_t dst;         // also PFC_FOR_UPSTREAM for the PFC frame
            uint16_t l4_port;
            uint16_t header_len;
            uint16_t payload_len;
            uint16_t ip_pkt_len;
            int64_t sq_num;
            uint64_t flow_id;     // also the idx of priority queue for the PFC frame
            int64_t enqueue_time;
            int64_t dequeue_time;
            uint8_t flow_priority;
            PktType pkt_type;     // also PFC state for the PFC frame
            #if ENABLE_PKT_PATH_RECORD
            uint32_t path[MAX_PATH_LEN];
            uint8_t path_len;
            #endif
            ECN_MARK ecn;
            int32_t cid;          //  Packet Spraying Protocol
            int32_t csn;          //  Packet Spraying Protocol
            uint64_t bitmap_start_offset;          //  Packet Spraying Protocol
            uint64_t bitmap_value;          //  Packet Spraying Protocol
            uint8_t block_num;   //  SACK Protocol
            uint64_t sack_block[8];   //  SACK Protocol
        };

        struct PktBuf {
            uint32_t head;
            uint32_t tail;
            uint32_t cur_num;
            uint32_t cur_bytes;
            Pkt pkts[PKT_BUF_LEN];
            // this length is used for record the queue length on a packet try to enqueue
            uint32_t queue_bytes_on_enqueue;
            // the index for calculate the queue bytes on euqueue
            uint32_t enqueue_index;
            // this length is used for record the queue length on a packet try to dequeue
            uint32_t queue_bytes_on_dequeue;
            // the index for calculate the queue bytes on dequeue
            uint32_t dequeue_index;
            PFCState pfc_state;
        };

        struct AckPktBuf {
            Pkt pkts[PKT_BUF_LEN];
            uint16_t head;
            uint16_t tail;
            uint16_t cur_num;
            uint32_t cur_bytes;
        };
        struct PktQueue {
            PktBuf pkt_buf[QUEUE_NUM];
            PFCState queue_pfc_state[QUEUE_NUM];
        };

    }
