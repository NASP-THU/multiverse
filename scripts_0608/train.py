import torch
import madrona_escape_room
import time
import numpy as np

from madrona_escape_room_learn import (
    train, profile, TrainConfig, PPOConfig, SimInterface,
)

from policy import make_policy, setup_obs

import argparse
import math
from pathlib import Path
import warnings
# warnings.filterwarnings("error")

from gen_fib_flex_topo import parse_topo, build_global_routing_table_with_array

torch.manual_seed(0)

class LearningCallback:
    def __init__(self, ckpt_dir, profile_report):
        self.mean_fps = 0
        self.ckpt_dir = ckpt_dir
        self.profile_report = profile_report

    def __call__(self, update_idx, update_time, update_results, learning_state):
        update_id = update_idx + 1
        fps = args.num_worlds * args.steps_per_update / update_time
        self.mean_fps += (fps - self.mean_fps) / update_id

        if update_id != 1 and  update_id % 10 != 0:
            return

        ppo = update_results.ppo_stats

        with torch.no_grad():
            reward_mean = update_results.rewards.mean().cpu().item()
            reward_min = update_results.rewards.min().cpu().item()
            reward_max = update_results.rewards.max().cpu().item()

            value_mean = update_results.values.mean().cpu().item()
            value_min = update_results.values.min().cpu().item()
            value_max = update_results.values.max().cpu().item()

            advantage_mean = update_results.advantages.mean().cpu().item()
            advantage_min = update_results.advantages.min().cpu().item()
            advantage_max = update_results.advantages.max().cpu().item()

            bootstrap_value_mean = update_results.bootstrap_values.mean().cpu().item()
            bootstrap_value_min = update_results.bootstrap_values.min().cpu().item()
            bootstrap_value_max = update_results.bootstrap_values.max().cpu().item()

            vnorm_mu = learning_state.value_normalizer.mu.cpu().item()
            vnorm_sigma = learning_state.value_normalizer.sigma.cpu().item()

        print(f"\nUpdate: {update_id}")
        print(f"    Loss: {ppo.loss: .3e}, A: {ppo.action_loss: .3e}, V: {ppo.value_loss: .3e}, E: {ppo.entropy_loss: .3e}")
        print()
        print(f"    Rewards          => Avg: {reward_mean: .3e}, Min: {reward_min: .3e}, Max: {reward_max: .3e}")
        print(f"    Values           => Avg: {value_mean: .3e}, Min: {value_min: .3e}, Max: {value_max: .3e}")
        print(f"    Advantages       => Avg: {advantage_mean: .3e}, Min: {advantage_min: .3e}, Max: {advantage_max: .3e}")
        print(f"    Bootstrap Values => Avg: {bootstrap_value_mean: .3e}, Min: {bootstrap_value_min: .3e}, Max: {bootstrap_value_max: .3e}")
        print(f"    Returns          => Avg: {ppo.returns_mean}, σ: {ppo.returns_stddev}")
        print(f"    Value Normalizer => Mean: {vnorm_mu: .3e}, σ: {vnorm_sigma :.3e}")

        if self.profile_report:
            print()
            print(f"    FPS: {fps:.0f}, Update Time: {update_time:.2f}, Avg FPS: {self.mean_fps:.0f}")
            print(f"    PyTorch Memory Usage: {torch.cuda.memory_reserved() / 1024 / 1024 / 1024:.3f}GB (Reserved), {torch.cuda.max_memory_allocated() / 1024 / 1024 / 1024:.3f}GB (Current)")
            profile.report()

        if update_id % 100 == 0:
            learning_state.save(update_idx, self.ckpt_dir / f"{update_id}.pth")


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


# arg_parser.add_argument('--enable_gpu_sim', type=str, required=True, help='Enable GPU simulation (e.g., "cpu" or "gpu")')
arg_parser.add_argument('--gpu_id', type=int, required=True, help='GPU ID')
arg_parser.add_argument('--fattree_K', type=int, required=True, help='Fattree K value')
arg_parser.add_argument('--cc_method', type=int, required=True, help='Congestion control method')



args = arg_parser.parse_args()

# print
# print(f"Number of environments: {args.num_env}")
# print(f"Enable GPU simulation: {args.enable_gpu_sim}")
print(f"gpu id id: {args.gpu_id}")
print(f"Fattree K: {args.fattree_K}")
print(f"CC Method: {args.cc_method}")




