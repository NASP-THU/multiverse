#!/bin/bash

# GPU Memory and Debug Environment Variables
# 11export MADRONA_MWGPU_DEVICE_HEAP_SIZE=17179869184  # 16GB GPU heap
# export MADRONA_MWGPU_FORCE_DEBUG=0                 # Disable debug mode for better performance  
# export CUDA_VISIBLE_DEVICES=0                      # Use only GPU 0

# Debug options (uncomment if needed)
# export MADRONA_MWGPU_VERBOSE_COMPILE=1
# export CUDA_LAUNCH_BLOCKING=1

# export MADRONA_MWGPU_DEVICE_HEAP_SIZE=4679869184

# echo "=== GPU Memory Settings ==="
# echo "MADRONA_MWGPU_DEVICE_HEAP_SIZE: $MADRONA_MWGPU_DEVICE_HEAP_SIZE bytes"
# echo "CUDA_VISIBLE_DEVICES: $CUDA_VISIBLE_DEVICES"
# echo "=========================="

# Cache file paths (recommended to keep stable)
# export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/build/mw_megakernel_cache.bin"
# export MADRONA_BVH_KERNEL_CACHE="$(pwd)/build/mw_bvh_cache.bin"

export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/.cache/mw_megakernel_cache.bin"
export MADRONA_BVH_KERNEL_CACHE="$(pwd)/.cache/mw_bvh_cache.bin"

# Optional: debug compile output
export MADRONA_MWGPU_VERBOSE_COMPILE=1

# Optional: fast development compilation (sacrifices runtime performance)
export MADRONA_MWGPU_FORCE_DEBUG=0

# Avoid repeated compilation
unset MADRONA_MWGPU_ENABLE_PGO

# Synchronous mode, runtime
# export CUDA_LAUNCH_BLOCKING=1
# Runtime
# 11export MADRONA_MWGPU_INIT_THREADS=32

 compute-sanitizer --tool=memcheck  python scripts/train_with_topo_fib_transfer.py --num-worlds 1 --num-updates 1 --ckpt-dir build/ckpts --gpu_id 0 --fattree_K 4 --cc_method 1 --topology-file scripts/input/network/fattree_8_128g_4gps_100Gbps_H100_no_scale_up --gpu-sim