#include "forward_system.hpp"
#include <algorithm>
#include <iostream>

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;
using namespace std;

#define INF 0x7FFFFFFF

namespace madEscape {
    // Utility function for printing bits (used for debugging forward plans)
    template <typename T>
        void printBits(T value) {
            for(CountT i = sizeof(T) * 8 - 1; i >= 0; i--) {
                if(value & (1ULL << i))  printf("1");
                else  printf("0");
            }
        }

    // ForwardPlan manipulation functions
    constexpr uint32_t forward_plan_bits_per_word = 64u;

    inline void clear_forward_plan_row(uint64_t *row) {
        for (uint32_t word = 0; word < ForwardPlan::wordCount; ++word) {
            row[word] = 0;
        }
    }

    inline void set_forward_plan_bit(uint64_t *row, uint32_t idx) {
        uint32_t word = idx / forward_plan_bits_per_word;
        if (word >= ForwardPlan::wordCount) {
            return;
        }
        uint32_t bit = idx % forward_plan_bits_per_word;
        row[word] |= (1ULL << bit);
    }

    inline bool test_forward_plan_bit(const uint64_t *row, uint32_t idx) {
        uint32_t word = idx / forward_plan_bits_per_word;
        if (word >= ForwardPlan::wordCount) {
            return false;
        }
        uint32_t bit = idx % forward_plan_bits_per_word;
        return (row[word] & (1ULL << bit)) != 0;
    }

    inline void print_forward_plan_row(const uint64_t *row) {
        for (uint32_t word = 0; word < ForwardPlan::wordCount; ++word) {
            printBits(row[word]);
            if (word + 1 < ForwardPlan::wordCount) {
                printf(" ");
            }
        }
    }
    //end of ForwardPlan manipulation functions



    uint32_t ECMP_HASH_SEED = 0x9747b28c;
    uint32_t EcmpHash(const uint32_t* key, size_t len, uint32_t seed) {
        uint32_t h = seed;
        if (len > 3) {
            const uint32_t* key_x4 = (const uint32_t*) key;
            size_t i = len >> 2;
            do {
                uint32_t k = *key_x4++;
                k *= 0xcc9e2d51;
                k = (k << 15) | (k >> 17);
                k *= 0x1b873593;
                h ^= k;
                h = (h << 13) | (h >> 19);
                h += (h << 2) + 0xe6546b64;
            } while (--i);
            key = key_x4;
        }
        if (len & 3) {
            size_t i = len & 3;
            uint32_t k = 0;
            key = &key[i - 1];
            do {
                k <<= 8;
                k |= *key--;
            } while (--i);
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
        }
        h ^= len;
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }
    uint32_t GetOutDev(Pkt pkt){
        uint32_t key[1+1+2+2+1+1];  // 32 bytes buffer
        int pos = 0;
        key[pos++] = pkt.src;
        key[pos++] = pkt.dst;
        uint64_t flow_id_src_port = pkt.flow_id;
        key[pos++] = uint32_t((flow_id_src_port >> 0) & 0xFFFFFFFF);
        key[pos++] = uint32_t((flow_id_src_port >> 32) & 0xFFFFFFFF);
        uint64_t flow_id_dst_port = pkt.flow_id;
        key[pos++] = uint32_t((flow_id_dst_port >> 0) & 0xFFFFFFFF);
        key[pos++] = uint32_t((flow_id_dst_port >> 32) & 0xFFFFFFFF);
        uint32_t pkt_type =uint32_t(pkt.pkt_type);
        key[pos++] = pkt_type;
        uint32_t cid = uint32_t(pkt.cid);
        key[pos++] = cid;
        uint32_t idx = EcmpHash(key, pos*4, ECMP_HASH_SEED);
        return idx;
    }