# Parse topology and initialize Topo struct
topology_file = "./fattree_4_16g_2gps_100Gbps_H100_no_scale_up"  # Replace with the actual topology file path
# topology_file = "./Spectrum-X_128g_8gps_100Gbps_H100_no_scale_up_test"  # Replace with the actual topology file path
aj_link, link_num, net_npu_num, sw_num, port_num, switch_next_hop_port, net_npu_next_hop_port, sw_port_num, net_npu_port_num, graph, port_mapping, _ = parse_topo(topology_file)

print("sw_num: ", sw_num)

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

# combined_next_hop_port的长度等于next_hop_port的真实长度
combined_next_hop_port = np.array(net_npu_next_hop_port[0:net_npu_port_num]+switch_next_hop_port[net_npu_port_num:net_npu_port_num+sw_port_num], dtype=np.int16)
topo.next_hop_port[:len(combined_next_hop_port)] = combined_next_hop_port
print("net_npu_port_num: ", net_npu_port_num, "sw_port_num: ", sw_port_num)
print("next_hop_port\n", len(topo.next_hop_port), topo.next_hop_port)
# Build global routing table and assign to Fib
_, global_routing_table_array, next_hop_num = build_global_routing_table_with_array(graph, port_mapping)

fib = madrona_escape_room.Fib()

# Adjust the shape of global_routing_table_array to match fib.fib
adjusted_routing_table = np.zeros((fib.fib.shape[0], fib.fib.shape[1], fib.fib.shape[2]), dtype=np.int16)
for i in range(min(len(global_routing_table_array), fib.fib.shape[0])):
    for j in range(min(len(global_routing_table_array[i]), fib.fib.shape[1])):
        adjusted_routing_table[i, j, :len(global_routing_table_array[i][j])] = global_routing_table_array[i][j]

fib.fib[:len(adjusted_routing_table)] = adjusted_routing_table

next_hop_num_np = np.array(next_hop_num, dtype=np.int16)
fib.next_hop_num[:next_hop_num_np.shape[0], :next_hop_num_np.shape[1]] = next_hop_num_np
print("fib.next_hop_num\n", len(fib.next_hop_num), fib.next_hop_num[1])

# print("fib.fib shape:", fib.fib.shape, "dtype:", fib.fib.dtype)
# print("fib.next_hop_num shape:", fib.next_hop_num.shape, "dtype:", fib.next_hop_num.dtype)


# print("**fib**\n")
# print(fib.fib[17])

links = np.zeros((2, 100), dtype=np.uint32)  # Create a 2D NumPy array for Links
# links[0][0] = 5  # Example initialization
# links[1][0] = 10  # Example initialization

sim = madrona_escape_room.SimManager(
    exec_mode = madrona_escape_room.madrona.ExecMode.CUDA if args.gpu_sim else madrona_escape_room.madrona.ExecMode.CPU,
    gpu_id = args.gpu_id,
    num_worlds = args.num_worlds,
    rand_seed = 5,
    auto_reset = True,
    k_aray = args.fattree_K,
    cc_method = args.cc_method,
    links = links,
    topo = topo,
    fib = fib
)

# ckpt_dir = Path(args.ckpt_dir)

# learning_cb = LearningCallback(ckpt_dir, args.profile_report)

# if torch.cuda.is_available():
#     dev = torch.device(f'cuda:{args.gpu_id}')
# else:
#     dev = torch.device('cpu')

# ckpt_dir.mkdir(exist_ok=True, parents=True)

# obs, num_obs_features = setup_obs(sim)
# policy = make_policy(num_obs_features, args.num_channels, args.separate_value)

# actions = sim.action_tensor().to_torch()
# dones = sim.done_tensor().to_torch()
# rewards = sim.reward_tensor().to_torch()

# # Flatten N, A, ... tensors to N * A, ...
# actions = actions.view(-1, *actions.shape[2:])
# dones  = dones.view(-1, *dones.shape[2:])
# rewards = rewards.view(-1, *rewards.shape[2:])

# if args.restore:
#     restore_ckpt = ckpt_dir / f"{args.restore}.pth"
# else:
#     restore_ckpt = None




FRAME_LEN = 1000
start = time.time()
for i in range(args.num_updates):
    # if i>=999:
    print("\n------------------------------------------------------------", i+1, "-th frame", (i+1)*FRAME_LEN, "--", (i+2)*FRAME_LEN, "------------------------------------------------------------\n")
    sim.step()
    end = time.time()
    print("time: ", (end - start))
    print("\n------------------------------------------------------------", i+1, "-th frame", (i+1)*FRAME_LEN, "--", (i+2)*FRAME_LEN, "time: ", (end - start), "------------------------------------------------------------\n")

# end = time.time()

# print("time: ", (end - start))
