#include "sim.hpp"

#include <madrona/mw_gpu_entry.hpp> 

#include <madrona/taskgraph_builder.hpp>
#include <madrona/math.hpp>
#include <madrona/custom_context.hpp>

#include "types.hpp"
#include "init.hpp"

#include <iostream>
#include <vector>
#include <algorithm>

// #include <cstdio>
// #include <cstring>
// #include <cstdlib>

using namespace madrona;
using namespace madrona::math;

namespace madsimple {

// Archtype: Switch
// SwitchType::edge
// SwitchType
// FIBTable
// calc_next_hop()
// const int32_t k_ary = 8; 
const int32_t num_switch = K_ARY*K_ARY*5/4;
const int32_t num_host = K_ARY*K_ARY*K_ARY/4;




void generate_switch(Engine &ctx, CountT k_ary)
{
    CountT num_edge_switch = k_ary*k_ary/2;
    CountT num_aggr_switch = k_ary*k_ary/2; // the number of aggragate switch
    CountT num_core_switch = k_ary*k_ary/4;

    CountT num_host = k_ary*k_ary*k_ary/4;
    CountT num_next_hop = k_ary;

    printf("*******generate_switch*******\n");
    // build edge switch
    for (int32_t i = 0; i < num_edge_switch; i++) {
        Entity e_switch = ctx.makeEntity<Switch>();
        ctx.get<SwitchType>(e_switch) = SwitchType::Edge;
        ctx.get<SwitchID>(e_switch).switch_id = i;

        for (int32_t j = 0; j < num_host; j++) {
            for (int32_t k = 0; k < num_next_hop; k++) {
                ctx.get<FIBTable>(e_switch).fib_table[j][k] = central_fib_table[i][j][k];
            }
        }

        ctx.data()._switch[ctx.data().numSwitch++] = e_switch;
    }

    // build aggragate switch
    for (int32_t i = 0; i < num_aggr_switch; i++) {
        Entity e_switch = ctx.makeEntity<Switch>();
        ctx.get<SwitchType>(e_switch) = SwitchType::Aggr;
        ctx.get<SwitchID>(e_switch).switch_id = i+num_edge_switch;

        for (int32_t j = 0; j < num_host; j++) {
            for (int32_t k = 0; k < num_next_hop; k++) {
                ctx.get<FIBTable>(e_switch).fib_table[j][k] = central_fib_table[i+num_edge_switch][j][k];
            }
        }

        ctx.data()._switch[ctx.data().numSwitch++] = e_switch;
    }

    // build core switch
    for (int32_t i = 0; i < num_core_switch; i++) {
        Entity e_switch = ctx.makeEntity<Switch>();
        ctx.get<SwitchType>(e_switch) = SwitchType::Core;
        ctx.get<SwitchID>(e_switch).switch_id = i + num_edge_switch + num_aggr_switch;

        for (int32_t j = 0; j < num_host; j++) {
            for (int32_t k = 0; k < num_next_hop; k++) {
                ctx.get<FIBTable>(e_switch).fib_table[j][k] = central_fib_table[i+num_edge_switch+num_aggr_switch][j][k];
            }
        }

        ctx.data()._switch[ctx.data().numSwitch++] = e_switch;
    }
    //printf("\n");
    printf("Total %d switches, %d in_ports, %d e_ports, %d hosts", \
           ctx.data().numSwitch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().numSender);
}


void generate_in_port(Engine &ctx, CountT k_ary)
{
    CountT num_edge_switch = k_ary*k_ary/2;
    CountT num_aggr_switch = k_ary*k_ary/2; // the number of aggragate switch
    CountT num_core_switch = k_ary*k_ary/4;

    printf("*******Generate in ports*******\n");
    // ingress_ports for edge switch
    for (int32_t i = 0; i < num_edge_switch; i++) {
        for (int32_t j = 0; j < k_ary; j++) {
            Entity in_port = ctx.makeEntity<IngressPort>();
            ctx.get<PortType>(in_port) = PortType::InPort;
            ctx.get<PortID>(in_port).port_id = j; //i*k_ary+j;
            ctx.get<PktBuf>(in_port).head = 0;
            ctx.get<PktBuf>(in_port).tail = 0;
            ctx.get<SwitchID>(in_port).switch_id = i;

            ctx.data().inPorts[ctx.data().numInPort++] = in_port;
        }
    }

    // ingress_ports for aggragate switch
    for (int32_t i = 0; i < num_aggr_switch; i++) {
        for (int32_t j = 0; j < k_ary; j++) {
            Entity in_port = ctx.makeEntity<IngressPort>();
            ctx.get<PortType>(in_port) = PortType::InPort;
            ctx.get<PortID>(in_port).port_id = j;  //i*k_ary+j;
            ctx.get<PktBuf>(in_port).head = 0;
            ctx.get<PktBuf>(in_port).tail = 0;
            ctx.get<SwitchID>(in_port).switch_id = i + num_edge_switch;

            ctx.data().inPorts[ctx.data().numInPort++] = in_port;
        }
    }

    // ingress_ports for core switch
    for (int32_t i = 0; i < num_core_switch; i++) {
        for (int32_t j = 0; j < k_ary; j++) {
            Entity in_port = ctx.makeEntity<IngressPort>();
            ctx.get<PortType>(in_port) = PortType::InPort;
            ctx.get<PortID>(in_port).port_id = j; //i*k_ary+j;
            ctx.get<PktBuf>(in_port).head = 0;
            ctx.get<PktBuf>(in_port).tail = 0;
            ctx.get<SwitchID>(in_port).switch_id = i + num_edge_switch + num_aggr_switch;

            ctx.data().inPorts[ctx.data().numInPort++] = in_port;
        }
    }

    printf("Total %d switches, %d in_ports, %d e_ports, %d hosts", \
           ctx.data().numSwitch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().numSender);
}


void generate_e_port(Engine &ctx, CountT k_ary)
{
    CountT num_edge_switch = k_ary*k_ary/2;
    CountT num_aggr_switch = k_ary*k_ary/2; // the number of aggragate switch
    CountT num_core_switch = k_ary*k_ary/4;

    printf("*******Generate egress ports*******\n");
    // egress_ports for edge switch
    for (int32_t i = 0; i < num_edge_switch; i++) {
        for (int32_t j = 0; j < k_ary; j++) {
            Entity e_port = ctx.makeEntity<EgressPort>();
            ctx.get<SchedTrajType>(e_port) = SchedTrajType::FIFO;
            ctx.get<PortType>(e_port) = PortType::EPort;
            ctx.get<PortID>(e_port).port_id = j; //i*k_ary+j;
            ctx.get<PktBuf>(e_port).head = 0;
            ctx.get<PktBuf>(e_port).tail = 0;

            ctx.get<SwitchID>(e_port).switch_id = i;

            if(j < k_ary/2) 
                ctx.get<NextHopType>(e_port) = NextHopType::HOST;
            else
                ctx.get<NextHopType>(e_port) = NextHopType::SWITCH;

            int32_t ett_idx = ctx.data().numEPort;
            ctx.get<NextHop>(e_port).next_hop = next_hop_table[ett_idx];

            ctx.data().ePorts[ctx.data().numEPort++] = e_port;
        }
    }

    // egress_ports for aggragate switch
    for (int32_t i = 0; i < num_aggr_switch; i++) {
        for (int32_t j = 0; j < k_ary; j++) {
            Entity e_port = ctx.makeEntity<EgressPort>();
            ctx.get<SchedTrajType>(e_port) = SchedTrajType::FIFO;
            ctx.get<PortType>(e_port) = PortType::EPort;
            ctx.get<PortID>(e_port).port_id = j; //num_edge_switch*k_ary + i*k_ary + j;
            ctx.get<PktBuf>(e_port).head = 0;
            ctx.get<PktBuf>(e_port).tail = 0;

            ctx.get<SwitchID>(e_port).switch_id = i + num_edge_switch;

            ctx.get<NextHopType>(e_port) = NextHopType::SWITCH;

            int32_t ett_idx = ctx.data().numEPort;
            ctx.get<NextHop>(e_port).next_hop = next_hop_table[ett_idx];

            ctx.data().ePorts[ctx.data().numEPort++] = e_port;
        }
    }

    // egress_ports for core switch
    for (int32_t i = 0; i < num_core_switch; i++) {
        for (int32_t j = 0; j < k_ary; j++) {

            Entity e_port = ctx.makeEntity<EgressPort>();
            ctx.get<SchedTrajType>(e_port) = SchedTrajType::FIFO;
            ctx.get<PortType>(e_port) = PortType::EPort;
            ctx.get<PortID>(e_port).port_id = j; //num_edge_switch*k_ary + num_core_switch*k_ary + i*k_ary+j;
            ctx.get<PktBuf>(e_port).head = 0;
            ctx.get<PktBuf>(e_port).tail = 0;
            ctx.get<SwitchID>(e_port).switch_id = i + num_edge_switch + num_aggr_switch;
            
            ctx.get<NextHopType>(e_port) = NextHopType::SWITCH;

            int32_t ett_idx = ctx.data().numEPort;
            ctx.get<NextHop>(e_port).next_hop = next_hop_table[ett_idx];

            ctx.data().ePorts[ctx.data().numEPort++] = e_port;
        }
    }
    printf("Total %d switches, %d in_ports, %d e_ports, %d hosts", \
           ctx.data().numSwitch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().numSender);
}


void generate_host(Engine &ctx, CountT k_ary)
{
    CountT num_host = k_ary*k_ary*k_ary/4;

    printf("*******Generate sender and receiver*******\n");
    //sender
    for (int32_t i = 0; i < num_host; i++) {
        Entity sender_e = ctx.makeEntity<Sender>();
        ctx.get<SenderID>(sender_e).sender_id = i;
        // ctx.get<EgressPortID>(sender_e).egress_port_id = i;

        ctx.get<NextHopType>(sender_e) = NextHopType::SWITCH;

        ctx.get<SimTime>(sender_e).sim_time = 0;
        ctx.get<HSLinkDelay>(sender_e).HS_link_delay = 1000; // 100 ns 
        ctx.get<NICRate>(sender_e).nic_rate = 1000*1000*1000*100; // 100 Gbps  

        int32_t ett_idx = ctx.data().numSender;
        ctx.get<NextHop>(sender_e).next_hop = next_hop_table_host_to_sw[ett_idx];

        ctx.data().sender[ctx.data().numSender++] = sender_e;
    }
    //receiver
    for (int32_t i = 0; i < num_host; i++) {
        Entity receiver_e = ctx.makeEntity<Receiver>();
        ctx.get<ReceiverID>(receiver_e).receiver_id = i;
        // ctx.get<IngressPortID>(receiver_e).ingress_port_id = i;

        ctx.get<NextHopType>(receiver_e) = NextHopType::SWITCH;

        ctx.get<SimTime>(receiver_e).sim_time = 0;
        ctx.get<HSLinkDelay>(receiver_e).HS_link_delay = 1000; // 100 ns
        ctx.get<NICRate>(receiver_e).nic_rate = 1000*1000*1000*100; // 100 Gbps

        int32_t ett_idx = ctx.data().numReceiver;
        ctx.get<NextHop>(receiver_e).next_hop = next_hop_table_host_to_sw[ett_idx];

        ctx.data().receiver[ctx.data().numReceiver++] = receiver_e;
    }
    printf("Total %d switches, %d in_ports, %d e_ports, %d hosts", \
           ctx.data().numSwitch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().numSender);
}


}