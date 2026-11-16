import torch
import madrona_escape_room
import time
import numpy as np
import argparse
import math
import pickle
import os
import sys
from pathlib import Path
import warnings

from madrona_escape_room_learn import (
    train, profile, TrainConfig, PPOConfig, SimInterface,
)

from policy import make_policy, setup_obs
from gen_fib_flex_topo import parse_topo, build_global_routing_table_with_array

# 导入topo和fib数据传递助手
from topo_fib_helper import (
    TopoData, FibData,
    convert_madrona_topo_to_topo_data, 
    topo_to_tensor, 
    fib_to_tensor
)

torch.manual_seed(0)




arg_parser = argparse.ArgumentParser()
arg_parser.add_argument('--gpu-id', type=int, default=1)
arg_parser.add_argument('--ckpt-dir', type=str, required=True)
arg_parser.add_argument('--restore', type=int)

arg_parser.add_argument('--num-worlds', type=int, required=True)
arg_parser.add_argument('--num-updates', type=int, required=True)
arg_parser.add_argument('--steps-per-update', type=int, default=40)
arg_parser.add_argument('--num-bptt-chunks', type=int, default=8)

arg_parser.add_argument('--lr', type=float, default=1e-4)
arg_parser.add_argument('--gamma', type=float, default=0.998)
arg_parser.add_argument('--entropy-loss-coef', type=float, default=0.01)
arg_parser.add_argument('--value-loss-coef', type=float, default=0.5)
arg_parser.add_argument('--clip-value-loss', action='store_true')

arg_parser.add_argument('--num-channels', type=int, default=256)
arg_parser.add_argument('--separate-value', action='store_true')
arg_parser.add_argument('--fp16', action='store_true')

arg_parser.add_argument('--gpu-sim', action='store_true')
arg_parser.add_argument('--profile-report', action='store_true')

arg_parser.add_argument('--gpu_id', type=int, required=True, help='GPU ID')
arg_parser.add_argument('--fattree_K', type=int, required=True, help='Fattree K value')
arg_parser.add_argument('--cc_method', type=int, required=True, help='Congestion control method')

args = arg_parser.parse_args()

print(f"gpu id id: {args.gpu_id}")
# print(f"Fattree K: {args.fattree_K}")
print(f"CC Method: {args.cc_method}")

# Parse topology and initialize Topo struct
# topology_file = "fattree_4_16g_2gps_100Gbps_H100_no_scale_up"
topology_file = "Spectrum-X_128g_8gps_100Gbps_H100_no_scale_up_test"
# topology_file = "fattree_16_1024g_8gps_100Gbps_H100_no_scale_up"

aj_link, link_num, net_npu_num, sw_num, port_num, switch_next_hop_port, net_npu_next_hop_port, sw_port_num, net_npu_port_num, graph, port_mapping, _ = parse_topo(topology_file)

# print("sw_num: ", sw_num)

topo = madrona_escape_room.Topo()
topo.link_num = link_num
topo.net_npu_num = net_npu_num
topo.sw_num = sw_num
topo.sw_port_num = sw_port_num
topo.net_npu_port_num = net_npu_port_num

# Assign aj_link
topo.aj_link[:len(aj_link)] = np.array(aj_link, dtype=np.int16)
# Assign port_num
topo.port_num[:len(port_num)] = np.array(port_num, dtype=np.int16)
# Combine switch_next_hop_port and net_npu_next_hop_port into next_hop_port

# combined_next_hop_port's length equal to the real length of next_hop_port
combined_next_hop_port = np.array(net_npu_next_hop_port[0:net_npu_port_num]+switch_next_hop_port[net_npu_port_num:net_npu_port_num+sw_port_num], dtype=np.int16)
topo.next_hop_port[:len(combined_next_hop_port)] = combined_next_hop_port
# print("net_npu_port_num: ", net_npu_port_num, "sw_port_num: ", sw_port_num)
# print("next_hop_port\n", len(topo.next_hop_port), topo.next_hop_port)

