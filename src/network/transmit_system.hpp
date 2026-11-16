#pragma once

#include "../sim.hpp"
#include "queue_system.hpp"
#include "packet_system.hpp"

using namespace madrona;

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
            PFCState &_pfc_state);

} // namespace madEscape 
