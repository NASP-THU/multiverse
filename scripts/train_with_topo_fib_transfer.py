# -*- coding: utf-8 -*-
# NOTE: torch and madrona_escape_room are imported later to avoid CUDA fork issues
import time
import numpy as np
import argparse
import math
import pickle
import os
import sys
from pathlib import Path
import warnings
from gen_fib_flex_topo import parse_topo, build_global_routing_table_with_array

# Import topo and fib data transfer helpers
from topo_fib_helper import (
    TopoData, FibData,
    convert_madrona_topo_to_topo_data, 
    topo_to_tensor, 
    fib_to_tensor
)

from parse_chakra import Node,Attribute,BoolList,Parse
from node_conversion import folder_to_int_array

enable_net_log=True

def print_net_log(*args, **kwargs):
    if enable_net_log:
        print(*args, **kwargs)

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument('--ckpt-dir', type=str, required=True)
arg_parser.add_argument('--restore', type=int)
arg_parser.add_argument('--num-worlds', type=int, required=True)
arg_parser.add_argument('--num_updates', type=int, required=True)
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
arg_parser.add_argument('--topology-file', type=str, default='fattree_24_3456g_12gps_100Gbps_H100_no_scale_up', 
                       help='Topology file name (without path)')

arg_parser.add_argument('--workload-folder', type=str, default='special_modify_json', help='Workload folder name')

arg_parser.add_argument('--run_model', type=int, default=0, help='Run model')
    # enum RunModel : uint64_t
# {
#     SYS = 0,
#     NET = 1,
#     MIX = 2,
# }


args = arg_parser.parse_args()

if args.run_model == 0:
    print_net_log("Running model: SYS")
elif args.run_model == 1:
    print_net_log("Running model: NET")
elif args.run_model == 2:
    print_net_log("Running model: MIX")
else:
    print_net_log("Invalid run model")
    exit(1)

print_net_log(f"gpu id id: {args.gpu_id}")
print_net_log(f"CC Method: {args.cc_method}")
print_net_log(f"Topology file: {args.topology_file}")
print_net_log(f"Max Equivalent Path Num: {args.topology_file.split('_')[1]}")

# Parse topology and initialize Topo struct
topology_file = args.topology_file


#aj_link, link_num, net_npu_num, sw_num, port_num, switch_next_hop_port, net_npu_next_hop_port, sw_port_num, net_npu_port_num, graph, port_mapping, _ = parse_topo(topology_file)

#_, global_routing_table_array, next_hop_num = build_global_routing_table_with_array(graph, port_mapping)





# Define NPU configuration early (needed for tensor sizing)
ring_dim0 = 4
ring_dim1 = 2
ring_dim2 = 16
ring_dim0 = 4
ring_dim1 = 2
ring_dim2 = 16
chunks_num = 1
npu_num = 128  # Total number of NPUs
chakra_nodes_data_length = 50000

# ============================================================
# CRITICAL: Process JSON files BEFORE importing CUDA modules
# to avoid fork() deadlock with CUDA context
# ============================================================
data_tensor = None
if args.run_model == 0 or args.run_model == 2:
    print_net_log("=" * 80)
    print_net_log("Step 1: Processing JSON files (before CUDA initialization)...")
    print_net_log("=" * 80)
    
    current_dir = os.path.dirname(os.path.abspath(__file__))
    workload_path = os.path.join(current_dir, "input", args.workload_folder)
    print("workload path: ", workload_path)

    # Process all JSON files using multiprocessing (or single-threaded if needed)
    # Set max_workers=0 to force single-threaded mode
    data = folder_to_int_array(workload_path, max_workers=0)  # FORCE SINGLE-THREADED

    print_net_log("=== Chakra Nodes Data Size Information ===")
    print_net_log(f"Number of files processed: {len(data)}")
    if len(data) > 0:
        total_elements = sum(len(file_data) for file_data in data)
        print_net_log(f"Total elements across all files: {total_elements:,}")
        print_net_log(f"Average elements per file: {total_elements/len(data):.1f}")
        print_net_log(f"Min elements in a file: {min(len(file_data) for file_data in data):,}")
        print_net_log(f"Max elements in a file: {max(len(file_data) for file_data in data):,}")
        
        # calculate memory usage (assuming each element is 4 bytes)
        total_memory_mb = total_elements * 4 / 1024 / 1024
        print_net_log(f"Estimated memory usage: {total_memory_mb:.2f} MB ({total_memory_mb/1024:.2f} GB)")
    else:
        print_net_log("No data found!")
    print_net_log("=== End Chakra Nodes Data Size Information ===\n")