# Build global routing table and assign to Fib
time_start = time.time()
_, global_routing_table_array, next_hop_num = build_global_routing_table_with_array(graph, port_mapping)
time_end = time.time()
print(f"time cost for building global routing table: {time_end - time_start:.3f}s")

fib = madrona_escape_room.Fib()

# no longer directly access fib.fib property, but create an appropriate sized array and pass data through tensor
# use our constants to determine the correct size
from constants import MAX_SW_NUM, MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM

# create routing table array
max_nodes = MAX_SW_NUM + MAX_NET_NPU_NUM
adjusted_routing_table = np.full((max_nodes, max_nodes, MAX_ONE_SW_PORT_NUM), -1, dtype=np.int16)

# fill actual routing data
for i in range(min(len(global_routing_table_array), max_nodes)):
    for j in range(min(len(global_routing_table_array[i]), max_nodes)):
        route_len = min(len(global_routing_table_array[i][j]), MAX_ONE_SW_PORT_NUM)
        if route_len > 0:
            adjusted_routing_table[i, j, :route_len] = global_routing_table_array[i][j][:route_len]

# create next_hop_num array
adjusted_next_hop_num = np.zeros((max_nodes, max_nodes), dtype=np.int16)
next_hop_num_np = np.array(next_hop_num, dtype=np.int16)
for i in range(min(next_hop_num_np.shape[0], max_nodes)):
    for j in range(min(next_hop_num_np.shape[1], max_nodes)):
        adjusted_next_hop_num[i, j] = next_hop_num_np[i, j]

print(f"routing table size: {adjusted_routing_table.shape}")
print(f"next_hop_num size: {adjusted_next_hop_num.shape}")
print(f"sampled next hop num: fib[1] = {adjusted_next_hop_num[1][:5]}")  # show first 5 elements

# ========== new: convert data and create tensor ==========
print("\n=== start TopoFib data tensorization and passing ===")

# 1. convert madrona data structure to our data structure
print("1. convert data format...")
topo_data = convert_madrona_topo_to_topo_data(topo)

# create FibData object and fill prepared data
fib_data = FibData()
fib_data.fib = adjusted_routing_table
fib_data.next_hop_num = adjusted_next_hop_num

print(f"   TopoData size: {len(topo_data.to_flat_array())} elements")
print(f"   FibData size: {len(fib_data.to_flat_array())} elements")

# 2. convert to tensor
print("2. create tensor...")
topo_tensor = topo_to_tensor(topo_data)
fib_tensor = fib_to_tensor(fib_data)

# calculate data size
topo_size_mb = topo_tensor.numel() * 4 / 1024 / 1024
fib_size_mb = fib_tensor.numel() * 4 / 1024 / 1024
total_size_mb = topo_size_mb + fib_size_mb

print(f"   Topo tensor: {topo_tensor.shape}, {topo_size_mb:.2f} MB")
print(f"   Fib tensor: {fib_tensor.shape}, {fib_size_mb:.2f} MB")
print(f"   Total data size: {total_size_mb:.2f} MB")

if total_size_mb > 1000:  # if larger than 1GB
    print(f"   Warning: data size is large ({total_size_mb:.2f} MB), please ensure enough memory")

# ===============================================
# create SimManager
print("3. create SimManager and create Agent entity for data passing...")
sim = madrona_escape_room.SimManager(
    exec_mode = madrona_escape_room.madrona.ExecMode.CUDA if args.gpu_sim else madrona_escape_room.madrona.ExecMode.CPU,
    gpu_id = args.gpu_id,
    num_worlds = args.num_worlds,
    rand_seed = 5,
    auto_reset = True,
    k_aray = args.fattree_K,
    cc_method = args.cc_method
)

# ========== pass topo and fibdata to C++ side==========
print("4. pass topo and fib data to C++ side...")

