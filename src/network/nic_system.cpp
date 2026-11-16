#include "nic_system.hpp"
#include "queue_system.hpp"


#include <algorithm>
#include <iostream>

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;
using namespace std;

// #define PRINT_PKT_LOG 1
// #define PRINT_SYS_LOG 1
// #define PRINT_PKT_PATH_LOG 1

//#define PRINT_QUEUE_OVERFLOW_LOG 1
#define INF 0x7FFFFFFF

namespace madEscape {

    void nic_forward(Engine &ctx, NIC_ID &_nic_id, NET_NPU_ID &_net_npu_id, NICRate &_nic_rate, SimTime &_sim_time, 
            SimTimePerUpdate &_sim_time_per_update, BidPktBuf &_bid_pkt_buf) {
        (void)_nic_rate;
        (void)_sim_time;
        (void)_sim_time_per_update;
#if PRINT_SYS_LOG
        if (_net_npu_id.net_npu_id == 0) {
            printf("\n*******Enter into nic_forward*********\n");
            printf("start _sim_time.sim_time: %ld, end_time: %ld, net_npu_id: %u, nic_id: %u\n", _sim_time.sim_time, _sim_time.sim_time+_sim_time_per_update.sim_time_per_update, _net_npu_id.net_npu_id, _nic_id.nic_id);
        }
#endif

        uint64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;

        // firstly, we handle the snd flow in sender side (send data pkt)
        // we should get the packet in the time order 
        int max_flow_cnt = ctx.data().snd_flows[_net_npu_id.net_npu_id].size();

        // int* p_idx = new int[flow_cnt];
        // we use pointer to avoid the direct copy 
        PktBuf** pkt_bufs = new PktBuf* [max_flow_cnt];

        int flow_cnt = 0;

        //snd flow in sender side (send data pkt)
        Map<uint64_t, Entity>& tmp_snd_flow = ctx.data().snd_flows[_net_npu_id.net_npu_id];  // avoid copy
        for (auto it = tmp_snd_flow.begin(); it != tmp_snd_flow.end(); ++it) {
            auto& entry = *it;
            Entity flow_entt = entry.value;
            // if the flow is not for this nic, skip
            // printf("ctx.get<NIC_ID>(flow_entt).nic_id: %u, nic_id: %u\n", ctx.get<NIC_ID>(flow_entt).nic_id, _nic_id.nic_id);
            if (ctx.get<NIC_ID>(flow_entt).nic_id != _nic_id.nic_id) {
                continue;
            }

            pkt_bufs[flow_cnt] = &(ctx.get<PktBuf>(flow_entt));
            flow_cnt++;
        }


        // merge sort
        while(true){
            // first, we need to find the packet with minimum enqueue time
            // use this index to record the queue index 
            int idx = -1;
            uint32_t min_time = INF;
            Pkt pkt, tmp_pkt;
            // we loop all port, find the packet with minimum enqueue time
            for(int i = 0; i < static_cast<int>(flow_cnt); ++i){
                if(!fetch_elem(*(pkt_bufs[i]), 0, tmp_pkt))
                    continue;
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

            // get the true packet from the queue
            _dequeue(*(pkt_bufs[idx]), pkt);

            uint32_t send_interval = (pkt.ip_pkt_len*8*1.0/_nic_rate.nic_rate)*(1000*1000*1000);
            if(_sim_time.sim_time > pkt.enqueue_time){
                pkt.dequeue_time = _sim_time.sim_time + send_interval;
            }
            else{
                pkt.dequeue_time = pkt.enqueue_time + send_interval;
            }
            _sim_time.sim_time = pkt.dequeue_time;

            _enqueue(_bid_pkt_buf.snd_buf, pkt);
            #if PRINT_PKT_LOG
            PrintPkt(pkt, "nic_forward(data pkt): ");
            #endif

            // after 
            if(_sim_time.sim_time > end_time){
                break;
            }
        }


        // snd flow in receiver side (send ACK/NACK/CNP pkt)
        Map<uint64_t, Entity>& tmp_recv_snd_flow = ctx.data().recv_snd_flows[_net_npu_id.net_npu_id];  // avoid copy
        for (auto it = tmp_recv_snd_flow.begin(); it != tmp_recv_snd_flow.end(); ++it) {
            auto& entry = *it;
            Entity flow_entt = entry.value;

            PktBuf& recv_snd_flow_buf = ctx.get<PktBuf>(flow_entt);  // avoid copy

            uint32_t pkt_num = get_queue_len(recv_snd_flow_buf);
            Pkt pkt;
            for (uint16_t k = 0; k < pkt_num; k++) {
                _dequeue(recv_snd_flow_buf, pkt);

                _enqueue(_bid_pkt_buf.snd_buf, pkt);
#if PRINT_PKT_LOG
                PrintPkt(pkt, "nic_forward(ACK/NACK/CNP): ");
#endif
            }
            clear_queue(ctx.get<PktBuf>(flow_entt));
        }

        // if(_sim_time.sim_time < end_time){
        //     _sim_time.sim_time = end_time;
        // }
        _sim_time.sim_time = end_time;
        // printf("nic_forward: end_time: %ld, final _sim_time.sim_time: %ld\n", end_time, _sim_time.sim_time);
        delete[] pkt_bufs;

    }


    void nic_transmit(Engine &ctx, NIC_ID &_nic_id, NET_NPU_ID &_net_npu_id,
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update, 
            BidPktBuf &_bid_pkt_buf, TXHistory &_tx_history, 
            NextHop &_next_hop, Seed &_seed) {
        (void)_nic_id;
        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;


        #if PRINT_SYS_LOG
        if (_net_npu_id.net_npu_id == 0) {
            printf("\n*******Enter into nic_transmit*********\n");
            printf("start _sim_time.sim_time: %ld, end_time: %ld\n", _sim_time.sim_time, end_time);
        }
        #endif

        Pkt pkt;
        while(true){
            if(!fetch_elem(_bid_pkt_buf.snd_buf, 0, pkt)) break;
            _dequeue(_bid_pkt_buf.snd_buf, pkt); 
            pkt.enqueue_time = pkt.dequeue_time + _HS_link_delay.HS_link_delay; 

            Entity in_port = ctx.data().inPorts[_next_hop.next_hop];
            _enqueue(ctx.get<PktBuf>(in_port), pkt); 
            _sim_time.sim_time += pkt.ip_pkt_len*8*1.0/_nic_rate.nic_rate*(1000*1000*1000);
            if(_sim_time.sim_time >= end_time){
                break;
            }
        }
    }


    void nic_receive(Engine &ctx, NIC_ID &_nic_id, NET_NPU_ID &_net_npu_id,
            BidPktBuf &_bid_pkt_buf, NICRate &_nic_rate,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update) {
        (void)_nic_id;
        (void)_nic_rate;

        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;

#if PRINT_SYS_LOG
        if (_net_npu_id.net_npu_id == 0) {
            printf("\n*******Enter into nic_receive*********\n");
            printf("start _sim_time.sim_time: %ld, end_time: %ld\n", _sim_time.sim_time, end_time);
        }
#endif
      

        Pkt pkt;
        while (_sim_time.sim_time < end_time) {
            if(!fetch_elem(_bid_pkt_buf.recv_buf, 0, pkt)) break;
            if(pkt.enqueue_time >= end_time) break;
            _dequeue(_bid_pkt_buf.recv_buf, pkt);
            // recv flow entity
            Entity recv_flow = Entity::none();
            if (pkt.pkt_type == PktType::DATA) {
#if PRINT_PKT_LOG
                PrintPkt(pkt, "nic_receive(data): ");
#endif

                auto *entry = ctx.data().recv_flows[pkt.dst].find(pkt.flow_id); 
                if (entry) {
                    recv_flow = entry->value;
                } // else recv_flow = Entity::none()
            }
            else { //ACK/NACK/CNP
#if PRINT_PKT_LOG
                PrintPkt(pkt, "nic_receive(//ACK/NACK/CNP): ");
#endif

                auto *entry = ctx.data().snd_recv_flows[pkt.dst].find(pkt.flow_id); 
                if (entry) {
                    recv_flow = entry->value;
                }
            }

            if (recv_flow == Entity::none()) {
#if PRINT_REDUNDANT_PKT_LOG
                printf("Error: no flow entity exist for the pkt(maybe flow already finished, but extral pkts are injected into network)(src: %u, dst: %u, flow_id: %u\n", pkt.src, pkt.dst, (uint32_t)pkt.flow_id);
#endif
                continue;
                // return;
            }

            #if ENABLE_PKT_PATH_RECORD
            // path tracer
            pkt.path[pkt.path_len] = _net_npu_id.net_npu_id;
            pkt.path[pkt.path_len+1] = _nic_id.nic_id;
            pkt.path_len += 2;
            assert(pkt.path_len <= MAX_PATH_LEN);
            #endif

            _enqueue(ctx.get<PktBuf>(recv_flow), pkt);

#if PRINT_PKT_PATH_LOG
            PrintPath(pkt.path, pkt.path_len);  
#endif
        }
    }

} // namespace madEscape 
