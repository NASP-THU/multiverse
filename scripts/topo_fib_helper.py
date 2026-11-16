import numpy as np
import torch
import struct

# 导入与C++宏定义一致的常量
from constants import (
    MAX_SW_NUM, MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM, MAX_ALL_PORT_NUM, MAX_NODES,
    AJ_LINK_MAX_SIZE, AJ_LINK_FIELDS, TOPO_TENSOR_MAX_SIZE, FIB_TENSOR_MAX_SIZE
)


class TopoData:
    """用于封装拓扑数据的类"""
    def __init__(self, aj_link=None, link_num=0, net_npu_num=0, sw_num=0, 
                 port_num=None, next_hop_port=None, sw_port_num=0, net_npu_port_num=0):
        # 二维数组 aj_link[AJ_LINK_MAX_SIZE][AJ_LINK_FIELDS]
        self.aj_link = aj_link if aj_link is not None else np.zeros((AJ_LINK_MAX_SIZE, AJ_LINK_FIELDS), dtype=np.int16)
        
        # 标量值
        self.link_num = link_num
        self.net_npu_num = net_npu_num
        self.sw_num = sw_num
        self.sw_port_num = sw_port_num
        self.net_npu_port_num = net_npu_port_num
        
        # 一维数组，最大长度为 MAX_NODES = MAX_SW_NUM + MAX_NET_NPU_NUM
        self.port_num = port_num if port_num is not None else np.zeros(MAX_NODES, dtype=np.int16)
        
        # 一维数组，最大长度为 MAX_ALL_PORT_NUM = MAX_SW_NUM*MAX_ONE_SW_PORT_NUM + MAX_NET_NPU_NUM*2
        self.next_hop_port = next_hop_port if next_hop_port is not None else np.zeros(MAX_ALL_PORT_NUM, dtype=np.int16)

    def to_flat_array(self):
        """将TopoData转换为扁平的整数数组"""
        flat_array = []
        
        # 1. 标量值 (6个)
        flat_array.extend([
            self.link_num,
            self.net_npu_num, 
            self.sw_num,
            self.sw_port_num,
            self.net_npu_port_num,
            0  # 预留字段，用于对齐
        ])
        
        # 2. aj_link 二维数组 (AJ_LINK_MAX_SIZE * AJ_LINK_FIELDS个元素)
        flat_array.extend(self.aj_link.flatten().tolist())
        
        # 3. port_num 一维数组 (MAX_NODES个元素)
        flat_array.extend(self.port_num.tolist())
        
        # 4. next_hop_port 一维数组 (MAX_ALL_PORT_NUM个元素)
        flat_array.extend(self.next_hop_port.tolist())
        
        return flat_array

    @classmethod
    def from_flat_array(cls, flat_array):
        """从扁平数组重构TopoData"""
        if len(flat_array) < 6:
            raise ValueError("数组长度不足")
            
        # 1. 读取标量值
        link_num = flat_array[0]
        net_npu_num = flat_array[1]
        sw_num = flat_array[2]
        sw_port_num = flat_array[3]
        net_npu_port_num = flat_array[4]
        # flat_array[5] 是预留字段
        
        offset = 6
        
        # 2. 读取 aj_link
        aj_link_size = AJ_LINK_MAX_SIZE * AJ_LINK_FIELDS
        if len(flat_array) < offset + aj_link_size:
            raise ValueError("aj_link数据不足")
        aj_link_flat = flat_array[offset:offset + aj_link_size]
        aj_link = np.array(aj_link_flat, dtype=np.int16).reshape((AJ_LINK_MAX_SIZE, AJ_LINK_FIELDS))
        offset += aj_link_size
        
        # 3. 读取 port_num
        port_num_size = MAX_NODES
        if len(flat_array) < offset + port_num_size:
            raise ValueError("port_num数据不足")
        port_num = np.array(flat_array[offset:offset + port_num_size], dtype=np.int16)
        offset += port_num_size
        
        # 4. 读取 next_hop_port
        next_hop_port_size = MAX_ALL_PORT_NUM
        if len(flat_array) < offset + next_hop_port_size:
            raise ValueError("next_hop_port数据不足")
        next_hop_port = np.array(flat_array[offset:offset + next_hop_port_size], dtype=np.int16)
        
        return cls(aj_link=aj_link, link_num=link_num, net_npu_num=net_npu_num,
                   sw_num=sw_num, port_num=port_num, next_hop_port=next_hop_port,
                   sw_port_num=sw_port_num, net_npu_port_num=net_npu_port_num)


