#pragma once

#include "../types.hpp"
#include "queue_system.hpp"
#include <cstdio>
#include <cstring>

namespace madEscape {
    void PrintPkt(Pkt p, const char *desc);
    template<typename T>
        inline void PrintQueue(T &queue) {
            printf("print packet buffer : \n");
            for (uint32_t i = 0; i < queue.cur_num; i++) {
                Pkt pkt = get_elem(queue, i);
                printf("index:%d\n", i);
                PrintPkt(pkt, "");
            }
            printf("\n\n");
        }

    void create_pkt(Pkt &pkt,
            uint32_t src,     // PFC state for the PFC frame 
            uint32_t dst,     // PFC_FOR_UPSTREAM for the PFC frame 
            uint16_t l4_port,
            uint16_t header_len,
            uint16_t payload_len, 
            uint16_t ip_pkt_len,
            int64_t sq_num,
            uint64_t flow_id,  // also the idx of priority queue for the PFC frame 
            int64_t enqueue_time,
            int64_t dequeue_time,
            uint8_t flow_priority,
            PktType pkt_type,
            uint32_t _path[MAX_PATH_LEN],
            uint8_t path_len,
            ECN_MARK ecn,
            int32_t cid = -1,
            int32_t csn = -1);

    void PrintPath(uint32_t *path, uint8_t path_len); 

}
