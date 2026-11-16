#include "level_gen.hpp"
#include "sim.hpp"
#include <cstdint>

namespace madEscape {

    using namespace madrona;
    using namespace madrona::math;
    using namespace madrona::phys;

    // Helper structures for tensor parsing (from cpp_topo_fib_handler_example.cpp)
    struct CppTopoData {
        // Scalar values
        int32_t link_num;
        int32_t net_npu_num;
        int32_t sw_num;
        int32_t sw_port_num;
        int32_t net_npu_port_num;
        int32_t reserved;  // Reserved field

        // Array data - using new macro definition sizes
        int32_t aj_link[MAX_LINKS_NUM][5];                    // 5000 elements (unchanged)
        int32_t port_num[MAX_SW_NUM + MAX_NET_NPU_NUM];      // 1344 elements (320+1024)
        int32_t next_hop_port[MAX_ALL_PORT_NUM];             // 12288 elements (320*32+1024*2)

        // Default constructor
        CppTopoData() : link_num(0), net_npu_num(0), sw_num(0), 
        sw_port_num(0), net_npu_port_num(0), reserved(0) {
            memset(aj_link, 0, sizeof(aj_link));
            memset(port_num, 0, sizeof(port_num));
            memset(next_hop_port, 0, sizeof(next_hop_port));
        }
    };

    struct CppFibData {
        // Array data - using new macro definition sizes
        uint8_t fib[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM];          // 1344*1024*32 = 44,040,192 elements
        uint8_t next_hop_num[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM];     // 1344*1024 = 1,376,256 elements

