#pragma once
#pragma once

#include "../sim.hpp"
#include "queue_system.hpp"
#include "packet_system.hpp"

using namespace madrona;

namespace madEscape {

    // NIC forward function - handles packet forwarding from flows to NIC buffer
    void nic_forward(Engine &ctx, NIC_ID &_nic_id, NET_NPU_ID &_net_npu_id, NICRate &_nic_rate, SimTime &_sim_time, 
            SimTimePerUpdate &_sim_time_per_update, BidPktBuf &_bid_pkt_buf);

    // NIC transmit function - handles packet transmission from NIC to network
    void nic_transmit(Engine &ctx, NIC_ID &_nic_id, NET_NPU_ID &_net_npu_id,
            NICRate &_nic_rate, HSLinkDelay &_HS_link_delay,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update, 
            BidPktBuf &_bid_pkt_buf, TXHistory &_tx_history, 
            NextHop &_next_hop, Seed &_seed);

    // NIC receive function - handles packet reception from network to flows
    void nic_receive(Engine &ctx, NIC_ID &_nic_id, NET_NPU_ID &_net_npu_id,
            BidPktBuf &_bid_pkt_buf, NICRate &_nic_rate,
            SimTime &_sim_time, SimTimePerUpdate &_sim_time_per_update);

} // namespace madEscape 
