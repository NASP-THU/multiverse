#include "level_gen.hpp"

//
#include "sim.hpp"
//

namespace madEscape {

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;




void initialize_ctx_data(Engine &ctx, const Topo &topo, const Fib &fib) {
    // Assign topo data to ctx
    memcpy(ctx.data().aj_link, topo.aj_link, sizeof(topo.aj_link));
    ctx.data().num_link = topo.link_num;
    ctx.data().num_net_npu = topo.net_npu_num;
    ctx.data().num_switch = topo.sw_num;
    memcpy(ctx.data().port_num, topo.port_num, sizeof(topo.port_num));
    memcpy(ctx.data().next_hop_port, topo.next_hop_port, sizeof(topo.next_hop_port));
    ctx.data().sw_port_num = topo.sw_port_num;
    ctx.data().net_npu_port_num = topo.net_npu_port_num;

    // Assign fib data to ctx
    memcpy(ctx.data().fib, fib.fib, sizeof(fib.fib));
    memcpy(ctx.data().next_hop_num, fib.next_hop_num, sizeof(fib.next_hop_num));

}



void new_generate_switch(Engine &ctx)
{
    uint32_t num_net_npu = ctx.data().num_net_npu;
    uint32_t num_switch = ctx.data().num_switch;
    uint32_t num_host = ctx.data().num_net_npu;
    uint32_t num_net_npu_port = ctx.data().net_npu_port_num;

    int16_t *port_num = ctx.data().port_num;

    int16_t (*next_hop_num)[MAX_NET_NPU_NUM] = ctx.data().next_hop_num; // Correct type initialization
    int16_t (*fib)[MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] = ctx.data().fib; // Correct type initialization

    int16_t num_next_hop = MAX_ONE_SW_PORT_NUM;

    printf("*******generate_switch*******\n");

    uint32_t start_port_idx = 0;
    uint32_t sw_cnt = 0;
    for (uint32_t sw_id = num_net_npu; sw_id < num_switch+num_net_npu; sw_id++) {
        Entity e_switch = ctx.makeEntity<Switch>();
        printf("");
        ctx.get<SwitchType>(e_switch) = SwitchType::Core;
        ctx.get<SwitchID>(e_switch).switch_id = sw_cnt;

        for (uint32_t j = 0; j < num_host + num_switch; j++) {
            for (uint32_t k = 0; k < num_next_hop; k++) {
                ctx.get<FIBTable>(e_switch).fib_table[j][k] = fib[sw_id][j][k];
            }
        }

        // // Print fib_table if sw_cnt == 1
        // if (sw_cnt == 0) {
        //     printf("FIBTable for switch %d:\n", sw_cnt);
        //     for (uint32_t j = 0; j < num_host + num_switch; j++) {
        //         printf("Row %d: ", j);
        //         for (uint32_t k = 0; k < num_next_hop; k++) {
        //             printf("%d ", ctx.get<FIBTable>(e_switch).fib_table[j][k]);
        //         }
        //         printf("\n");
        //     }
        // }

        ctx.get<QueueNumPerPort>(e_switch).queue_num_per_port = QUEUE_NUM;

        ctx.get<StartPortID>(e_switch).start_port_id = start_port_idx;
        ctx.get<StopPortID>(e_switch).stop_port_id = start_port_idx + port_num[sw_id]-1;
        ctx.get<PortNum>(e_switch).port_num = port_num[sw_id];

        ctx.data()._switches[sw_cnt++] = e_switch;
        
        start_port_idx += port_num[sw_id];
    }

    printf("Total %d switches\n",
           ctx.data().num_switch);
}


void new_generate_in_port(Engine &ctx)
{
    uint32_t num_net_npu = ctx.data().num_net_npu;
    uint32_t num_switch = ctx.data().num_switch;

    uint32_t num_sw_port = ctx.data().sw_port_num;

    int16_t *port_num = ctx.data().port_num;

    ctx.data().numInPort = 0;
    printf("*******Generate in ports*******\n");
    uint32_t global_port_id = 0;
    // uint32_t start_port_idx = 0;
    for (uint32_t sw_id = num_net_npu; sw_id < num_switch+num_net_npu; sw_id++) {

        for (uint32_t local_port_id = 0; local_port_id < port_num[sw_id]; local_port_id++) {
            Entity in_port = ctx.makeEntity<IngressPort>();
            ctx.get<PortType>(in_port) = PortType::InPort;
            ctx.get<LocalPortID>(in_port).local_port_id = local_port_id;
            ctx.get<GlobalPortID>(in_port).global_port_id = global_port_id;

            ctx.get<PktBuf>(in_port).head = 0;
            ctx.get<PktBuf>(in_port).tail = 0;
            ctx.get<PktBuf>(in_port).cur_num = 0;
            ctx.get<PktBuf>(in_port).cur_bytes = 0;

            ctx.get<SwitchID>(in_port).switch_id = sw_id-num_net_npu;

            ctx.get<SimTime>(in_port).sim_time = 0;
            ctx.get<SimTimePerUpdate>(in_port).sim_time_per_update = LOOKAHEAD_TIME; // 1000ns

            ctx.data().inPorts[ctx.data().numInPort++] = in_port;
            global_port_id++;
        }
    }
}


// void new_generate_e_port(Engine &ctx)
// {
//     uint32_t num_net_npu = ctx.data().num_net_npu;
//     uint32_t num_switch = ctx.data().num_switch;

//     uint32_t num_sw_port = ctx.data().sw_port_num;
//     uint32_t num_net_npu_port = ctx.data().net_npu_port_num;

//     int16_t *port_num = ctx.data().port_num;

//     int16_t *next_hop_port = ctx.data().next_hop_port; // Correct initialization

//     printf("*******Generate egress ports*******\n");
//     // egress_ports for edge switch

//     ctx.data().numEPort = 0;
//     uint32_t global_port_id = 0;
//     for (uint32_t sw_id = num_net_npu; sw_id < num_switch; sw_id++) {

//         for (uint32_t local_port_id = 0; local_port_id < port_num[sw_id]; local_port_id++) {
//             Entity e_port = ctx.makeEntity<EgressPort>();
//             ctx.get<SchedTrajType>(e_port) = SchedTrajType::SP;
//             ctx.get<PortType>(e_port) = PortType::EPort;
//             ctx.get<LocalPortID>(e_port).local_port_id = local_port_id;
//             ctx.get<GlobalPortID>(e_port).global_port_id = global_port_id;

//             for (uint32_t k = 0; k < QUEUE_NUM; k++) {
//                 ctx.get<PktQueue>(e_port).pkt_buf[k].head = 0;
//                 ctx.get<PktQueue>(e_port).pkt_buf[k].tail = 0;
//                 ctx.get<PktQueue>(e_port).pkt_buf[k].cur_num = 0;
//                 ctx.get<PktQueue>(e_port).pkt_buf[k].cur_bytes = 0;

//                 ctx.get<PktQueue>(e_port).queue_pfc_state[k] = PFCState::RESUME;
//             }

//             ctx.get<TXHistory>(e_port).head = 0;
//             ctx.get<TXHistory>(e_port).tail = 0;
//             ctx.get<TXHistory>(e_port).cur_num = 0;
//             ctx.get<TXHistory>(e_port).cur_bytes = 0;

//             ctx.get<SwitchID>(e_port).switch_id = sw_id-num_net_npu;

//             if (next_hop_port[num_net_npu_port + global_port_id] < num_net_npu_port) { 
//                 ctx.get<NextHopType>(e_port) = NextHopType::HOST;
//                 ctx.get<NextHop>(e_port).next_hop = next_hop_port[num_net_npu_port + global_port_id]; // next_hop_port table also includes mapping from switch to host 
//             } else {
//                 ctx.get<NextHopType>(e_port) = NextHopType::SWITCH;
//                 ctx.get<NextHop>(e_port).next_hop = next_hop_port[num_net_npu_port + global_port_id] - num_net_npu_port; 
//             }
//             ctx.get<LinkRate>(e_port).link_rate = 1000LL * 1000 * 1000 * 100;
//             ctx.get<SSLinkDelay>(e_port).SS_link_delay = SS_LINK_DELAY;

//             ctx.get<SimTime>(e_port).sim_time = 0;
//             ctx.get<SimTimePerUpdate>(e_port).sim_time_per_update = LOOKAHEAD_TIME; // 1000ns

//             ctx.get<Seed>(e_port).seed = sw_id + 1;

//             ctx.data().ePorts[ctx.data().numEPort++] = e_port;
//             global_port_id++;
//         }
//     }
// }



void new_generate_egress_port_and_nic(Engine &ctx) 
{
    printf("enter generate_egress_port_and_nic\n");
    uint32_t limit = ctx.data().net_npu_port_num + ctx.data().sw_port_num;
    printf("Printing next_hop_port (first %u elements):\n", limit);
    for (uint32_t i = 0; i < limit; i++) {
        printf("next_hop_port[%u] = %d\n", i, ctx.data().next_hop_port[i]);
    }

    uint16_t (*link)[5] = ctx.data().aj_link;
    int16_t *next_hop_port = ctx.data().next_hop_port;

    uint32_t num_sw_port = ctx.data().sw_port_num;
    uint32_t num_net_npu_port = ctx.data().net_npu_port_num;

    uint32_t num_link = ctx.data().num_link;
    uint32_t num_net_npu = ctx.data().num_net_npu;
    
    uint32_t global_nic_cnt = 0;
    uint32_t local_nic_cnt = 0;

    uint32_t local_port_cnt = 0;
    uint32_t global_port_cnt = 0;

    uint32_t prev_src_node = 0; // A end of the previous link
    uint32_t cur_src_node = 0; // A end of the current link

    for (uint32_t idx = 0; idx < num_link; idx++) {
        cur_src_node = link[idx][0];

        if (link[idx][0] < num_net_npu) { // host<-->switch
            if (prev_src_node != cur_src_node) {
                // local_nic_cnt = 0;
            }
            Entity nic_e = ctx.makeEntity<NIC>();
            ctx.get<NIC_ID>(nic_e).nic_id = global_nic_cnt;
    
            // printf("MountedFlows\n");
            ctx.get<MountedFlows>(nic_e).head = 0;
            ctx.get<MountedFlows>(nic_e).tail = 0;
            ctx.get<MountedFlows>(nic_e).cur_num = 0;
    
            // printf("BidPktBuf\n");
            ctx.get<BidPktBuf>(nic_e).snd_buf.head = 0;
            ctx.get<BidPktBuf>(nic_e).snd_buf.tail = 0;
            ctx.get<BidPktBuf>(nic_e).snd_buf.cur_num = 0;
            ctx.get<BidPktBuf>(nic_e).snd_buf.cur_bytes = 0;
    
            ctx.get<BidPktBuf>(nic_e).recv_buf.head = 0;
            ctx.get<BidPktBuf>(nic_e).recv_buf.tail = 0;
            ctx.get<BidPktBuf>(nic_e).recv_buf.cur_num = 0;
            ctx.get<BidPktBuf>(nic_e).recv_buf.cur_bytes = 0;
    
            // printf("TXHistory\n");
            ctx.get<TXHistory>(nic_e).head = 0;
            ctx.get<TXHistory>(nic_e).tail = 0;
            ctx.get<TXHistory>(nic_e).cur_num = 0;
            ctx.get<TXHistory>(nic_e).cur_bytes = 0;
    
            // printf("NextHopType\n");
            ctx.get<NextHopType>(nic_e) = NextHopType::SWITCH;
    
            // printf("HSLinkDelay\n");
            ctx.get<HSLinkDelay>(nic_e).HS_link_delay = link[idx][4]; //unit is nanosecond 
            // printf("NICRate\n");       
            ctx.get<NICRate>(nic_e).nic_rate = 1000LL*1000*1000*link[idx][3]; //unit is Gbps 
            // printf("ett_idx\n");  
    
            // printf("NextHop\n");
            // TODO
            // uint32_t ett_nic_idx = ctx.data().num_nic;
            ctx.get<NextHop>(nic_e).next_hop = next_hop_port[global_nic_cnt];
            //
    
            // printf("SimTime\n");
            ctx.get<SimTime>(nic_e).sim_time = 0;
    
            ctx.get<Seed>(nic_e).seed = cur_src_node;
            ctx.get<SimTimePerUpdate>(nic_e).sim_time_per_update = LOOKAHEAD_TIME; //1000ns
            
            ctx.data()._nics[ctx.data().num_nic] = nic_e;

            ctx.data().num_nic++;
            // local_nic_cnt++;
            global_nic_cnt++;

            // printf("node_id:%d, nic_id: %d, next_hop: %d\n", cur_src_node, ctx.get<NIC_ID>(nic_e).nic_id, ctx.get<NextHop>(nic_e).next_hop); 
        }
        else { // switch<-->host and switch<-->switch
            if (prev_src_node != cur_src_node) {
                local_port_cnt = 0;
            }
            Entity e_port = ctx.makeEntity<EgressPort>();
            ctx.get<SchedTrajType>(e_port) = SchedTrajType::SP;
            ctx.get<PortType>(e_port) = PortType::EPort;
            ctx.get<LocalPortID>(e_port).local_port_id = local_port_cnt;
            ctx.get<GlobalPortID>(e_port).global_port_id = global_port_cnt;

            for (uint32_t k = 0; k < QUEUE_NUM; k++) {
                ctx.get<PktQueue>(e_port).pkt_buf[k].head = 0;
                ctx.get<PktQueue>(e_port).pkt_buf[k].tail = 0;
                ctx.get<PktQueue>(e_port).pkt_buf[k].cur_num = 0;
                ctx.get<PktQueue>(e_port).pkt_buf[k].cur_bytes = 0;

                ctx.get<PktQueue>(e_port).queue_pfc_state[k] = PFCState::RESUME;
            }

            ctx.get<TXHistory>(e_port).head = 0;
            ctx.get<TXHistory>(e_port).tail = 0;
            ctx.get<TXHistory>(e_port).cur_num = 0;
            ctx.get<TXHistory>(e_port).cur_bytes = 0;

            ctx.get<SwitchID>(e_port).switch_id = cur_src_node - num_net_npu;

            if (next_hop_port[num_net_npu_port + global_port_cnt] < num_net_npu_port) {
                ctx.get<NextHopType>(e_port) = NextHopType::HOST;
                ctx.get<NextHop>(e_port).next_hop = next_hop_port[num_net_npu_port + global_port_cnt]; // next_hop_port table also includes mapping from switch to host 
            } else {
                ctx.get<NextHopType>(e_port) = NextHopType::SWITCH;
                ctx.get<NextHop>(e_port).next_hop = next_hop_port[num_net_npu_port + global_port_cnt] - num_net_npu_port; 
            }
            
            ctx.get<SSLinkDelay>(e_port).SS_link_delay = link[idx][4]; //unit is nanosecond 
            ctx.get<LinkRate>(e_port).link_rate = 1000LL * 1000 * 1000 * link[idx][3];


            ctx.get<SimTime>(e_port).sim_time = 0;
            ctx.get<SimTimePerUpdate>(e_port).sim_time_per_update = LOOKAHEAD_TIME; // 1000ns

            ctx.get<Seed>(e_port).seed = cur_src_node + 1;

            ctx.data().ePorts[ctx.data().numEPort++] = e_port;
            
            local_port_cnt++;
            global_port_cnt++;

            // printf("node_id:%d, global_port_id: %d, local_port_id: %d, next_hop: %d\n", cur_src_node, ctx.get<GlobalPortID>(e_port).global_port_id, ctx.get<LocalPortID>(e_port).local_port_id, ctx.get<NextHop>(e_port).next_hop); 
        }
        prev_src_node = cur_src_node;
    }




    // // for test the memory occupation 
    // for (uint32_t idx = 1000; idx < 30000; idx++) {
    //     Entity e_port = ctx.makeEntity<EgressPort>();

    //     ctx.get<SchedTrajType>(e_port) = SchedTrajType::SP;
    //     ctx.get<PortType>(e_port) = PortType::EPort;
    //     ctx.get<LocalPortID>(e_port).local_port_id = idx%100;
    //     ctx.get<GlobalPortID>(e_port).global_port_id = idx;

    //     for (uint32_t k = 0; k < QUEUE_NUM; k++) {
    //         ctx.get<PktQueue>(e_port).pkt_buf[k].head = 0;
    //         ctx.get<PktQueue>(e_port).pkt_buf[k].tail = 0;
    //         ctx.get<PktQueue>(e_port).pkt_buf[k].cur_num = 0;
    //         ctx.get<PktQueue>(e_port).pkt_buf[k].cur_bytes = 0;

    //         ctx.get<PktQueue>(e_port).queue_pfc_state[k] = PFCState::RESUME;
    //     }

    //     ctx.get<TXHistory>(e_port).head = 0;
    //     ctx.get<TXHistory>(e_port).tail = 0;
    //     ctx.get<TXHistory>(e_port).cur_num = 0;
    //     ctx.get<TXHistory>(e_port).cur_bytes = 0;



    //     ctx.data().ePorts[ctx.data().numEPort++] = e_port;

    //     ctx.get<SwitchID>(e_port).switch_id = 10000;
    //     ctx.get<NextHopType>(e_port) = NextHopType::SWITCH;
    //     ctx.get<NextHop>(e_port).next_hop = 100000;
    // }

}



void new_generate_host(Engine &ctx)
{
    uint32_t num_net_npu = ctx.data().num_net_npu;
    uint32_t num_switch = ctx.data().num_switch;

    uint32_t num_sw_port = ctx.data().sw_port_num;
    uint32_t num_net_npu_port = ctx.data().net_npu_port_num;

    int16_t *next_hop_port = ctx.data().next_hop_port;

    uint32_t net_npu_cnt = 0;
    for (uint32_t i = 0; i < num_net_npu; i++) {
        Entity net_npu_e = ctx.makeEntity<NET_NPU>();
        
        ctx.get<NET_NPU_ID>(net_npu_e).net_npu_id = i;
        
        ctx.get<SimTime>(net_npu_e).sim_time = 0;
        ctx.get<SimTimePerUpdate>(net_npu_e).sim_time_per_update = LOOKAHEAD_TIME; //1000ns

        ctx.get<SendFlows>(net_npu_e).head = 0;
        ctx.get<SendFlows>(net_npu_e).tail = 0;
        ctx.get<SendFlows>(net_npu_e).cur_num = 0;

        ctx.get<CompletedFlowQueue>(net_npu_e).head = 0;
        ctx.get<CompletedFlowQueue>(net_npu_e).tail = 0;
        ctx.get<CompletedFlowQueue>(net_npu_e).cur_num = 0;

        ctx.get<NewFlowQueue>(net_npu_e).head = 0;
        ctx.get<NewFlowQueue>(net_npu_e).tail = 0;
        ctx.get<NewFlowQueue>(net_npu_e).cur_num = 0;

        ctx.data()._net_npus[net_npu_cnt++] = net_npu_e;
    }


    // printf("*******Generate NICs*******\n");
    // //nic
    // for (uint32_t i = 0; i < num_host; i++) {
    //     // if (i > 3) {
    //     //     break;
    //     // }
    //     // printf("nic: %d\n", i);
    //     Entity nic_e = ctx.makeEntity<NIC>();
    //     ctx.get<NIC_ID>(nic_e).nic_id = i;
    //     // ctx.get<EgressPortID>(nic_e).egress_port_id = i;

    //     // printf("MountedFlows\n");
    //     ctx.get<MountedFlows>(nic_e).head = 0;
    //     ctx.get<MountedFlows>(nic_e).tail = 0;
    //     ctx.get<MountedFlows>(nic_e).cur_num = 0;

    //     // printf("BidPktBuf\n");
    //     ctx.get<BidPktBuf>(nic_e).snd_buf.head = 0;
    //     ctx.get<BidPktBuf>(nic_e).snd_buf.tail = 0;
    //     ctx.get<BidPktBuf>(nic_e).snd_buf.cur_num = 0;
    //     ctx.get<BidPktBuf>(nic_e).snd_buf.cur_bytes = 0;

    //     ctx.get<BidPktBuf>(nic_e).recv_buf.head = 0;
    //     ctx.get<BidPktBuf>(nic_e).recv_buf.tail = 0;
    //     ctx.get<BidPktBuf>(nic_e).recv_buf.cur_num = 0;
    //     ctx.get<BidPktBuf>(nic_e).recv_buf.cur_bytes = 0;

    //     // printf("TXHistory\n");
    //     ctx.get<TXHistory>(nic_e).head = 0;
    //     ctx.get<TXHistory>(nic_e).tail = 0;
    //     ctx.get<TXHistory>(nic_e).cur_num = 0;
    //     ctx.get<TXHistory>(nic_e).cur_bytes = 0;

    //     // printf("NextHopType\n");
    //     ctx.get<NextHopType>(nic_e) = NextHopType::SWITCH;

    //     // printf("HSLinkDelay\n");
    //     ctx.get<HSLinkDelay>(nic_e).HS_link_delay = HS_LINK_DELAY; // 1 us, 1000 ns 
    //     // printf("NICRate\n");       
    //     ctx.get<NICRate>(nic_e).nic_rate = 1000LL*1000*1000*100; // 100 Gbps 
    //     // printf("ett_idx\n");  

    //     // printf("NextHop\n");
    //     // TODO
    //     uint32_t ett_nic_idx = ctx.data().num_nic;
    //     ctx.get<NextHop>(nic_e).next_hop = next_hop_port[ett_nic_idx];
    //     //

    //     // printf("SimTime\n");
    //     ctx.get<SimTime>(nic_e).sim_time = 0;

    //     ctx.get<Seed>(nic_e).seed = i;
    //     ctx.get<SimTimePerUpdate>(nic_e).sim_time_per_update = LOOKAHEAD_TIME; //1000ns
        
    //     ctx.data()._nics[ctx.data().num_nic++] = nic_e;
    // }


    // memset(ctx.data().flow_cnt, 0, MAX_PATH_LEN*sizeof(uint32_t));

    // printf("Total %d switches, %d in_ports, %d e_ports, %d send_flows, %d recv_flows\n", \
    //        ctx.data().numSwitch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().num_snd_flow, ctx.data().num_recv_flow);
    
    printf("Total %d switches, %d in_ports, %d e_ports, %d net_npus\n", \
           ctx.data().num_switch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().num_net_npu);
}


}
