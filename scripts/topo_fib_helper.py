import numpy as np
import torch
import struct
import time

# Import constants consistent with C++ macro definitions
from constants import (
    MAX_SW_NUM, MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM, MAX_ALL_PORT_NUM, MAX_NODES,
    AJ_LINK_MAX_SIZE, AJ_LINK_FIELDS, TOPO_TENSOR_MAX_SIZE, FIB_TENSOR_MAX_SIZE
)


class TopoData:
    """Class for encapsulating topology data"""
    def __init__(self, aj_link=None, link_num=0, net_npu_num=0, sw_num=0, 
                 port_num=None, next_hop_port=None, sw_port_num=0, net_npu_port_num=0):
        # 2D array aj_link[AJ_LINK_MAX_SIZE][AJ_LINK_FIELDS] - consistent with C++ int32_t
        self.aj_link = aj_link if aj_link is not None else np.zeros((AJ_LINK_MAX_SIZE, AJ_LINK_FIELDS), dtype=np.int32)
        
        # Scalar values
        self.link_num = link_num
        self.net_npu_num = net_npu_num
        self.sw_num = sw_num
        self.sw_port_num = sw_port_num
        self.net_npu_port_num = net_npu_port_num
        
        # 1D array, maximum length is MAX_NODES = MAX_SW_NUM + MAX_NET_NPU_NUM
        self.port_num = port_num if port_num is not None else np.zeros(MAX_NODES, dtype=np.int32)
        
        # 1D array, maximum length is MAX_ALL_PORT_NUM = MAX_SW_NUM*MAX_ONE_SW_PORT_NUM + MAX_NET_NPU_NUM*2
        self.next_hop_port = next_hop_port if next_hop_port is not None else np.zeros(MAX_ALL_PORT_NUM, dtype=np.int32)

    def to_flat_array(self):
        """Convert TopoData to flat integer array"""
        flat_array = []
        
        # 1. Scalar values (6 values)
        flat_array.extend([
            self.link_num,
            self.net_npu_num, 
            self.sw_num,
            self.sw_port_num,
            self.net_npu_port_num,
            0  # Reserved field for alignment
        ])
        
        # 2. aj_link 2D array (AJ_LINK_MAX_SIZE * AJ_LINK_FIELDS elements)
        flat_array.extend(self.aj_link.flatten().tolist())
        
        # 3. port_num 1D array (MAX_NODES elements)
        flat_array.extend(self.port_num.tolist())
        
        # 4. next_hop_port 1D array (MAX_ALL_PORT_NUM elements)
        flat_array.extend(self.next_hop_port.tolist())
        
        return flat_array

    @classmethod
    def from_flat_array(cls, flat_array):
        """Reconstruct TopoData from flat array"""
        if len(flat_array) < 6:
            raise ValueError("Array length insufficient")
            
        # 1. Read scalar values
        link_num = flat_array[0]
        net_npu_num = flat_array[1]
        sw_num = flat_array[2]
        sw_port_num = flat_array[3]
        net_npu_port_num = flat_array[4]
        # flat_array[5] is reserved field
        
        offset = 6
        
        # 2. Read aj_link
        aj_link_size = AJ_LINK_MAX_SIZE * AJ_LINK_FIELDS
        if len(flat_array) < offset + aj_link_size:
            raise ValueError("Insufficient aj_link data")
        aj_link_flat = flat_array[offset:offset + aj_link_size]
        aj_link = np.array(aj_link_flat, dtype=np.int32).reshape((AJ_LINK_MAX_SIZE, AJ_LINK_FIELDS))
        offset += aj_link_size
        
        # 3. Read port_num
        port_num_size = MAX_NODES
        if len(flat_array) < offset + port_num_size:
            raise ValueError("Insufficient port_num data")
        port_num = np.array(flat_array[offset:offset + port_num_size], dtype=np.int32)
        offset += port_num_size
        
        # 4. Read next_hop_port
        next_hop_port_size = MAX_ALL_PORT_NUM
        if len(flat_array) < offset + next_hop_port_size:
            raise ValueError("Insufficient next_hop_port data")
        next_hop_port = np.array(flat_array[offset:offset + next_hop_port_size], dtype=np.int32)
        
        return cls(aj_link=aj_link, link_num=link_num, net_npu_num=net_npu_num,
                   sw_num=sw_num, port_num=port_num, next_hop_port=next_hop_port,
                   sw_port_num=sw_port_num, net_npu_port_num=net_npu_port_num)


