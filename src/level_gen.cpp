#include "level_gen.hpp"

//
#include "sim.hpp"
//

namespace madEscape {

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

// Helper structures for tensor parsing (from cpp_topo_fib_handler_example.cpp)
struct CppTopoData {
    // 标量值
    uint32_t link_num;
    uint32_t net_npu_num;
    uint32_t sw_num;
    uint32_t sw_port_num;
    uint32_t net_npu_port_num;
    uint32_t reserved;  // 预留字段
    
    // 数组数据 - 使用新的宏定义大小
    int16_t aj_link[MAX_LINKS_NUM][5];                     // 5000个元素 (不变)
    int16_t port_num[MAX_SW_NUM + MAX_NET_NPU_NUM];      // 1344个元素 (320+1024)
    int16_t next_hop_port[MAX_ALL_PORT_NUM];             // 12288个元素 (320*32+1024*2)
    
    // 默认构造函数
    CppTopoData() : link_num(0), net_npu_num(0), sw_num(0), 
                    sw_port_num(0), net_npu_port_num(0), reserved(0) {
        memset(aj_link, 0, sizeof(aj_link));
        memset(port_num, 0, sizeof(port_num));
        memset(next_hop_port, 0, sizeof(next_hop_port));
    }
};

struct CppFibData {
    // 数组数据 - 使用新的宏定义大小
    int16_t fib[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM];          // 1344*1024*32 = 44,040,192个元素
    int16_t next_hop_num[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM];     // 1344*1024 = 1,376,256个元素
    
    // 默认构造函数
    CppFibData() {
        // 初始化fib为-1
        for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
            for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                    fib[i][j][k] = -1;
                }
                next_hop_num[i][j] = 0;
            }
        }
    }
};

// Helper function to parse topo tensor (from cpp_topo_fib_handler_example.cpp)
inline bool parse_topo_tensor(const TopoTensor& tensor, CppTopoData& topo_data) {
    const int32_t* flat_data = tensor.data;
    
    // 解析标量值 (前6个元素)
    topo_data.link_num = static_cast<uint32_t>(flat_data[0]);
    topo_data.net_npu_num = static_cast<uint32_t>(flat_data[1]);
    topo_data.sw_num = static_cast<uint32_t>(flat_data[2]);
    topo_data.sw_port_num = static_cast<uint32_t>(flat_data[3]);
    topo_data.net_npu_port_num = static_cast<uint32_t>(flat_data[4]);
    topo_data.reserved = static_cast<uint32_t>(flat_data[5]);
    
    int idx = 6;
    
    // parse aj_link[1000][5] - 5000 elements
    for (int i = 0; i < MAX_LINKS_NUM; i++) {
        for (int j = 0; j < 5; j++) {
            topo_data.aj_link[i][j] = static_cast<int16_t>(flat_data[idx++]);
        }
    }
    
    // parse port_num[MAX_NODES] - MAX_NODES elements
    for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
        topo_data.port_num[i] = static_cast<int16_t>(flat_data[idx++]);
    }
    
    // parse next_hop_port[MAX_ALL_PORT_NUM] - MAX_ALL_PORT_NUM elements
    for (int i = 0; i < MAX_ALL_PORT_NUM; i++) {
        topo_data.next_hop_port[i] = static_cast<int16_t>(flat_data[idx++]);
    }
    
    return true;
}

// Helper function to parse fib tensor (from cpp_topo_fib_handler_example.cpp)
inline bool parse_fib_tensor(const FibTensor& tensor, CppFibData& fib_data) {
    const int32_t* flat_data = tensor.data;
    
    int idx = 0;
    
    // parse fib[MAX_NODES][MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] - using macro definition size
    for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
        for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
            for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                fib_data.fib[i][j][k] = static_cast<int16_t>(flat_data[idx++]);
            }
        }
    }
    
    // parse next_hop_num[MAX_NODES][MAX_NET_NPU_NUM] - using macro definition size
    for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
        for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
            fib_data.next_hop_num[i][j] = static_cast<int16_t>(flat_data[idx++]);
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
    
    // Initialize the tensor components with default data
    TopoTensor &topo_tensor = ctx.get<TopoTensor>(tensor_agent_entity);
    FibTensor &fib_tensor = ctx.get<FibTensor>(tensor_agent_entity);
    
    // Initialize with default values using memset for better performance
    memset(topo_tensor.data, 0, sizeof(topo_tensor.data));
    // printf("  initialize FibTensor...\n ");
    memset(fib_tensor.data, -1, sizeof(fib_tensor.data));
    
    printf("  Agent entity created for tensor access. Python can now access tensors.\n");
}

