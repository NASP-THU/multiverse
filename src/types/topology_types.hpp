#pragma once

#include "common_types.hpp"

namespace madEscape {

    struct Topo {
        int32_t aj_link[MAX_LINKS_NUM][5];  // Changed to int32_t
        int32_t link_num;
        int32_t net_npu_num;
        int32_t sw_num;
        int32_t port_num[MAX_SW_NUM + MAX_NET_NPU_NUM];  // Changed to int32_t
        int32_t next_hop_port[MAX_ALL_PORT_NUM];  // Changed to int32_t
        int32_t sw_port_num;
        int32_t net_npu_port_num;

        Topo() {
            for (int i = 0; i < MAX_LINKS_NUM; ++i) {
                for (int j = 0; j < 5; ++j) {
                    aj_link[i][j] = 0;
                }
            }
            link_num = 0;
            net_npu_num = 0;
            sw_num = 0;
            for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; ++i) {
                port_num[i] = 0;
            }
            for (int i = 0; i < MAX_ALL_PORT_NUM; ++i) {
                next_hop_port[i] = 0;
            }
            sw_port_num = 0;
            net_npu_port_num = 0;
        }
    };

    struct Fib {
        uint8_t *fib;// [MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM];
        uint8_t *next_hop_num;// [MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_SW_NUM + MAX_NET_NPU_NUM];

        // Constructor to initialize arrays
        Fib() {
            /*
            for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                    for (int k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                        fib[i][j][k] = 255;  // Use 255 as invalid value for uint8_t
                    }
                }
            }

            for (int i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                for (int j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                    next_hop_num[i][j] = 255;  // Use 255 as invalid value for uint8_t
                }
            }
            */
            fib = nullptr;
            next_hop_num = nullptr;
        }
    };

    // New: Tensor component for passing topology data
    struct TopoTensor {
        int32_t data[TOPO_TENSOR_SIZE];  // Increased size to accommodate larger TopoData
    };

    // New: Tensor component for passing routing table data
    struct FibTensor {
        uint8_t data[FIB_TENSOR_SIZE];  // Use precise size calculated by macro definition, now using uint8_t
    };
}