        // Default constructor
        CppFibData() {
            // Initialize fib to 255
            for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                    for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                        fib[i][j][k] = 255;  // Use 255 as invalid value for uint8_t
                    }
                    next_hop_num[i][j] = 255;
                }
            }
        }
    };

    // 
    void printFibTable(Engine &ctx)
    {
        uint32_t num_net_npu = ctx.data().num_net_npu;
        uint32_t num_switch = ctx.data().num_switch;
        uint32_t num_nodes = num_net_npu + num_switch;

        uint8_t (*next_hop_num)[MAX_SW_NUM + MAX_NET_NPU_NUM] = ctx.data().next_hop_num;
        (void)next_hop_num;
        uint8_t (*fib)[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] = ctx.data().fib;
        (void)fib;

        const uint8_t invalid = 255;
        const uint32_t max_hops = MAX_ONE_SW_PORT_NUM;

        printf("    ------- FIB Tables -------\n");
        for (uint32_t sw_idx = 0; sw_idx < num_switch; sw_idx++) {
            uint32_t global_sw_id = sw_idx + num_net_npu;
            Entity e = ctx.data()._switches[sw_idx];

            printf("Switch %u (global id %u):\n", sw_idx, global_sw_id);

            FIBTable &ft = ctx.get<FIBTable>(e);

            for (uint32_t dst = 0; dst < num_nodes; dst++) {
                printf("  dst %u:", dst);

                uint8_t nh = next_hop_num[global_sw_id][dst];
                bool printed_any = false;

                if (nh == invalid || nh == 0) {
                    // fallback: print until an invalid entry is found
                    for (uint32_t k = 0; k < max_hops; k++) {
                        uint8_t v = ft.fib_table[dst][k];
                        if (v == invalid) break;
                        if (printed_any) printf(", ");
                        printf(" %u", v);
                        printed_any = true;
                    }
                } else {
                    for (uint32_t k = 0; k < nh && k < max_hops; k++) {
                        if (printed_any) printf(", ");
                        printf(" %u", ft.fib_table[dst][k]);
                        printed_any = true;
                    }
                }

                if (!printed_any) {
                    printf(" <none>");
                }

                printf("\n");
            }

            printf("\n");
        }
    }


    // Helper function to parse topo tensor (from cpp_topo_fib_handler_example.cpp)
    inline bool parse_topo_tensor(const TopoTensor& tensor, CppTopoData& topo_data) {
        const int32_t* flat_data = tensor.data;

        // Parse scalar values (first 6 elements)
        topo_data.link_num = static_cast<int32_t>(flat_data[0]);
        topo_data.net_npu_num = static_cast<int32_t>(flat_data[1]);
        topo_data.sw_num = static_cast<int32_t>(flat_data[2]);
        topo_data.sw_port_num = static_cast<int32_t>(flat_data[3]);
        topo_data.net_npu_port_num = static_cast<int32_t>(flat_data[4]);
        topo_data.reserved = static_cast<int32_t>(flat_data[5]);

        int idx = 6;

        // parse aj_link[1000][5] - 5000 elements
        for (int i = 0; i < MAX_LINKS_NUM; i++) {
            for (int j = 0; j < 5; j++) {
                topo_data.aj_link[i][j] = static_cast<int32_t>(flat_data[idx++]);
            }
        }

        // parse port_num[MAX_NODES] - MAX_NODES elements
        for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
            topo_data.port_num[i] = static_cast<int32_t>(flat_data[idx++]);
        }

        // parse next_hop_port[MAX_ALL_PORT_NUM] - MAX_ALL_PORT_NUM elements
        for (int i = 0; i < MAX_ALL_PORT_NUM; i++) {
            topo_data.next_hop_port[i] = static_cast<int32_t>(flat_data[idx++]);
        }

        return true;
    }

    // Helper function to parse fib tensor (from cpp_topo_fib_handler_example.cpp)
    inline bool parse_fib_tensor(const FibTensor& tensor, CppFibData& fib_data) {
        const uint8_t* flat_data = tensor.data;

        int idx = 0;

        // parse fib[MAX_NODES][MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] - using macro definition size
        for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
            for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                    fib_data.fib[i][j][k] = flat_data[idx++];
                }
            }
        }

        // parse next_hop_num[MAX_NODES][MAX_NET_NPU_NUM] - using macro definition size
        for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
            for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                fib_data.next_hop_num[i][j] = flat_data[idx++];
            }
        }

        return true;
    }

    // Global variable to store the tensor Agent entity
    static Entity tensor_agent_entity = Entity::none();

    // Function to create Agent entity early for tensor access
    void create_agent_for_tensor_access(Engine &ctx) {
        // Create Agent entity to enable tensor access from Python
        tensor_agent_entity = ctx.makeEntity<Agent>();
        //for llm system.
        ctx.data().init_entity = tensor_agent_entity;
        // CRITICAL: Store this entity as agents[0] to ensure ECS export system can access it
        ctx.data().agents[0] = tensor_agent_entity;

        // Initialize the tensor components with default data
        TopoTensor &topo_tensor = ctx.get<TopoTensor>(tensor_agent_entity);
        FibTensor &fib_tensor = ctx.get<FibTensor>(tensor_agent_entity);

        // Initialize with default values using memset for better performance
        memset(topo_tensor.data, 0, sizeof(int32_t)*100);
        memset(fib_tensor.data, 255, sizeof(uint8_t)*1000);  // Use 255 as invalid value for uint8_t

        printf("  Agent entity created for tensor access. Python can now access tensors.\n");
        printf("  Agent entity stored as agents[0] for ECS export compatibility.\n");
    }


    void new_generate_switch(Engine &ctx)
    {
        uint32_t num_net_npu = ctx.data().num_net_npu;
        uint32_t num_switch = ctx.data().num_switch;
        uint32_t num_host = ctx.data().num_net_npu;


        int32_t *port_num = ctx.data().port_num;


        uint8_t (*fib)[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] = ctx.data().fib; // Correct type initialization

        uint8_t num_next_hop = MAX_ONE_SW_PORT_NUM;

        printf("    *******enter generate_switch*******\n");

        uint32_t start_port_idx = 0;
        uint32_t sw_cnt = 0;
        for (uint32_t sw_id = num_net_npu; sw_id < num_switch+num_net_npu; sw_id++) {
            Entity e_switch = ctx.makeEntity<Switch>();

            ctx.get<SwitchType>(e_switch) = SwitchType::Core;
            ctx.get<SwitchID>(e_switch).switch_id = sw_cnt;


            // fib initilization for switch
            size_t fib_copy_size = (num_host + num_switch) * num_next_hop * sizeof(uint8_t);
            //ctx.get<FIBTable>(e_switch).fib_table = fib[sw_id];
            memcpy(ctx.get<FIBTable>(e_switch).fib_table, fib[sw_id], fib_copy_size);


            ctx.get<QueueNumPerPort>(e_switch).queue_num_per_port = QUEUE_NUM;

            ctx.get<StartPortID>(e_switch).start_port_id = start_port_idx;
            ctx.get<StopPortID>(e_switch).stop_port_id = start_port_idx + port_num[sw_id]-1;
            ctx.get<PortNum>(e_switch).port_num = port_num[sw_id];

            ctx.data()._switches[sw_cnt++] = e_switch;

            start_port_idx += port_num[sw_id];
        }

        printf("    Total %d switches\n", ctx.data().num_switch);

        // print fib
        // printFibTable(ctx);
    }


    void new_generate_in_port(Engine &ctx)
    {
        uint32_t num_net_npu = ctx.data().num_net_npu;
        uint32_t num_switch = ctx.data().num_switch;

        uint32_t num_sw_port = ctx.data().sw_port_num;
        (void)num_sw_port;

        int32_t *port_num = ctx.data().port_num;

        ctx.data().numInPort = 0;
        printf("    *******Generate in ports*******\n");
        uint32_t global_port_id = 0;
        // uint32_t start_port_idx = 0;
        for (uint32_t sw_id = num_net_npu; sw_id < num_switch+num_net_npu; sw_id++) {

            for (uint32_t local_port_id = 0; local_port_id < static_cast<uint32_t>(port_num[sw_id]); local_port_id++) {
                Entity in_port = ctx.makeEntity<IngressPort>();
                ctx.get<PortType>(in_port) = PortType::InPort;
                ctx.get<LocalPortID>(in_port).local_port_id = local_port_id;
                ctx.get<GlobalPortID>(in_port).global_port_id = global_port_id;

                ctx.get<PktBuf>(in_port).head = 0;
                ctx.get<PktBuf>(in_port).tail = 0;
                ctx.get<PktBuf>(in_port).cur_num = 0;
                ctx.get<PktBuf>(in_port).cur_bytes = 0;
                ctx.get<PktBuf>(in_port).queue_bytes_on_enqueue = 0;
                ctx.get<PktBuf>(in_port).queue_bytes_on_dequeue = 0;
                ctx.get<PktBuf>(in_port).dequeue_index = 0;
                ctx.get<PktBuf>(in_port).enqueue_index = 0;
                ctx.get<PktBuf>(in_port).pfc_state = PFCState::RESUME;

                ctx.get<SwitchID>(in_port).switch_id = sw_id-num_net_npu;

                ctx.get<SimTime>(in_port).sim_time = 0;
                ctx.get<SimTimePerUpdate>(in_port).sim_time_per_update = LOOKAHEAD_TIME; // 1000ns

                ctx.data().inPorts[ctx.data().numInPort++] = in_port;
                global_port_id++;
            }
        }
    }




    void new_generate_egress_port_and_nic(Engine &ctx) 
    {
        printf("    enter generate_egress_port_and_nic\n");

        int32_t (*link)[5] = ctx.data().aj_link;
        int32_t *next_hop_port = ctx.data().next_hop_port;

        uint32_t num_sw_port = ctx.data().sw_port_num;
        uint32_t num_net_npu_port = ctx.data().net_npu_port_num;
        (void)num_sw_port;
        (void)num_net_npu_port;

        uint32_t num_link = ctx.data().num_link;
        uint32_t num_net_npu = ctx.data().num_net_npu;

        uint32_t global_nic_cnt = 0;
        uint32_t local_nic_cnt = 0;
        (void)local_nic_cnt;

        uint32_t local_port_cnt = 0;
        uint32_t global_port_cnt = 0;

        uint32_t prev_src_node = 0; // A end of the previous link
        uint32_t cur_src_node = 0; // A end of the current link

        uint8_t *nic_idx = ctx.data().nic_idx;
        memset(ctx.data().nic_idx, 0, sizeof(ctx.data().nic_idx));

        for (uint32_t idx = 0; idx < num_link; idx++) {
            cur_src_node = link[idx][0];
            // printf("edge: a: %u, z: %u\n", link[idx][0], link[idx][1]);
            if (static_cast<uint32_t>(link[idx][0]) < num_net_npu) { // host<-->switch, link[idx][0] must be host npu id
                Entity nic_e = ctx.makeEntity<NIC>();
                //ctx.get<NIC_ID>(nic_e).nic_id = global_nic_cnt;
                ctx.get<NIC_ID>(nic_e).nic_id = nic_idx[cur_src_node];

                // printf("a: %u, z: %u, global_nic_cnt: %u\n", cur_src_node, link[idx][1], global_nic_cnt);

                ctx.get<NET_NPU_ID>(nic_e).net_npu_id = cur_src_node;

                ctx.get<BidPktBuf>(nic_e).snd_buf.head = 0;
                ctx.get<BidPktBuf>(nic_e).snd_buf.tail = 0;
                ctx.get<BidPktBuf>(nic_e).snd_buf.cur_num = 0;
                ctx.get<BidPktBuf>(nic_e).snd_buf.cur_bytes = 0;

                ctx.get<BidPktBuf>(nic_e).recv_buf.head = 0;
                ctx.get<BidPktBuf>(nic_e).recv_buf.tail = 0;
                ctx.get<BidPktBuf>(nic_e).recv_buf.cur_num = 0;
                ctx.get<BidPktBuf>(nic_e).recv_buf.cur_bytes = 0;

                ctx.get<TXHistory>(nic_e).head = 0;
                ctx.get<TXHistory>(nic_e).tail = 0;
                ctx.get<TXHistory>(nic_e).cur_num = 0;
                ctx.get<TXHistory>(nic_e).cur_bytes = 0;

                ctx.get<NextHopType>(nic_e) = NextHopType::SWITCH;

                ctx.get<HSLinkDelay>(nic_e).HS_link_delay = link[idx][4]; //unit is nanosecond 
                ctx.get<NICRate>(nic_e).nic_rate = 1000LL*1000*1000*link[idx][3]; //unit is Gbps
                ctx.get<NextHop>(nic_e).next_hop = next_hop_port[global_nic_cnt];
                ctx.get<SimTime>(nic_e).sim_time = 0;

                ctx.get<Seed>(nic_e).seed = cur_src_node;
                ctx.get<SimTimePerUpdate>(nic_e).sim_time_per_update = LOOKAHEAD_TIME; //1000ns

                // ctx.data()._nics[ctx.data().num_nic] = nic_e;

                ctx.data()._nics[cur_src_node][nic_idx[cur_src_node]] = nic_e;
                nic_idx[cur_src_node]++;

                ctx.data().num_nic++;

                global_nic_cnt++;
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
                    ctx.get<PktQueue>(e_port).pkt_buf[k].queue_bytes_on_enqueue = 0;
                    ctx.get<PktQueue>(e_port).pkt_buf[k].queue_bytes_on_dequeue = 0;
                    ctx.get<PktQueue>(e_port).pkt_buf[k].enqueue_index = 0;
                    ctx.get<PktQueue>(e_port).pkt_buf[k].dequeue_index = 0;
                    ctx.get<PktQueue>(e_port).pkt_buf[k].pfc_state = PFCState::RESUME;
                }

                ctx.get<TXHistory>(e_port).head = 0;
                ctx.get<TXHistory>(e_port).tail = 0;
                ctx.get<TXHistory>(e_port).cur_num = 0;
                ctx.get<TXHistory>(e_port).cur_bytes = 0;

                ctx.get<SwitchID>(e_port).switch_id = cur_src_node - num_net_npu;

                if (static_cast<uint32_t>(next_hop_port[num_net_npu_port + global_port_cnt]) < num_net_npu_port) {
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

                ctx.get<PFCState>(e_port) = PFCState::RESUME;

                ctx.data().ePorts[ctx.data().numEPort++] = e_port;

                local_port_cnt++;
                global_port_cnt++;

            }
            prev_src_node = cur_src_node;
        }
       
    }



    void new_generate_host(Engine &ctx)
    {
        printf("    enter generate_host\n");
        int32_t num_net_npu = ctx.data().num_net_npu;
        int32_t num_switch = ctx.data().num_switch;
        // (void)num_switch;

        // int32_t num_sw_port = ctx.data().sw_port_num;
        // int32_t num_net_npu_port = ctx.data().net_npu_port_num;
        // (void)num_sw_port;
        // (void)num_net_npu_port;

        // int32_t *next_hop_port = ctx.data().next_hop_port;
        // (void)next_hop_port;

        uint8_t (*fib)[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] = ctx.data().fib; // Correct type initialization
        uint8_t num_next_hop = MAX_ONE_SW_PORT_NUM;

        uint32_t net_npu_cnt = 0;
        for (uint32_t i = 0; i < static_cast<uint32_t>(num_net_npu); i++) {
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

            // fib initilization for NPU
            // size_t fib_copy_size = (num_net_npu + num_switch) * num_next_hop * sizeof(uint8_t);
            // memcpy(ctx.get<FIBTable>(net_npu_e).fib_table, fib[i], fib_copy_size);


            ctx.data()._net_npus[net_npu_cnt++] = net_npu_e;
        }


        printf("Total %d switches, %d in_ports, %d e_ports, %d net_npus\n", \
                ctx.data().num_switch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().num_net_npu);


        // the following code is used to pre-allocate the flow entities to avoid the allocation overhead at runtime
        // Entity tmp_flow_entt_array[MAX_CONCURRENT_FLOW_NUM];
        Entity *tmp_flow_entt_array = ctx.data().tmp_flow_entt_array;
        
        for (uint32_t i = 0; i < MAX_CONCURRENT_FLOW_NUM; i++) {
            tmp_flow_entt_array[i] = ctx.makeEntity<SndFlow>();
        }
        for (uint32_t i = 0; i < MAX_CONCURRENT_FLOW_NUM; i++) {
            ctx.destroyEntity(tmp_flow_entt_array[i]);
        }
        printf("pre allocate %d SndFlow entities\n", MAX_CONCURRENT_FLOW_NUM);

        for (uint32_t i = 0; i < MAX_CONCURRENT_FLOW_NUM; i++) {
            tmp_flow_entt_array[i] = ctx.makeEntity<RecvFlow>();
        }
        for (uint32_t i = 0; i < MAX_CONCURRENT_FLOW_NUM; i++) {
            ctx.destroyEntity(tmp_flow_entt_array[i]);
        }
        printf("pre allocate %d RecvFlow entities\n", MAX_CONCURRENT_FLOW_NUM);
    }
}