// // New function to initialize ctx.data from tensors
// void initialize_ctx_data_from_tensors(Engine &ctx) {
//     printf("=== initialize_ctx_data_from_tensors: START ===\n");
    
//     // Get the existing agent entity from global variable
//     Entity agent_entity = tensor_agent_entity;
    
//     printf("Agent entity retrieved: %s\n", agent_entity != Entity::none() ? "Valid" : "Invalid");
    
//     if (agent_entity == Entity::none()) {
//         printf("ERROR: Cannot access tensor components - invalid agent entity\n");
//         return;
//     }
    
//     printf("Attempting to access tensor components...\n");
    
//     // Get tensor components from the existing Agent entity with error checking
//     TopoTensor &topo_tensor = ctx.get<TopoTensor>(agent_entity);
//     FibTensor &fib_tensor = ctx.get<FibTensor>(agent_entity);
    
//     printf("Tensor components accessed successfully\n");
//     printf("  TopoTensor data ptr: %s\n", topo_tensor.data ? "Valid" : "NULL");
//     printf("  FibTensor data ptr: %s\n", fib_tensor.data ? "Valid" : "NULL");
        
//     // Try to parse the tensor data
//     CppTopoData topo_data;
//     CppFibData fib_data;
    
//     // Check if tensor data has been updated from Python
//     bool has_topo_data = false;
//     bool has_fib_data = false;
    
//     printf("Checking for topo tensor data...\n");
//     // Check if topo tensor has data (first few elements non-zero)
//     for (int i = 0; i < 6; i++) {
//         printf("  topo_tensor.data[%d] = %d\n", i, topo_tensor.data[i]);
//         if (topo_tensor.data[i] != 0) {
//             has_topo_data = true;
//         }
//     }
    
//     printf("Checking for fib tensor data...\n");
//     // Check if fib tensor has data (first few elements not -1)
//     for (int i = 0; i < 10; i++) {
//         printf("  fib_tensor.data[%d] = %d\n", i, fib_tensor.data[i]);
//         if (fib_tensor.data[i] != -1) {
//             has_fib_data = true;
//         }
//     }
    
//     printf("Data availability check results:\n");
//     printf("  has_topo_data: %s\n", has_topo_data ? "Yes" : "No");
//     printf("  has_fib_data: %s\n", has_fib_data ? "Yes" : "No");
    
//     if (has_topo_data || has_fib_data) {
//         printf("Found tensor data from Agent entity, processing...\n");
        
//         // Parse topo tensor
//         if (has_topo_data) {
//             printf("Parsing topo tensor...\n");
//             if (!parse_topo_tensor(topo_tensor, topo_data)) {
//                 printf("Error: Failed to parse topo tensor from agent entity\n");
//                 has_topo_data = false;
//             } else {
//                 printf("Topo tensor parsed successfully\n");
//                 printf("  Parsed - link_num: %u, net_npu_num: %u, sw_num: %u\n",
//                        topo_data.link_num, topo_data.net_npu_num, topo_data.sw_num);
//             }
//         }
        
//         // Parse fib tensor
//         if (has_fib_data) {
//             printf("Parsing fib tensor...\n");
//             if (!parse_fib_tensor(fib_tensor, fib_data)) {
//                 printf("Error: Failed to parse fib tensor from agent entity\n");
//                 has_fib_data = false;
//             } else {
//                 printf("Fib tensor parsed successfully\n");
//                 printf("  Sample fib data: [%d, %d, %d, %d]\n",
//                        fib_data.fib[0][1][0], fib_data.fib[0][1][1], 
//                        fib_data.fib[0][1][2], fib_data.fib[0][1][3]);
//             }
//         }
        
//         // Update ctx.data with parsed tensor data
//         if (has_topo_data) {
//             printf("Updating ctx.data with parsed topo data...\n");
//             ctx.data().num_link = topo_data.link_num;
//             ctx.data().num_net_npu = topo_data.net_npu_num;
//             ctx.data().num_switch = topo_data.sw_num;
//             ctx.data().sw_port_num = topo_data.sw_port_num;
//             ctx.data().net_npu_port_num = topo_data.net_npu_port_num;
            
//             // Copy aj_link array
//             for (int i = 0; i < 1000; i++) {
//                 for (int j = 0; j < 5; j++) {
//                     ctx.data().aj_link[i][j] = static_cast<uint16_t>(topo_data.aj_link[i][j]);
//                 }
//             }
            