# ============================================================
# NOW we can safely import CUDA modules
# ============================================================
print_net_log("=" * 80)
print_net_log("Step 2: Importing CUDA modules...")
print_net_log("=" * 80)
import torch
import madrona_escape_room

torch.manual_seed(0)

# Define tensor conversion functions (need torch imported)
if args.run_model == 0 or args.run_model == 2:
    chakra_nodes_num = npu_num
    
    def empty_tensor(rows=chakra_nodes_num, max_len=chakra_nodes_data_length):
        tensor = torch.zeros((rows, max_len), dtype=torch.uint32)
        return tensor

    def ints_to_tensor(int_array, num_world=1, rows=chakra_nodes_num, max_len=chakra_nodes_data_length):
        i = 0
        # Create uint32 tensor first (to handle values > INT32_MAX)
        tensor_uint32 = torch.zeros((num_world, rows, max_len), dtype=torch.uint32)
        for array in int_array:
            tensor_uint32[0, i, :len(array)] = torch.tensor(array, dtype=torch.uint32)
            i += 1
        # Return as int32 view (binary representation unchanged, framework requires int32)
        return tensor_uint32.view(dtype=torch.int32)

    def tensor_to_ints(tensor):
        encoded = tensor.cpu().numpy().tolist()
        return encoded

    # Convert data to tensor
    data_tensor = ints_to_tensor(data)
    print_net_log("data_tensor[0]: ", data_tensor[0][0][:100])
    print_net_log("data_tensor[1]: ", data_tensor[0][1][:100])

print_net_log("=" * 80)
print_net_log("Step 3: Initializing simulation manager...")
print_net_log("=" * 80)
sim = madrona_escape_room.SimManager(
    exec_mode = madrona_escape_room.madrona.ExecMode.CUDA if args.gpu_sim else madrona_escape_room.madrona.ExecMode.CPU,
    gpu_id = args.gpu_id,
    num_worlds = args.num_worlds,
    rand_seed = 5,
    auto_reset = True,
    k_aray = args.fattree_K,
    # max_equivalent_path_num = args.topology_file.split('_')[1],
    max_equivalent_path_num = 128,
    cc_method = args.cc_method
)

sim.parseTopologyFile(topology_file)


