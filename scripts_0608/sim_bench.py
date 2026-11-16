import torch
import madrona_escape_room
import argparse
import time

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument('--num-worlds', type=int, required=True)
arg_parser.add_argument('--num-steps', type=int, required=True)
arg_parser.add_argument('--profile-renderer', action='store_true')
arg_parser.add_argument('--gpu-id', type=int, default=0)

args = arg_parser.parse_args()

sim = madrona_escape_room.SimManager(
    exec_mode = madrona_escape_room.madrona.ExecMode.CUDA,
    gpu_id = args.gpu_id,
    num_worlds = args.num_worlds,
    auto_reset = True,
    rand_seed = 5,
    enable_batch_renderer = args.profile_renderer,
)

actions = sim.action_tensor().to_torch()

start = time.time()
for i in range(args.num_steps):
    actions[..., 0] = torch.randint_like(actions[..., 0], 0, 4)
    actions[..., 1] = torch.randint_like(actions[..., 1], 0, 8)
    actions[..., 2] = torch.randint_like(actions[..., 2], 0, 5)
    actions[..., 3] = torch.randint_like(actions[..., 3], 0, 2)

    sim.step()

end = time.time()

print("FPS", args.num_steps * args.num_worlds / (end - start))