//             // Copy port_num array
//             for (int i = 0; i < (MAX_SW_NUM + MAX_NET_NPU_NUM); i++) {
//                 ctx.data().port_num[i] = topo_data.port_num[i];
//             }
            
//             // Copy next_hop_port array
//             for (int i = 0; i < MAX_ALL_PORT_NUM; i++) {
//                 ctx.data().next_hop_port[i] = topo_data.next_hop_port[i];
//             }
            
//             printf("  Topo data updated from tensor successfully\n");
//         }
        
//         // Assign parsed fib data to ctx
//         if (has_fib_data) {
//             printf("Updating ctx.data with parsed fib data...\n");
//             for (int i = 0; i < (MAX_SW_NUM + MAX_NET_NPU_NUM); i++) {
//                 for (int j = 0; j < MAX_NET_NPU_NUM; j++) {
//                     for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
//                         ctx.data().fib[i][j][k] = fib_data.fib[i][j][k];
//                     }
//                     ctx.data().next_hop_num[i][j] = fib_data.next_hop_num[i][j];
//                 }
//             }
            
//             printf("  Fib data updated from tensor successfully\n");
//         }
        
//         printf("Final ctx.data state after tensor update:\n");
//         printf("  num_link: %u, num_net_npu: %u, num_switch: %u\n", 
//                ctx.data().num_link, ctx.data().num_net_npu, ctx.data().num_switch);
//         printf("  sw_port_num: %u, net_npu_port_num: %u\n", 
//                ctx.data().sw_port_num, ctx.data().net_npu_port_num);
//         printf("  Sample aj_link[0]: [%u, %u, %u, %u, %u]\n", 
//                ctx.data().aj_link[0][0], ctx.data().aj_link[0][1], 
//                ctx.data().aj_link[0][2], ctx.data().aj_link[0][3], ctx.data().aj_link[0][4]);
//         printf("  Sample fib[0][1]: [%d, %d, %d, %d]\n", 
//                ctx.data().fib[0][1][0], ctx.data().fib[0][1][1], 
//                ctx.data().fib[0][1][2], ctx.data().fib[0][1][3]);
               
//     } else {
//         printf("No tensor data found in Agent entity - using existing ctx.data\n");
//         printf("Current ctx.data state:\n");
//         printf("  num_link: %u, num_net_npu: %u, num_switch: %u\n", 
//                ctx.data().num_link, ctx.data().num_net_npu, ctx.data().num_switch);
//     }
    
//     printf("=== initialize_ctx_data_from_tensors: END ===\n");
// }

void new_generate_switch(Engine &ctx)
{
    uint32_t num_net_npu = ctx.data().num_net_npu;
    uint32_t num_switch = ctx.data().num_switch;
    uint32_t num_host = ctx.data().num_net_npu;
    uint32_t num_net_npu_port = ctx.data().net_npu_port_num;

    int16_t *port_num = ctx.data().port_num;

    int16_t (*next_hop_num)[MAX_SW_NUM + MAX_NET_NPU_NUM] = ctx.data().next_hop_num; // Correct type initialization
    int16_t (*fib)[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] = ctx.data().fib; // Correct type initialization

    int16_t num_next_hop = MAX_ONE_SW_PORT_NUM;

    printf("    *******generate_switch*******\n");

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

    printf("    Total %d switches\n", ctx.data().num_switch);
}


void new_generate_in_port(Engine &ctx)
{
    uint32_t num_net_npu = ctx.data().num_net_npu;
    uint32_t num_switch = ctx.data().num_switch;

    uint32_t num_sw_port = ctx.data().sw_port_num;

    int16_t *port_num = ctx.data().port_num;

    ctx.data().numInPort = 0;
    printf("    *******Generate in ports*******\n");
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



void new_generate_egress_port_and_nic(Engine &ctx) 
{
    printf("    enter generate_egress_port_and_nic\n");
    // uint32_t limit = ctx.data().net_npu_port_num + ctx.data().sw_port_num;
    // printf("Printing next_hop_port (first %u elements):\n", limit);
    // for (uint32_t i = 0; i < limit; i++) {
    //     printf("next_hop_port[%u] = %d\n", i, ctx.data().next_hop_port[i]);
    // }

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
    printf("    enter generate_host\n");
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

    
    printf("Total %d switches, %d in_ports, %d e_ports, %d net_npus\n", \
           ctx.data().num_switch, ctx.data().numInPort, ctx.data().numEPort, ctx.data().num_net_npu);
}


}