    // Forward function - handles packet forwarding from ingress to egress ports
    // on *egress* ports
    // each egress port actively fetches packets from in_ports
    void _forward(Engine &ctx,
            SchedTrajType sched_traj_type,
            LocalPortID &_local_port_id,
            GlobalPortID &_global_port_id,
            PktQueue &_queue,
            SwitchID &_switch_id,
            SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update,
            LinkRate &_link_rate,
            Seed &_seed,
            PFCState &_pfc_state)
    {
        (void)sched_traj_type;
        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;
        Entity sw_id = ctx.data()._switches[_switch_id.switch_id];
        uint32_t start_port_id = ctx.get<StartPortID>(sw_id).start_port_id;
        uint32_t stop_port_id = ctx.get<StopPortID>(sw_id).stop_port_id;
        uint32_t port_cnt = stop_port_id - start_port_id + 1;
        #if PRINT_SYS_LOG
            if (_global_port_id.global_port_id == 0) {
                printf("\n*******Enter into _forward sys*********\n");
                printf("start _sim_time.sim_time: %ld, end_time: %ld\n", _sim_time.sim_time, end_time);
                printf("_switch_id.switch_id: %d, start_port_id: %d, stop_port_id: %d\n", _switch_id.switch_id, start_port_id, stop_port_id);
            }
        #endif
        // USE LCG to generate a random start position
        LCG lcg(48271, 0, 2147483647, _seed.seed);
        uint32_t offset = port_cnt > 0 ? (lcg.next() % port_cnt) : 0;
        _seed.seed = lcg.next();
        Entity in_port;
        // the logic: for each queue of ingress port, there are sorted by enqueue time
        // hence, we use merge sort of all packet
        // we first prepare all data structures
        // the index of packet in each ingress port queue
        int* p_idx = new int[port_cnt];
        // we keep pointers to the forward bitmap rows
        const uint64_t** fwd_bitmap = new const uint64_t*[port_cnt];
        // we use pointer to avoid the direct copy 
        PktBuf** pkt_bufs = new PktBuf* [port_cnt];
        for (uint32_t cnt = 0; cnt < port_cnt; cnt++) {
            p_idx[cnt] = 0;
            uint32_t i = start_port_id + ((offset + cnt) % port_cnt);
            in_port = ctx.data().inPorts[i];
            fwd_bitmap[cnt] = ctx.get<ForwardPlan>(in_port).forward_plan[_local_port_id.local_port_id];
            // get the pointer
            pkt_bufs[cnt] = &(ctx.get<PktBuf>(in_port));
        }
        const uint32_t max_bit_length = ForwardPlan::bitCount;

        // we assume 
        while(true){
            int idx = -1;
            uint32_t min_time = INF;
            Pkt pkt, tmp_pkt;
            // we loop all port, find the packet with minimum enqueue time
            for(int i = 0; i < static_cast<int>(port_cnt); ++i){
                // first we need to find the next packet for this egress port
                while(p_idx[i] < static_cast<int>(max_bit_length) &&
                        !test_forward_plan_bit(fwd_bitmap[i], static_cast<uint32_t>(p_idx[i]))){
                    p_idx[i]++;
                }
                // if queue is in the end, we skip this port
                if(p_idx[i] >= static_cast<int>(max_bit_length))
                    continue;
                // we use reference 
                fetch_elem(*(pkt_bufs[i]), p_idx[i], tmp_pkt);
                if(tmp_pkt.enqueue_time < min_time){
                    min_time = tmp_pkt.enqueue_time;
                    idx = i;
                    pkt = tmp_pkt;
                }
            }
            // if there is no packet, we should exit the while loop
            if(min_time == INF){
                break;
            }
            
            // this packet will be enqueue, hence, we increase the index;
            p_idx[idx]++;



            uint8_t queue_idx = pkt.flow_priority; 
            // use reference to avoid direct copy
            PktBuf &egress_queue = _queue.pkt_buf[queue_idx];
            int now_queue_length = calculate_queue_length_on_enqueue(egress_queue, pkt.enqueue_time);
            

            // we should do two things,
            // firstly, check this packet should be enqueue or drop
            // to do this, we should maintain the queueing length when packet is arriving
            // TODO: should we use the total length?
            if(now_queue_length+pkt.ip_pkt_len > BUF_SIZE){
                // DROP this packet
                #if PRINT_PKT_LOG
                PrintPkt(pkt, "forward: drop pkt");
            #endif
                continue;
            }
            // PFC LOGIC
            // first, if we find the queue length is larger than the threshold, we should pause all upstream ingress port
            // hence, we set the PFC state to PAUSE for all upstream ingress port
            // we use the pfc state in the egress entity to record whether this egress port trigger PFC PAUSE
            if (now_queue_length >= BUF_SIZE*(PFC_HIGH_THR/100.0) && _pfc_state == PFCState::RESUME) {
            // we should pause all upstream ingress port
                _pfc_state = PFCState::PAUSE;
                printf("switch id %d, egress port Entity %d change to PFC PAUSE queue %p and now queue length %d cur_num %d in sim time %ld\n", _switch_id.switch_id, ctx.data().ePorts[_global_port_id.global_port_id].id, &egress_queue, now_queue_length, egress_queue.cur_num, _sim_time.sim_time);
            }
            uint32_t send_interval = (pkt.ip_pkt_len*8*1.0/_link_rate.link_rate)*(1000*1000*1000);
            // noted, there are two systems used for egress port, one is the forward system, the other is the transmit system
            // hence, we increase the sim time in the transmit system, and only set the pkt.dequeue time in the forward system
            // secondly, set the right dequeue time of current packet
            if(now_queue_length == 0){
                // if there is no packet, we only need to send this packet
                pkt.dequeue_time = pkt.enqueue_time + send_interval;
            }
            else{
                Pkt last_pkt = get_last_packet(egress_queue);
                if(last_pkt.dequeue_time > pkt.enqueue_time)
                    pkt.dequeue_time = last_pkt.dequeue_time + send_interval;
                else
                    pkt.dequeue_time = pkt.enqueue_time + send_interval;

            }
            #if ENABLE_PKT_PATH_RECORD
            // path tracer
            pkt.path[pkt.path_len] = ctx.get<SwitchID>(in_port).switch_id;
            pkt.path[pkt.path_len+1] = ctx.get<LocalPortID>(in_port).local_port_id;
            pkt.path_len += 2;
            assert(pkt.path_len <= MAX_PATH_LEN);
            #endif

            if (egress_queue.cur_num >= PKT_BUF_LEN) {
                printf("Egress queue is full %p, global_port_id: %d in sim time %ld\n", &egress_queue, _global_port_id.global_port_id, _sim_time.sim_time);
                PrintQueue(egress_queue);
            }

            // last enqueue this packet
            _enqueue(egress_queue, pkt);



        }
        delete [] p_idx;
        delete [] fwd_bitmap;
        delete [] pkt_bufs;
    }

