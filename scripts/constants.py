"""
Python constants consistent with macro definitions in C++ header file types.hpp
Ensures Python code and C++ code use the same values
"""

# basic constants - consistent with the macro definitions in types.hpp
# MAX_SW_NUM = 200               # max switch number
# MAX_NET_NPU_NUM = 500        # max net_npu number
# MAX_LINKS_NUM = 4000 

MAX_SW_NUM = 3000 #320              # max switch number
MAX_NET_NPU_NUM = 5000        # max net_npu number
MAX_LINKS_NUM = 30000

# MAX_SW_NUM = 10               # max switch number
# MAX_NET_NPU_NUM = 50        # max net_npu number
# MAX_LINKS_NUM = 200 

MAX_ONE_SW_PORT_NUM = 144      # max port number per switch

# derived constants
MAX_ALL_PORT_NUM = MAX_SW_NUM * MAX_ONE_SW_PORT_NUM + MAX_NET_NPU_NUM * 2  # 1500*32 + 8500*2 = 65000
MAX_NODES = MAX_SW_NUM + MAX_NET_NPU_NUM  # 1500 + 8500 = 10000 (total nodes: switch + NPU)

# other related constants
MAX_NET_NPU_FLOW_NUM = 10     # max concurrent flow number per net_npu

MAX_NUM_NEXT_HOP = MAX_ONE_SW_PORT_NUM  # max next hop number = 32  
MAX_PATH_LEN = 40               # max path length

# for array size constants
AJ_LINK_MAX_SIZE = MAX_LINKS_NUM         # max size of aj_link array
AJ_LINK_FIELDS = 5              # number of fields per aj_link entry

# Tensor size constants (for memory allocation)
TOPO_TENSOR_MAX_SIZE = (20*MAX_LINKS_NUM)    # max size of topo tensor (increase to accommodate larger arrays)

# FIB tensor size calculation based on data structures (与C++端FIB_TENSOR_SIZE保持一致):
# fib[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] +
# next_hop_num[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_NET_NPU_NUM]
FIB_TENSOR_MAX_SIZE = (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM + \
                      (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM)
# FIB_TENSOR_MAX_SIZE = 16*10*10*1000*1000

# # calculate verification (to ensure correctness)
# def verify_constants():
#     """verify constants calculation"""
#     print("=== verify constants ===")
#     print(f"MAX_SW_NUM: {MAX_SW_NUM}")
#     print(f"MAX_NET_NPU_NUM: {MAX_NET_NPU_NUM}")
#     print(f"MAX_ONE_SW_PORT_NUM: {MAX_ONE_SW_PORT_NUM}")
#     print(f"MAX_ALL_PORT_NUM: {MAX_ALL_PORT_NUM}")
#     print(f"MAX_NODES: {MAX_NODES}")
#     print(f"FIB_TENSOR_MAX_SIZE: {FIB_TENSOR_MAX_SIZE}")
    
#     # verify calculation
#     expected_all_port = 320 * 32 + 1024 * 2
#     expected_nodes = 320 + 1024
#     expected_fib_size = (320 + 1024) * 1024 * 32 + (320 + 1024) * 1024
    
#     assert MAX_ALL_PORT_NUM == expected_all_port, f"MAX_ALL_PORT_NUM error: {MAX_ALL_PORT_NUM} != {expected_all_port}"
#     assert MAX_NODES == expected_nodes, f"MAX_NODES error: {MAX_NODES} != {expected_nodes}"
#     assert FIB_TENSOR_MAX_SIZE == expected_fib_size, f"FIB_TENSOR_MAX_SIZE error: {FIB_TENSOR_MAX_SIZE} != {expected_fib_size}"
    
#     print(f"FIB tensor size calculation: 1344 * 1024 * 32 + 1344 * 1024 = {expected_fib_size}")
#     print(f"about {expected_fib_size * 4 / 1024 / 1024:.1f} MB memory")
#     print("✓ all constants verification passed")

# if __name__ == "__main__":
#     verify_constants() 