class FibData:
    """用于封装路由表数据的类"""
    def __init__(self, fib=None, next_hop_num=None):
        # 三维数组 fib[MAX_NODES][MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM]
        self.fib = fib if fib is not None else np.full((MAX_NODES, MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM), -1, dtype=np.int16)
        
        # 二维数组 next_hop_num[MAX_NODES][MAX_NET_NPU_NUM]
        self.next_hop_num = next_hop_num if next_hop_num is not None else np.zeros((MAX_NODES, MAX_NET_NPU_NUM), dtype=np.int16)

    def to_flat_array(self):
        """将FibData转换为扁平的整数数组"""
        flat_array = []
        
        # 1. fib 三维数组 (MAX_NODES * MAX_NET_NPU_NUM * MAX_ONE_SW_PORT_NUM个元素)
        flat_array.extend(self.fib.flatten().tolist())
        
        # 2. next_hop_num 二维数组 (MAX_NODES * MAX_NET_NPU_NUM个元素)
        flat_array.extend(self.next_hop_num.flatten().tolist())
        
        return flat_array

    @classmethod 
    def from_flat_array(cls, flat_array):
        """从扁平数组重构FibData"""
        # 1. 读取 fib
        fib_size = MAX_NODES * MAX_NET_NPU_NUM * MAX_ONE_SW_PORT_NUM
        if len(flat_array) < fib_size:
            raise ValueError("fib数据不足")
        fib_flat = flat_array[0:fib_size]
        fib = np.array(fib_flat, dtype=np.int16).reshape((MAX_NODES, MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM))
        
        # 2. 读取 next_hop_num
        next_hop_num_size = MAX_NODES * MAX_NET_NPU_NUM
        if len(flat_array) < fib_size + next_hop_num_size:
            raise ValueError("next_hop_num数据不足")
        next_hop_num_flat = flat_array[fib_size:fib_size + next_hop_num_size]
        next_hop_num = np.array(next_hop_num_flat, dtype=np.int16).reshape((MAX_NODES, MAX_NET_NPU_NUM))
        
        return cls(fib=fib, next_hop_num=next_hop_num)


def topo_to_tensor(topo_data, max_len=TOPO_TENSOR_MAX_SIZE):
    """将TopoData转换为tensor"""
    int_array = topo_data.to_flat_array()
    
    if len(int_array) > max_len:
        raise ValueError(f"TopoData大小 {len(int_array)} 超过最大长度 {max_len}")
    
    tensor = torch.zeros(max_len, dtype=torch.int32)
    tensor[:len(int_array)] = torch.tensor(int_array, dtype=torch.int32)
    return tensor


def tensor_to_topo(tensor):
    """将tensor转换回TopoData"""
    int_array = tensor.cpu().numpy().tolist()
    # 移除末尾的零
    while int_array and int_array[-1] == 0:
        int_array.pop()
    
    return TopoData.from_flat_array(int_array)


def fib_to_tensor(fib_data, max_len=FIB_TENSOR_MAX_SIZE):
    """将FibData转换为tensor"""
    int_array = fib_data.to_flat_array()
    
    if len(int_array) > max_len:
        raise ValueError(f"FibData大小 {len(int_array)} 超过最大长度 {max_len}")
    
    tensor = torch.zeros(max_len, dtype=torch.int32)
    tensor[:len(int_array)] = torch.tensor(int_array, dtype=torch.int32)
    return tensor


def tensor_to_fib(tensor):
    """将tensor转换回FibData"""
    int_array = tensor.cpu().numpy().tolist()
    # 移除末尾的零
    while int_array and int_array[-1] == 0:
        int_array.pop()
    
    return FibData.from_flat_array(int_array)


def convert_madrona_topo_to_topo_data(madrona_topo):
    """将madrona的Topo对象转换为TopoData"""
    return TopoData(
        aj_link=np.array(madrona_topo.aj_link, dtype=np.int16),
        link_num=madrona_topo.link_num,
        net_npu_num=madrona_topo.net_npu_num,
        sw_num=madrona_topo.sw_num,
        port_num=np.array(madrona_topo.port_num, dtype=np.int16),
        next_hop_port=np.array(madrona_topo.next_hop_port, dtype=np.int16),
        sw_port_num=madrona_topo.sw_port_num,
        net_npu_port_num=madrona_topo.net_npu_port_num
    )


def convert_madrona_fib_to_fib_data(madrona_fib):
    """将madrona的Fib对象转换为FibData"""
    return FibData(
        fib=np.array(madrona_fib.fib, dtype=np.int16),
        next_hop_num=np.array(madrona_fib.next_hop_num, dtype=np.int16)
    )


# 用于大容量数据传递的辅助函数
def create_large_tensor(size_mb=100):
    """创建指定大小(MB)的空tensor"""
    # int32 = 4 bytes
    num_elements = size_mb * 1024 * 1024 // 4
    return torch.zeros(num_elements, dtype=torch.int32)


def split_large_data(int_array, chunk_size=1000000):
    """将大数据分割成多个块"""
    chunks = []
    for i in range(0, len(int_array), chunk_size):
        chunks.append(int_array[i:i + chunk_size])
    return chunks


def merge_data_chunks(chunks):
    """合并数据块"""
    result = []
    for chunk in chunks:
        result.extend(chunk)
    return result 