    // Set forward plan function - determines forwarding plan for ingress port
    void set_forward_plan(Engine &ctx,
            LocalPortID &_local_port_id,
            GlobalPortID &_global_port_id,
            SwitchID &_switch_id,
            PktBuf &_queue,
            ForwardPlan &_forward_plan,
            SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update)
    {   
        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;
        #if PRINT_SYS_LOG
            if (_global_port_id.global_port_id == 0) {
                printf("\n\n*******Enter into set_forward_plan sys*********\n");
                printf("start _sim_time.sim_time: %ld, end_time: %ld\n", _sim_time.sim_time, end_time);
            }
        #endif
        Entity sw_ett = ctx.data()._switches[_switch_id.switch_id];
        uint32_t port_num = ctx.get<PortNum>(sw_ett).port_num;

        for (uint32_t i = 0; i < port_num; i++) {
            clear_forward_plan_row(_forward_plan.forward_plan[i]);
        }


        uint32_t q_len = get_queue_len(_queue);

        if(_queue.pfc_state == PFCState::PAUSE) {
            return;
        }


        uint32_t idx = 0;
        while(q_len--) { // at most processing q_len packets
            Pkt pkt;
            // fetch_elem packet from buffer while checking whether buffer is empty
            if(!fetch_elem(_queue, idx, pkt)) break;
            if(pkt.enqueue_time >= end_time) break;
            if (pkt.pkt_type != PktType::PFC_PAUSE && pkt.pkt_type != PktType::PFC_RESUME) {
                int32_t sw_idx = _switch_id.switch_id;
                Entity switch_ett = ctx.data()._switches[sw_idx];            
                #if PRINT_PKT_LOG
                    PrintPkt(pkt, "forward:");
                #endif
                uint8_t next_hop_link_idx = 0;
                if(PKT_SPRAYING_PROTOCOL_ON && pkt.pkt_type == PktType::DATA){

                    uint32_t equal_path_num = ctx.data().next_hop_num[ctx.data().num_net_npu+_switch_id.switch_id][pkt.dst];
                    if (equal_path_num == 0) { 
                        idx += 1; 
                        continue; 
                    }
                    uint32_t hashed_dev_id = GetOutDev(pkt);
                    uint32_t hop_idx = hashed_dev_id % equal_path_num;
                    next_hop_link_idx = ctx.get<FIBTable>(switch_ett).fib_table[pkt.dst][hop_idx];
                }
                else{
                    // printf("next_hop_num[%d]: %d\n", ctx.data().num_net_npu + _switch_id.switch_id, ctx.data().next_hop_num[ctx.data().num_net_npu + _switch_id.switch_id][pkt.dst]);
                    // for (int i = 0; i < ctx.data().num_net_npu + ctx.data().num_switch; i++) {
                    //     printf("%d, ", ctx.data().next_hop_num[ctx.data().num_net_npu + _switch_id.switch_id][pkt.dst]);
                    // }
                    // printf("\n");
                    int32_t sw_idx = ctx.data().num_net_npu + _switch_id.switch_id;
                    uint32_t next_hop_num_val = ctx.data().next_hop_num[sw_idx][pkt.dst];

                    // printf("pkt.src: %d, pkt.dst: %d, sw_idx: %d, next_hop_num_val: %d, pkt.flow_id: %d\n", pkt.src, pkt.dst, sw_idx, next_hop_num_val, pkt.flow_id);
                    // printf("pkt.src: %d, pkt.dst: %d, sw_idx: %d, next_hop_num_val: %d, pkt.flow_id: %d, hop_idx: %d\n", pkt.src, pkt.dst, sw_idx, next_hop_num_val, pkt.flow_id, pkt.flow_id % next_hop_num_val);

                    // if (next_hop_num_val == 0 || next_hop_num_val == 255) {
                    //     printf("[ERROR] next_hop_num invalid! switch_id=%d, pkt.dst=%d, pkt.src=%d, num_net_npu=%d, src_idx=%d, next_hop_num_val=%u\n",
                    //            _switch_id.switch_id, pkt.dst, pkt.src, ctx.data().num_net_npu, src_idx, next_hop_num_val);
                    //     // Print some nearby next_hop_num values for debugging
                    //     printf("[DEBUG] next_hop_num[%d][0..5]: ", src_idx);
                    //     for (int d = 0; d < 6 && d < ctx.data().num_net_npu; d++) {
                    //         printf("%u ", ctx.data().next_hop_num[src_idx][d]);
                    //     }
                    //     printf("\n");
                    //     idx += 1;
                    //     continue;
                    // }
                    
                    uint32_t hop_idx = pkt.flow_id % next_hop_num_val;
                    next_hop_link_idx = ctx.get<FIBTable>(switch_ett).fib_table[pkt.dst][hop_idx];
                }
               

                set_forward_plan_bit(_forward_plan.forward_plan[next_hop_link_idx], idx);
               
            }

            #if PRINT_PKT_PATH_LOG
            PrintPath(pkt.path, pkt.path_len);
            #endif

            #if ENABLE_PKT_PATH_RECORD
            // path tracer
            _queue.pkts[(_queue.head+idx) % PKT_BUF_LEN].path[pkt.path_len] = _switch_id.switch_id;
            _queue.pkts[(_queue.head+idx) % PKT_BUF_LEN].path[pkt.path_len+1] = _local_port_id.local_port_id;
            _queue.pkts[(_queue.head+idx) % PKT_BUF_LEN].path_len += 2;
            assert(_queue.pkts[(_queue.head+idx) % PKT_BUF_LEN].path_len <= MAX_PATH_LEN);
            #endif

            #if PRINT_PKT_PATH_LOG
            PrintPath(_queue.pkts[(_queue.head+idx) % PKT_BUF_LEN].path, _queue.pkts[(_queue.head+idx) % PKT_BUF_LEN].path_len);
            #endif

            //  broadcast PFC pause/resume frame into all egress port excepting for the current port itself
            else {
                if (pkt.dst == 0) {// the pfc frame is received from the downstream egress port 
                    Entity e_port = ctx.data().ePorts[_global_port_id.global_port_id]; 
                    // the idx of priority queue is placed in the filed of pkt.flow_id
                    // the PFC state is placed in the filed of the pkt.pkt_type
                    if (pkt.pkt_type == PktType::PFC_PAUSE)
                        ctx.get<PktQueue>(e_port).queue_pfc_state[pkt.flow_id] = PFCState::PAUSE;
                    else 
                        ctx.get<PktQueue>(e_port).queue_pfc_state[pkt.flow_id] = PFCState::RESUME;
                }
                for (uint32_t i = 0; i < port_num; i++) { // broadcast
                    if (i != _local_port_id.local_port_id)
                        set_forward_plan_bit(_forward_plan.forward_plan[i], idx);
                }
            }
            idx += 1;
            
            if(idx == (ForwardPlan::bitCount) && _global_port_id.global_port_id == 0){
                    printf("idx > ForwardPlan::bitCount,  switch_id: %d, global_port_id: %d, local_port_id: %d, idx: %d, queue_len: %d\n", _switch_id.switch_id, _global_port_id.global_port_id, _local_port_id.local_port_id, idx, get_queue_len(_queue));
                    
                    PrintQueue(_queue);
            }


            assert(idx < ForwardPlan::bitCount);
            

        }
    }

