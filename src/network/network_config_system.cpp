#include "network_config_system.hpp"
#include "../level_gen.hpp"
#include <algorithm>
#include <iostream>

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;
using namespace std;

namespace madEscape {

    // Network configuration and entity generation function
    // Processes tensor data from Python and creates network entities
    void pass_config_data_and_gen_network_entt(Engine &ctx, 
            TopoTensor &_topo_tensor, 
            FibTensor &_fib_tensor) {
        #if PRINT_SYS_LOG
            printf("======================================================\n");
            printf("=== pass_config_data_and_gen_network_entt ===\n");
        #endif
        // Check if tensor data is available
        bool has_topo_data = false;
        bool has_fib_data = false;
        for (int i = 0; i < 6; i++) {
            // printf("%d,", _topo_tensor.data[i]);
            if (_topo_tensor.data[i] != 0) {
                has_topo_data = true;
                break;
            }
        }
        for (int i = 0; i < 257; i++) { // this works for <= scale of fattree 128
            if (_fib_tensor.data[i] != 255) {
                has_fib_data = true;
            }
        }
        // if there is tensor data and the entities are not created, create the entities
        if ((has_topo_data && has_fib_data) && !ctx.data().entities_created) {
            printf("\n=== Creating network entities based on tensor data ===\n");
            // 1. parse the tensor data to ctx.data()
            if (has_topo_data) {
                printf("  1. Parsing topo tensor data...\n");
                const int32_t* topo_flat_data = _topo_tensor.data;
                // Parse scalar values
                if (topo_flat_data[0] > 0) ctx.data().num_link = static_cast<uint32_t>(topo_flat_data[0]);
                if (topo_flat_data[1] > 0) ctx.data().num_net_npu = static_cast<uint32_t>(topo_flat_data[1]);
                if (topo_flat_data[2] > 0) ctx.data().num_switch = static_cast<uint32_t>(topo_flat_data[2]);
                if (topo_flat_data[3] > 0) ctx.data().sw_port_num = static_cast<uint32_t>(topo_flat_data[3]);
                if (topo_flat_data[4] > 0) ctx.data().net_npu_port_num = static_cast<uint32_t>(topo_flat_data[4]);
                int idx = 6;
                // parse aj_link[MAX_LINKS_NUM][5]
                for (int i = 0; i < MAX_LINKS_NUM; i++) {
                    for (int j = 0; j < 5; j++) {
                        ctx.data().aj_link[i][j] = static_cast<int32_t>(topo_flat_data[idx++]);
                    }
                }
                // parse port_num[]
                for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                    ctx.data().port_num[i] = static_cast<int32_t>(topo_flat_data[idx++]);
                }
                // Parse next_hop_port[]
                for (int i = 0; i < MAX_ALL_PORT_NUM; i++) {
                    ctx.data().next_hop_port[i] = static_cast<int32_t>(topo_flat_data[idx++]);
                }
                printf("   ✓ Topo data parsed: num_link=%u, num_net_npu=%u, num_switch=%u\n",
                        ctx.data().num_link, ctx.data().num_net_npu, ctx.data().num_switch);
            }
            if (has_fib_data) {
                printf("  2. Parsing fib tensor data...\n");
                const uint8_t* fib_flat_data = _fib_tensor.data;
                int idx = 0;
                // parse fib[][][]
                ctx.data().fib = (uint8_t (*)[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM])fib_flat_data;
                //memcpy(ctx.data().fib, fib_flat_data, (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM * sizeof(uint8_t));
                //for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                //    for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                //        for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                //            ctx.data().fib[i][j][k] = static_cast<uint8_t>(fib_flat_data[idx++]);
                //        }
                //    }
                //}
                idx+=(MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM;
                // parse next_hop_num[][]
                ctx.data().next_hop_num = (uint8_t (*)[MAX_SW_NUM + MAX_NET_NPU_NUM])(fib_flat_data+idx);
                //memcpy(ctx.data().next_hop_num, fib_flat_data+idx, (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM) * sizeof(uint8_t));
                //for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                //    for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                //        ctx.data().next_hop_num[i][j] = static_cast<uint8_t>(fib_flat_data[idx++]);
                //    }
                //}
                printf("   ✓ Fib data parsed: %d entries processed\n", idx);
            }
            // 3. Creating network entities with parsed data
            printf("  3. Creating network entities with parsed data...\n");
            if (ctx.data().num_switch > 0) {
                // printf("   Creating switches...\n");
                new_generate_switch(ctx);
                printf("   ✓ Created %u switches\n", ctx.data().num_switch);
            }
            if ( ctx.data().num_switch > 0) {
                // printf("   Creating ingress ports...\n");
                new_generate_in_port(ctx);
                printf("   ✓ Created %u ingress ports\n", ctx.data().numInPort);
            }
            if (ctx.data().num_link > 0) {
                // printf("   Creating egress ports and NICs...\n");
                new_generate_egress_port_and_nic(ctx);
                printf("   ✓ Created %u egress ports\n", ctx.data().numEPort);
            }
            if (ctx.data().num_net_npu > 0) {
                // printf("   Creating hosts/NPUs...\n");
                new_generate_host(ctx);
                printf("   ✓ Created %u NPUs\n", ctx.data().num_net_npu);
            }
            // mark the entities as created, avoid duplicate creation
            ctx.data().entities_created = true;
            printf("=== Network entity creation completed ===\n");
        }
        else if (!has_topo_data && !has_fib_data) {
        }
        else if (ctx.data().entities_created) {
        }

    }

} // namespace madEscape 
