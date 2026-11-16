#!/bin/bash

# GPU Memory and Debug Environment Variables
# export MADRONA_MWGPU_DEVICE_HEAP_SIZE=17179869184  # 16GB GPU heap
# export MADRONA_MWGPU_FORCE_DEBUG=0                 # Disable debug mode for better performance  
# export CUDA_VISIBLE_DEVICES=0                      # Use only GPU 0

# Debug options (uncomment if needed)
# export MADRONA_MWGPU_VERBOSE_COMPILE=1
# export CUDA_LAUNCH_BLOCKING=1

# # Show link logs
# export MADRONA_MWGPU_LINK_VERBOSE=1
# # Detailed compile output
# export MADRONA_MWGPU_VERBOSE_COMPILE=1

# export MADRONA_MWGPU_DEVICE_HEAP_SIZE=4679869184

# echo "=== GPU Memory Settings ==="
# echo "MADRONA_MWGPU_DEVICE_HEAP_SIZE: $MADRONA_MWGPU_DEVICE_HEAP_SIZE bytes"
# echo "CUDA_VISIBLE_DEVICES: $CUDA_VISIBLE_DEVICES"
# echo "=========================="

export MADRONA_MWGPU_FORCE_DEBUG=1


num_env=1
num_updates=40000
gpu_id=2
fattree_K=4
cc_method=1 #0 = dcqcn, 1 = hpcc(TBD)
#topology_file="fattree_24_3456g_12gps_100Gbps_H100_no_scale_up"  # Default topology file
topology_file="./input/network/fattree_8_128g_4gps_100Gbps_H100_no_scale_up"  # Default topology file
# topology_file="./input/network/fattree_4_16g_2gps_100Gbps_H100_no_scale_up"
# # show link log
# export MADRONA_MWGPU_LINK_VERBOSE=1
# # detailed compile output
# export MADRONA_MWGPU_VERBOSE_COMPILE=1

cd ./build
#cmake ..
 cmake -DCMAKE_BUILD_TYPE=Debug \
       -DCUDA_VERBOSE=ON \
       -DMADRONA_ENABLE_TRACING=ON \
       ..

make -j 60
cd ../
cd ./scripts/

#gdb --args python train_with_topo_fib_transfer.py --num-worlds $num_env --num-updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file 
gdb --args python3.9-dbg train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file 