class FibData:
    """Class for encapsulating routing table data"""
    def __init__(self, fib=None, next_hop_num=None):
        # 3D array fib[MAX_NODES][MAX_NODES][MAX_ONE_SW_PORT_NUM] - consistent with C++ side
        self.fib = fib if fib is not None else np.full((MAX_NODES, MAX_NODES, MAX_ONE_SW_PORT_NUM), 255, dtype=np.uint8)
        
        # 2D array next_hop_num[MAX_NODES][MAX_NODES] - consistent with C++ side
        self.next_hop_num = next_hop_num if next_hop_num is not None else np.zeros((MAX_NODES, MAX_NODES), dtype=np.uint8)

    def to_flat_array(self):
        """Convert FibData to flat integer array"""
        flat_array = []
        
        # 1. fib 3D array (MAX_NODES * MAX_NODES * MAX_ONE_SW_PORT_NUM elements)
        flat_array.extend(self.fib.flatten().tolist())
        
        # 2. next_hop_num 2D array (MAX_NODES * MAX_NODES elements)
        flat_array.extend(self.next_hop_num.flatten().tolist())
        
        return flat_array

    @classmethod 
    def from_flat_array(cls, flat_array):
        """Reconstruct FibData from flat array"""
        # 1. Read fib
        fib_size = MAX_NODES * MAX_NODES * MAX_ONE_SW_PORT_NUM
        if len(flat_array) < fib_size:
            raise ValueError("Insufficient fib data")
        fib_flat = flat_array[0:fib_size]
        fib = np.array(fib_flat, dtype=np.uint8).reshape((MAX_NODES, MAX_NODES, MAX_ONE_SW_PORT_NUM))
        
        # 2. Read next_hop_num
        next_hop_num_size = MAX_NODES * MAX_NODES
        if len(flat_array) < fib_size + next_hop_num_size:
            raise ValueError("Insufficient next_hop_num data")
        next_hop_num_flat = flat_array[fib_size:fib_size + next_hop_num_size]
        next_hop_num = np.array(next_hop_num_flat, dtype=np.uint8).reshape((MAX_NODES, MAX_NODES))
        
        return cls(fib=fib, next_hop_num=next_hop_num)


def topo_to_tensor(topo_data, max_len=TOPO_TENSOR_MAX_SIZE):
    """Convert TopoData to tensor"""
    time_start = time.time()
    int_array = topo_data.to_flat_array()
    time_end = time.time()
    print(f"time cost for to_flat_array: {time_end - time_start:.3f}s")
    
    time_start = time.time()
    if len(int_array) > max_len:
        raise ValueError(f"TopoData size {len(int_array)} exceeds maximum length {max_len}")
    
    time_start = time.time()
    tensor = torch.zeros(max_len, dtype=torch.int32)
    tensor[:len(int_array)] = torch.tensor(int_array, dtype=torch.int32)
    time_end = time.time()
    print(f"time cost for torch.tensor: {time_end - time_start:.3f}s")
    
    return tensor


def tensor_to_topo(tensor):
    """Convert tensor back to TopoData"""
    int_array = tensor.cpu().numpy().tolist()
    # Remove trailing zeros
    while int_array and int_array[-1] == 0:
        int_array.pop()
    
    return TopoData.from_flat_array(int_array)


def fib_to_tensor(fib_data, max_len=FIB_TENSOR_MAX_SIZE):
    """Convert FibData to tensor"""
    time_start = time.time()
    int_array = fib_data.to_flat_array()
    time_end = time.time()
    print(f"time cost for to_flat_array: {time_end - time_start:.3f}s")
    
    if len(int_array) > max_len:
        raise ValueError(f"FibData size {len(int_array)} exceeds maximum length {max_len}")
    
    time_start = time.time()
    tensor = torch.zeros(max_len, dtype=torch.uint8)
    time_end = time.time()
    print(f"time cost for torch.zeros: {time_end - time_start:.3f}s")
    
    time_start = time.time()
    tensor[:len(int_array)] = torch.tensor(int_array, dtype=torch.uint8)
    time_end = time.time()
    print(f"time cost for tensor[:len(int_array)] = torch.tensor(int_array, dtype=torch.uint8): {time_end - time_start:.3f}s")
    
    return tensor


def tensor_to_fib(tensor):
    """Convert tensor back to FibData"""
    int_array = tensor.cpu().numpy().astype(np.uint8).tolist()
    # Remove trailing zeros
    while int_array and int_array[-1] == 0:
        int_array.pop()
    
    return FibData.from_flat_array(int_array)


def convert_madrona_topo_to_topo_data(madrona_topo):
    """Convert madrona's Topo object to TopoData"""
    return TopoData(
        aj_link=np.array(madrona_topo.aj_link, dtype=np.int32),  # Fix: maintain consistency with C++ int32_t
        link_num=madrona_topo.link_num,
        net_npu_num=madrona_topo.net_npu_num,
        sw_num=madrona_topo.sw_num,
        port_num=np.array(madrona_topo.port_num, dtype=np.int32),
        next_hop_port=np.array(madrona_topo.next_hop_port, dtype=np.int32),
        sw_port_num=madrona_topo.sw_port_num,
        net_npu_port_num=madrona_topo.net_npu_port_num
    )


def convert_madrona_fib_to_fib_data(madrona_fib):
    """Convert madrona's Fib object to FibData"""
    return FibData(
        fib=np.array(madrona_fib.fib, dtype=np.uint8),
        next_hop_num=np.array(madrona_fib.next_hop_num, dtype=np.uint8)
    )


# Helper functions for large capacity data transfer
def create_large_tensor(size_mb=100):
    """Create empty tensor of specified size (MB)"""
    # int32 = 4 bytes
    num_elements = size_mb * 1024 * 1024 // 4
    return torch.zeros(num_elements, dtype=torch.int32)


def split_large_data(int_array, chunk_size=1000000):
    """Split large data into multiple chunks"""
    chunks = []
    for i in range(0, len(int_array), chunk_size):
        chunks.append(int_array[i:i + chunk_size])
    return chunks


def merge_data_chunks(chunks):
    """Merge data chunks"""
    result = []
    for chunk in chunks:
        result.extend(chunk)
    return result 