try:
    # get tensor reference from C++ side
    sim_topo_tensor = sim.topo_tensor().to_torch()
    sim_fib_tensor = sim.fib_tensor().to_torch()
    
    print(f"   C++ side topo tensor shape: {sim_topo_tensor.shape}")
    print(f"   C++ side fib tensor shape: {sim_fib_tensor.shape}")
    
    # check tensor size compatibility
    if topo_tensor.shape[0] > sim_topo_tensor.shape[1]:
        raise ValueError(f"Topo data is too large: {topo_tensor.shape[0]} > {sim_topo_tensor.shape[1]}")
    if fib_tensor.shape[0] > sim_fib_tensor.shape[1]:
        raise ValueError(f"Fib data is too large: {fib_tensor.shape[0]} > {sim_fib_tensor.shape[1]}")
    
    # pass data (support multiple worlds)
    for world_idx in range(args.num_worlds):
        sim_topo_tensor[world_idx, :topo_tensor.shape[0]] = topo_tensor
        sim_fib_tensor[world_idx, :fib_tensor.shape[0]] = fib_tensor
    
    print(f"   ✓ successfully pass data to {args.num_worlds} worlds")
    
    # verify data passing
    print("5. verify data passing...")
    test_world = 0
    retrieved_topo = sim_topo_tensor[test_world, :len(topo_data.to_flat_array())].cpu().numpy()
    retrieved_fib = sim_fib_tensor[test_world, :len(fib_data.to_flat_array())].cpu().numpy()
    
    original_topo = np.array(topo_data.to_flat_array())
    original_fib = np.array(fib_data.to_flat_array())
    
    topo_match = np.array_equal(retrieved_topo, original_topo)
    fib_match = np.array_equal(retrieved_fib, original_fib)
    
    print(f"   Topo data verification: {'✓ passed' if topo_match else '✗ failed'}")
    print(f"   Fib data verification: {'✓ passed' if fib_match else '✗ failed'}")
    
    if not (topo_match and fib_match):
        print("   Warning: data verification failed, please check tensor passing logic") 
    
except Exception as e:
    print(f"   ✗ data passing failed: {e}")
    print("   This may be because the C++ side has not finished compiling, continue running the original logic...")

print("=== TopoFib data passing completed ===\n")

# =============================================

# # immediately run a step to trigger tensor data reading and parsing
# print("6. trigger tensor data reading and parsing...")
# sim.step()
# print("   ✓ tensor data reading and parsing step completed\n")

# original training logic continues
FRAME_LEN = 1000
start = time.time()
for i in range(args.num_updates):
    frame_start = time.time()
    print(f"\n-------- frame {i} ({(i)*FRAME_LEN} -- {(i+1)*FRAME_LEN}) --------")
    
    # check or update tensor data before each step
    if i == 0:
        print("First step, tensor data is ready")
    
    sim.step()
    
    frame_end = time.time()
    frame_time = frame_end - frame_start
    
    # add dynamic update logic for tensor data
    # e.g. update routing table, topology changes, etc.
    print(f"frame {i+1} completed, frame time cost: {frame_time:.3f}s")


    # # output tensor state every certain frames
    # if (i + 1) % 100 == 0:
    #     try:
    #         current_topo = sim.topo_tensor().to_torch()[0, :10]  # check the first 10 elements
    #         print(f"   current topo tensor first 10 elements: {current_topo.cpu().numpy()}")
    #     except:
    #         pass
end = time.time()
total_time = end - start
print(f"Simulation is completed!, total time cost: {total_time:.3f}s")

# # optional: save final tensor state
# try:
#     final_topo_tensor = sim.topo_tensor().to_torch().cpu()
#     final_fib_tensor = sim.fib_tensor().to_torch().cpu()
    
#     # save to file (if needed)
#     # torch.save({
#     #     'topo': final_topo_tensor,
#     #     'fib': final_fib_tensor
#     # }, 'final_topo_fib_state.pt')
    
#     print("Final tensor state is ready to save (if needed)")
# except:
#     pass 