#pragma once
#include "../sim.hpp"
#include "queue_system.hpp"
#include "packet_system.hpp"

using namespace madrona;

namespace madEscape {

    // Utility function for printing bits (used for debugging forward plans)
    template <typename T>
        void printBits(T value);

    // Forward function - handles packet forwarding from ingress to egress ports
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
            PFCState &_pfc_state);

    // Set forward plan function - determines forwarding plan for ingress port
    void set_forward_plan(Engine &ctx,
            LocalPortID &_local_port_id,
            GlobalPortID &_global_port_id,
            SwitchID &_switch_id,
            PktBuf &_queue,
            ForwardPlan &_forward_plan,
            SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update);

    // Remove packets function - removes processed packets from ingress port buffer
    void remove_pkts(Engine &ctx,
            LocalPortID &_local_port_id,
            GlobalPortID &_global_port_id,
            PktBuf &_queue,
            ForwardPlan &_forward_plan,
            SimTime &_sim_time,
            SimTimePerUpdate &_sim_time_per_update,
            SwitchID &_switch_id
        );

} // namespace madEscape 
