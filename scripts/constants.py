"""
与C++头文件types.hpp中宏定义保持一致的Python常量
确保Python代码和C++代码使用相同的数值
"""

# 基础常量 - 与 types.hpp 中的宏定义保持一致
MAX_SW_NUM = 320                # 最大交换机数量
MAX_NET_NPU_NUM = 1024          # 最大网络NPU数量
MAX_ONE_SW_PORT_NUM = 32        # 每个交换机的最大端口数量

# 派生常量
MAX_ALL_PORT_NUM = MAX_SW_NUM * MAX_ONE_SW_PORT_NUM + MAX_NET_NPU_NUM * 2  # 320*32 + 1024*2 = 12288
MAX_NODES = MAX_SW_NUM + MAX_NET_NPU_NUM  # 320 + 1024 = 1344 (总节点数：交换机 + NPU)

# 其他相关常量
MAX_NET_NPU_FLOW_NUM = 2000     # 每个NET_NPU最大并发流数量
MAX_NUM_NEXT_HOP = MAX_ONE_SW_PORT_NUM  # 最大下一跳数量 = 32  
MAX_PATH_LEN = 40               # 最大路径长度

# 用于数组大小的常量
MAX_LINKS_NUM = 10000 
AJ_LINK_MAX_SIZE = MAX_LINKS_NUM         # aj_link数组的最大大小
AJ_LINK_FIELDS = 5              # aj_link每个条目的字段数

# Tensor大小常量 (用于内存分配)
TOPO_TENSOR_MAX_SIZE = (20*MAX_LINKS_NUM)    # topo tensor的最大大小 (增加以适应更大数组)

# FIB tensor size calculation based on data structures (与C++端FIB_TENSOR_SIZE保持一致):
# fib[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM] +
# next_hop_num[MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_NET_NPU_NUM]
FIB_TENSOR_MAX_SIZE = (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM + \
                      (MAX_SW_NUM + MAX_NET_NPU_NUM) * (MAX_SW_NUM + MAX_NET_NPU_NUM)  # = 45,416,448

# # 计算验证 (用于确保计算正确)
# def verify_constants():
#     """验证常量计算是否正确"""
#     print("=== 常量验证 ===")
#     print(f"MAX_SW_NUM: {MAX_SW_NUM}")
#     print(f"MAX_NET_NPU_NUM: {MAX_NET_NPU_NUM}")
#     print(f"MAX_ONE_SW_PORT_NUM: {MAX_ONE_SW_PORT_NUM}")
#     print(f"MAX_ALL_PORT_NUM: {MAX_ALL_PORT_NUM}")
#     print(f"MAX_NODES: {MAX_NODES}")
#     print(f"FIB_TENSOR_MAX_SIZE: {FIB_TENSOR_MAX_SIZE}")
    
#     # 验证计算
#     expected_all_port = 320 * 32 + 1024 * 2
#     expected_nodes = 320 + 1024
#     expected_fib_size = (320 + 1024) * 1024 * 32 + (320 + 1024) * 1024
    
#     assert MAX_ALL_PORT_NUM == expected_all_port, f"MAX_ALL_PORT_NUM错误: {MAX_ALL_PORT_NUM} != {expected_all_port}"
#     assert MAX_NODES == expected_nodes, f"MAX_NODES错误: {MAX_NODES} != {expected_nodes}"
#     assert FIB_TENSOR_MAX_SIZE == expected_fib_size, f"FIB_TENSOR_MAX_SIZE错误: {FIB_TENSOR_MAX_SIZE} != {expected_fib_size}"
    
#     print(f"FIB tensor大小计算: 1344 * 1024 * 32 + 1344 * 1024 = {expected_fib_size}")
#     print(f"约 {expected_fib_size * 4 / 1024 / 1024:.1f} MB内存")
#     print("✓ 所有常量验证通过")

# if __name__ == "__main__":
#     verify_constants() 