    // Remove packets function - removes processed packets from ingress port buffer
    void remove_pkts(Engine &ctx,
            LocalPortID &_local_port_id,
            GlobalPortID &_global_port_id,
            PktBuf &_queue,
            ForwardPlan &_forward_plan,
            SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update,
            SwitchID &_switch_id)
    {   
        (void)ctx;
        (void)_local_port_id;
        (void)_global_port_id;
        (void)_forward_plan;

        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;
        #if PRINT_SYS_LOG
            if (_global_port_id.global_port_id == 0) {
                printf("*******Enter into remove_pkts sys*********\n");
            }
        #endif

         // PFC logic

        // for ingress port, we should loop all outport in the same switch
        Entity sw_ett = ctx.data()._switches[_switch_id.switch_id];
        uint32_t start_port_id = ctx.get<StartPortID>(sw_ett).start_port_id;
        uint32_t stop_port_id = ctx.get<StopPortID>(sw_ett).stop_port_id;

        if(_queue.pfc_state == PFCState::RESUME) {
            // TODO: if there is possible, some port pause the sending, while some port resume the sending
            for (uint32_t i = start_port_id; i <= stop_port_id; i++){
                Entity e_port = ctx.data().ePorts[i];
                // we loop all egress ports in the same switch, if any egress port is paused, we should also pause this ingress port
                if(ctx.get<PFCState>(e_port) == PFCState::PAUSE) {
                    _queue.pfc_state = PFCState::PAUSE;
                    //printf("due to switch id %d, egress port Entity %d is paused, in remove packets, inport Entity %d change from PFC RESUME to PFC PAUSE queue %p\n", _switch_id.switch_id, e_port.id, ctx.data().inPorts[_global_port_id.global_port_id].id, &_queue);
                    break;
                }
            }
        }
        else if(_queue.pfc_state == PFCState::PAUSE) {
            bool all_resumed = true;
            for (uint32_t i = start_port_id; i <= stop_port_id; i++) {
                Entity e_port = ctx.data().ePorts[i];
                if(ctx.get<PFCState>(e_port) == PFCState::PAUSE) {
                    all_resumed = false;
                    break;
                }
            }
            if(all_resumed) {
                _queue.pfc_state = PFCState::RESUME;
            }
        }


        uint32_t q_len = get_queue_len(_queue);
        if (q_len <= 0) {
            _sim_time.sim_time = end_time;
            return;
        }
        Pkt pkt;
        while(fetch_elem(_queue, 0, pkt)) {
            // fetch_elem packet from buffer while checking whether buffer is empty
            if(pkt.enqueue_time >= end_time) {
                break;
            }
            _dequeue(_queue, pkt);
        }

       

        _sim_time.sim_time = end_time; // FIXME
    }

} // namespace madEscape 