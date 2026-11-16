#include "transmit_system.hpp"

#include <algorithm>
#include <iostream>

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;
using namespace std;

namespace madEscape {

    // Transmit function - handles packet transmission from egress ports
    void transmit(Engine &ctx, 
            SchedTrajType sched_traj_type, 
            PortType port_type, 
            LocalPortID &_local_port_id, 
            GlobalPortID &_global_port_id, 
            SwitchID &_switch_id, 
            NextHop &_next_hop, 
            NextHopType next_hop_type, 
            PktQueue &_pkt_queue, 
            TXHistory &_tx_history, 
            SSLinkDelay &_ss_link_delay, 
            LinkRate &_link_rate, 
            SimTime &_sim_time, 
            SimTimePerUpdate &_sim_time_per_update, 
            Seed &_seed,
            PFCState &_pfc_state)
    {
        (void)sched_traj_type;
        (void)port_type;
        (void)_local_port_id;
        (void)_global_port_id;
        (void)_tx_history;
        (void)_link_rate;
        int64_t end_time = _sim_time.sim_time + _sim_time_per_update.sim_time_per_update;

        #if PRINT_SYS_LOG
        printf("\n*******Enter into transmit sys*********\n");
        printf("start _sim_time.sim_time: %ld, end_time: %ld\n", _sim_time.sim_time, end_time);
        #endif
        // PFC logic 
        // before transmit, we should check the PFC state of the ingress port in the same link
        // TODO: now, we only use the only one queue
        if (next_hop_type == NextHopType::SWITCH) {
            // we should get the ingress port in the same link
            Entity in_port_in_same_link = ctx.data().inPorts[_next_hop.next_hop];
            // we should get the PFC state of the ingress port in the same link
            if(ctx.get<PktBuf>(in_port_in_same_link).pfc_state == PFCState::PAUSE && _pkt_queue.pkt_buf[0].pfc_state == PFCState::RESUME) {
                _pkt_queue.pkt_buf[0].pfc_state = PFCState::PAUSE;

                // since we receive the PFC PAUSE frame, we should reset the queue length on enqueue
                reset_queue_length_on_enqueue_when_pfc_pause(_pkt_queue.pkt_buf[0]);
            }
            else if(ctx.get<PktBuf>(in_port_in_same_link).pfc_state == PFCState::RESUME && _pkt_queue.pkt_buf[0].pfc_state == PFCState::PAUSE) {
                _pkt_queue.pkt_buf[0].pfc_state = PFCState::RESUME;
                printf("Due to ingress port Entity %d is resumed, egress port Entity %d transmit PFC state is RESUME, global_port_id: %d in sim time %ld\n", in_port_in_same_link.id, ctx.data().ePorts[_global_port_id.global_port_id].id, _global_port_id.global_port_id, _sim_time.sim_time);

                // since we receive the PFC RESUME frame, we should recalculate the queue length on dequeue
                recalculate_queue_length_on_dequeue_when_pfc_resume(_pkt_queue.pkt_buf[0], _sim_time.sim_time, _link_rate);
            }
        }
        uint8_t queue_num = ctx.get<QueueNumPerPort>(ctx.data()._switches[_switch_id.switch_id]).queue_num_per_port;
        // strict priority(SP) to schedule packets 
        for (uint8_t i = 0; i < queue_num; i++) {
            if (get_queue_len(_pkt_queue.pkt_buf[i]) <= 0) continue;

            if (_pkt_queue.pkt_buf[i].pfc_state == PFCState::PAUSE) {
                // printf("transmit PFC state is PAUSE\n");
                continue;
            }
            // new logic of transmit
            // since we drop packet in the forward system
            // in this system, we only need to calculate the ECN states
            // this system is dirven by the dequeue time of packets
            while(_sim_time.sim_time < end_time) {
                Pkt pkt;
                if(!fetch_elem(_pkt_queue.pkt_buf[i], 0, pkt)) break;
                if(pkt.dequeue_time >= end_time) break;
                _dequeue(_pkt_queue.pkt_buf[i], pkt);
                // we set the ECN market based on queue length of the dequeue time 
                int now_queue_len = calculate_queue_length_on_dequeue(_pkt_queue.pkt_buf[i], pkt.dequeue_time) + pkt.ip_pkt_len;


                if (now_queue_len < BUF_SIZE * (PFC_LOW_THR / 100.0) && _pfc_state == PFCState::PAUSE) {
                    _pfc_state = PFCState::RESUME;
                   printf("switch id %d, egress port Entity %d change to PFC RESUME and now queue length %d in sim time %ld\n", _switch_id.switch_id, ctx.data().ePorts[_global_port_id.global_port_id].id, now_queue_len, _sim_time.sim_time);
                }


                LCG lcg(48271, 0, 2147483647, _seed.seed);
                float mark_prob = 0;
                float rand_num;
                if (now_queue_len <= K_MIN && pkt.ecn == ECN_MARK::NO) {
                    pkt.ecn = ECN_MARK::NO;
                }
                else if (now_queue_len > K_MIN && now_queue_len < K_MAX) {
                    mark_prob = (now_queue_len-K_MIN)*P_MAX/(K_MAX-K_MIN);
                    rand_num = (lcg.next()%1000)/1000.0;
                    if (rand_num < mark_prob) {pkt.ecn = ECN_MARK::YES;}
                    _seed.seed = lcg.next();
                }
                else {
                    pkt.ecn = ECN_MARK::YES;
                }
                if(pkt.dequeue_time < _sim_time.sim_time) {
                    pkt.dequeue_time = _sim_time.sim_time;
                }
                else {
                    _sim_time.sim_time = pkt.dequeue_time;
                }
                pkt.enqueue_time = pkt.dequeue_time + _ss_link_delay.SS_link_delay;
                #if ENABLE_PKT_PATH_RECORD
                // path tracer
                pkt.path[pkt.path_len] = _switch_id.switch_id;
                pkt.path[pkt.path_len+1] = _local_port_id.local_port_id;
                pkt.path_len += 2;
                assert(pkt.path_len <= MAX_PATH_LEN);
                #endif

                //move packet from this egress port to next hop port / the dest host
                Entity next_hop_ett;
                if (next_hop_type == NextHopType::SWITCH) {
                    next_hop_ett = ctx.data().inPorts[_next_hop.next_hop];
                    _enqueue(ctx.get<PktBuf>(next_hop_ett), pkt);
                    #if PRINT_PKT_LOG
                    PrintPkt(pkt, "switch transmit to switch: ");
                    #endif
                }
                else { // next hop is NIC, we need to get the NIC entity by the GPU FIB table and pkt.flow_id
                    uint32_t hop_idx = pkt.flow_id % ctx.data().next_hop_num[pkt.dst][pkt.src]; // flow_id % nic_num
                    uint32_t local_nic_idx = ctx.data().fib[pkt.dst][pkt.src][hop_idx];

                    next_hop_ett = ctx.data()._nics[pkt.dst][local_nic_idx];

                    _enqueue(ctx.get<BidPktBuf>(next_hop_ett).recv_buf, pkt);
                    
                    #if PRINT_PKT_LOG
                    PrintPkt(pkt, "switch transmit to NIC: ");
                    #endif
                }
                #if PRINT_PKT_PATH_LOG
                PrintPath(pkt.path, pkt.path_len);
                #endif
            }
        }
        _sim_time.sim_time = end_time;
    }
} // namespace madEscape 