if args.run_model == 0 or args.run_model == 2:
    
    # ========== Set ProcessParams data ==========
    print_net_log("4. Setting ProcessParams data...")

    # parameters for parallel group size
    # Ring parameters for collective communication
    # Note: ring_dim0, ring_dim1, ring_dim2, chunks_num, and npu_num are already defined at the top
    # ring_dim0 = 8
    # ring_dim1 = 16
    # ring_dim2 = 8
    # chunks_num = 1
    # npu_num=3456 #1024
    all_npu_node_finish=0
    ring_imp=0
    tree_imp=1
    # enum CollectiveCommType : uint64_t
    # {
    #     ALL_REDUCE = 0,
    #     REDUCE = 1,
    #     ALL_GATHER = 2,
    #     GATHER = 3,
    #     SCATTER = 4,
    #     BROADCAST = 5,
    #     ALL_TO_ALL = 6,
    #     REDUCE_SCATTER = 7,
    #     REDUCE_SCATTER_BLOCK = 8,
    #     BARRIER = 9
    # };
    ALL_REDUCE = ring_imp
    REDUCE = ring_imp
    ALL_GATHER = ring_imp
    GATHER = ring_imp
    SCATTER = ring_imp
    BROADCAST = ring_imp
    ALL_TO_ALL = ring_imp
    REDUCE_SCATTER = ring_imp
    REDUCE_SCATTER_BLOCK = ring_imp
    BARRIER = ring_imp



    # Get ProcessParams tensor from simulator
    try:
        process_params_tensor = sim.processParams_tensor().to_torch()
        print_net_log(f"   ProcessParams tensor shape: {process_params_tensor.shape}")
        print_net_log(f"   ProcessParams tensor device: {process_params_tensor.device}, dtype: {process_params_tensor.dtype}")
        
        # Set ring parameters in tensor
        # Layout: [0] = ring_dim0, [1] = ring_dim1, [2] = ring_dim2, [3] = chunks_num
        # // ring form 0
        # // npu_num from 100
        # // coll-imp from 200
        # // finish from 999
        for world_idx in range(args.num_worlds):
            process_params_tensor[world_idx, 0] = ring_dim0
            process_params_tensor[world_idx, 1] = ring_dim1
            process_params_tensor[world_idx, 2] = ring_dim2
            process_params_tensor[world_idx, 3] = chunks_num
            process_params_tensor[world_idx, 100] = npu_num
            process_params_tensor[world_idx, 999] = 0
            # ALL_REDUCE = ring_imp
            # REDUCE = ring_imp
            # ALL_GATHER = ring_imp
            # GATHER = ring_imp
            # SCATTER = ring_imp
            # BROADCAST = ring_imp
            # ALL_TO_ALL = ring_imp
            # REDUCE_SCATTER = ring_imp
            # REDUCE_SCATTER_BLOCK = ring_imp
            # BARRIER ring_imp
            process_params_tensor[world_idx, 200] = ALL_REDUCE
            process_params_tensor[world_idx, 201] = REDUCE
            process_params_tensor[world_idx, 202] = ALL_GATHER
            process_params_tensor[world_idx, 203] = GATHER
            process_params_tensor[world_idx, 204] = SCATTER
            process_params_tensor[world_idx, 205] = BROADCAST
            process_params_tensor[world_idx, 206] = ALL_TO_ALL
            process_params_tensor[world_idx, 207] = REDUCE_SCATTER
            process_params_tensor[world_idx, 208] = REDUCE_SCATTER_BLOCK
            process_params_tensor[world_idx, 209] = BARRIER
            process_params_tensor[world_idx, 999] = 0
            # Remaining elements stay as 0 (reserved for future use)
        
        print_net_log(f"   ✓ ProcessParams data set for {args.num_worlds} worlds")
        print_net_log(f"   ring_dim0={ring_dim0}, ring_dim1={ring_dim1}, ring_dim2={ring_dim2}, chunks_num={chunks_num}, npu_num={npu_num}")
        
        # Verify data setting
        test_world = 0
        retrieved_params = process_params_tensor[test_world, :5].cpu().numpy()
        print_net_log(f"   Verification - retrieved params: {retrieved_params}")
        
    except Exception as e:
        print_net_log(f"   ✗ ProcessParams data setting failed: {e}")

    print_net_log("=== ProcessParams data setting completed ===\n")



if args.run_model == 0 or args.run_model == 2:

    sim.chakra_nodes_data_tensor().to_torch().copy_(data_tensor)
    int_tensor=tensor_to_ints(sim.chakra_nodes_data_tensor().to_torch())
    print_net_log("int_tensor[0]: ", int_tensor[0][0][:100])
    print_net_log("int_tensor[1]: ", int_tensor[0][1][:100])

# original training logic continues

FRAME_LEN = 1000
start = time.time()
total_frame_time = 0.0


print("num_updates:", args.num_updates, "\n")

i=0
while True:
    
    if (i >= args.num_updates):
        break
    
    frame_start = time.time()
    if i%10==0:
        print(f"\n-------- frame {i} ({(i)*FRAME_LEN} -- {(i+1)*FRAME_LEN}) --------")
    
    # check or update tensor data before each step
    if i == 0:
        print("First step, tensor data is ready")
    
    sim.step()
    
    frame_end = time.time()
    frame_time = frame_end - frame_start
    if i != 0:
        total_frame_time += frame_time
    # add dynamic update logic for tensor data
    # e.g. update routing table, topology changes, etc.
    if enable_net_log:
        if i%100==0:
            print(f"frame {i+1} completed, frame time cost: {frame_time:.3f}s, total frame time cost(without first frame): {total_frame_time:.3f}s")

    if i%100==20:
        process_params_tensor = sim.processParams_tensor().to_torch()
        if process_params_tensor[0, 999]==1: # all npu tasks are finished
            print_net_log(f"cpu end : all npu tasks are finished, at sim time: {(i+1) * FRAME_LEN} ns")
            break
    i+=1

    # # output tensor state every certain frames
    # if (i + 1) % 100 == 0:
    #     try:
    #         current_topo = sim.topo_tensor().to_torch()[0, :10]  # check the first 10 elements
    #         print_net_log(f"   current topo tensor first 10 elements: {current_topo.cpu().numpy()}")
    #     except:
    #         pass
    
end = time.time()
total_time = end - start
print(f"Simulation is completed!, total runtime(clock time) is: {total_time:.3f}s")
