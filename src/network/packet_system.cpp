#include "packet_system.hpp"

namespace madEscape {
    void PrintPkt(Pkt p, const char *desc) {

        if (p.pkt_type == PktType::DATA)
            printf("pkt type: DATA\n");
        else if (p.pkt_type == PktType::ACK)
            printf("pkt type: ACK\n");
        else if (p.pkt_type == PktType::PFC_PAUSE)
            printf("pkt type: PFC_PAUSE\n");
        else if (p.pkt_type == PktType::PFC_RESUME)
            printf("pkt type: PFC_RESUME\n");
        else if (p.pkt_type == PktType::NACK)
            printf("pkt type: NACK\n");
        else if (p.pkt_type == PktType::SACK)
            printf("pkt type: SACK\n");
        else
            printf("pkt type: CNP\n");

            printf("%s:: flow_id: %lu, src: %u, dst: %u, payload_len: %u, sq_num: %ld, enqueue_time: %ld, dequeue_time: %ld, ecn: %u, cid: %d, csn: %d\n", desc, p.flow_id, p.src, p.dst, (uint32_t)p.payload_len, p.sq_num, p.enqueue_time, p.dequeue_time, (uint32_t)p.ecn, p.cid, p.csn);
    }

    void create_pkt(Pkt &pkt,
            uint32_t src,   // PFC state for the PFC frame 
            uint32_t dst,  // PFC_FOR_UPSTREAM for the PFC frame 
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
            int32_t cid,
            int32_t csn)
    {
        pkt.src = src;
        pkt.dst = dst;
        pkt.l4_port = l4_port;
        pkt.header_len = header_len;
        pkt.payload_len = payload_len;
        pkt.ip_pkt_len = ip_pkt_len;
        pkt.sq_num = sq_num;
        pkt.flow_id = flow_id;
        pkt.enqueue_time = enqueue_time;
        pkt.dequeue_time = dequeue_time;
        pkt.flow_priority = flow_priority;
        pkt.pkt_type = pkt_type;
        #if ENABLE_PKT_PATH_RECORD
        memset(pkt.path, 0, MAX_PATH_LEN * sizeof(uint32_t));  // Clear first
        if (_path != nullptr && path_len > 0) {
            size_t copy_len = (path_len <= MAX_PATH_LEN) ? path_len : MAX_PATH_LEN;
            memcpy(pkt.path, _path, copy_len * sizeof(uint32_t));
        }
        pkt.path_len = path_len;
        #endif 
        pkt.ecn = ecn;
        pkt.cid = cid;
        pkt.csn = csn;
        if(pkt.pkt_type != PktType::SACK){
            pkt.block_num = 0;
            memset(pkt.sack_block, 0, sizeof(pkt.sack_block));
        }
    }

    void create_sack_pkt(Pkt &pkt);

    void PrintPath(uint32_t *path, uint8_t path_len) {
        printf("path: length: %u: ", (uint32_t)path_len);
        for(uint32_t i = 0; i < path_len/2; i++) {
            printf("(%d, %d), ", path[2*i], path[2*i+1]);
        }
        printf("\n");
    } 
}
