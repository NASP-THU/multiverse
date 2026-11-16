#!/bin/bash
set -e
# ============================================================
# 配置参数：直接在这里修改参数值
# ============================================================
DIM0=4
DIM1=2
DIM2=16
NPU_NUM=128
DEV_MODE=2 # 0 = SYS, 1 = NET, 2 = MIX

num_env=1
gpu_id=0
fattree_K=4
cc_method=1 #0 = dcqcn, 1 = hpcc(TBD)
topology_file="./input/network/multiverse_rail_128gpus-switch_4-net_bw100_chain.txt"  # Default topology file
workload_path="./workload/128gpus-tp4-pp2-dp16-vpp2-gbs64-mbs1-hid_size12288-seq_len2048-json-comm_scale100-comp_scale100"
num_updates=999999 # max step number 


# ============================================================

echo "=== 配置参数 ==="
echo "dim0: $DIM0"
echo "dim1: $DIM1"
echo "dim2: $DIM2"
echo "npu_num: $NPU_NUM"
echo "================"

# ============================================================
# 替换文件中的值
# ============================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/scripts/train_with_topo_fib_transfer.py"
CPP_MGR="$SCRIPT_DIR/src/mgr.cpp"
HPP_TYPES="$SCRIPT_DIR/src/types/llm_sys_types.hpp"
GLOBAL_TYPES="$SCRIPT_DIR/src/types/global_types.hpp"

# 备份原始文件（可选，如果需要恢复）
# cp "$PYTHON_SCRIPT" "${PYTHON_SCRIPT}.bak"
# cp "$CPP_MGR" "${CPP_MGR}.bak"
# cp "$HPP_TYPES" "${HPP_TYPES}.bak"

# 替换 scripts/train_with_topo_fib_transfer.py 中的 ring_dim0, ring_dim1, ring_dim2
if [ -f "$PYTHON_SCRIPT" ]; then
    # 替换 ring_dim0（匹配 "    ring_dim0 = 数字" 格式）
    sed -i "s/^\([[:space:]]*ring_dim0[[:space:]]*=[[:space:]]*\)[0-9]*/\1$DIM0/" "$PYTHON_SCRIPT"
    # 替换 ring_dim1
    sed -i "s/^\([[:space:]]*ring_dim1[[:space:]]*=[[:space:]]*\)[0-9]*/\1$DIM1/" "$PYTHON_SCRIPT"
    # 替换 ring_dim2
    sed -i "s/^\([[:space:]]*ring_dim2[[:space:]]*=[[:space:]]*\)[0-9]*/\1$DIM2/" "$PYTHON_SCRIPT"
    # 替换 npu_num（匹配 "    npu_num=数字" 格式，保留后面的注释）
    sed -i "s/^\([[:space:]]*npu_num[[:space:]]*=[[:space:]]*\)[0-9]*/\1$NPU_NUM/" "$PYTHON_SCRIPT"
    # sed -i "s/^\([[:space:]]*npu_num=\)[0-9]*\(.*\)/\1$NPU_NUM\2/" "$PYTHON_SCRIPT"
    echo "✓ 已更新 $PYTHON_SCRIPT"
else
    echo "警告: 未找到 $PYTHON_SCRIPT"
fi

# 替换 src/mgr.cpp 中的第二个参数 128
# if [ -f "$CPP_MGR" ]; then
#     # 匹配 {impl_->cfg.numWorlds, 128, 400000} 中的 128（保留前面的空格和格式）
#     sed -i "s/\(impl_->cfg\.numWorlds,[[:space:]]*\)[0-9]*\(,[[:space:]]*40000\)/\1$NPU_NUM\2/" "$CPP_MGR"
#     echo "✓ 已更新 $CPP_MGR"
# else
#     echo "警告: 未找到 $CPP_MGR"
# fi

# 替换 src/types/llm_sys_types.hpp 中的 NPU_NUM
if [ -f "$HPP_TYPES" ]; then
    sed -i "s/^#define[[:space:]]*NPU_NUM[[:space:]]*[0-9]*/#define NPU_NUM $NPU_NUM/" "$HPP_TYPES"
    echo "✓ 已更新 $HPP_TYPES"
else
    echo "警告: 未找到 $HPP_TYPES"
fi

# 替换 src/types/global_types.hpp 中的 DEV_MODE
if [ -f "$GLOBAL_TYPES" ]; then
    sed -i "s/^#define[[:space:]]*DEV_MODE[[:space:]]*[0-9]*/#define DEV_MODE $DEV_MODE/" "$GLOBAL_TYPES"
    echo "✓ 已更新 $GLOBAL_TYPES"
else
    echo "警告: 未找到 $GLOBAL_TYPES"
fi

echo ""

# GPU Memory and Debug Environment Variables
# export MADRONA_MWGPU_DEVICE_HEAP_SIZE=17179869184  # 16GB GPU heap
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

# export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/.cache/mw_megakernel_cache.bin"
# export MADRONA_BVH_KERNEL_CACHE="$(pwd)/.cache/mw_bvh_cache.bin"

# optional: debug compile output
export MADRONA_MWGPU_VERBOSE_COMPILE=1

# optional: fast compile during development (sacrifice runtime performance)
# export MADRONA_MWGPU_FORCE_DEBUG=1

# avoid redundant compilation
unset MADRONA_MWGPU_ENABLE_PGO


## synchronous mode, runtime
# export CUDA_LAUNCH_BLOCKING=1

export MADRONA_MWGPU_VERBOSE_HOSTALLOC=1

## runtime
# export MADRONA_MWGPU_INIT_THREADS=32



# # show link log
# export MADRONA_MWGPU_LINK_VERBOSE=1
# # detailed compile output
# export MADRONA_MWGPU_VERBOSE_COMPILE=1

cd ./build
cmake ..
# add debug flag
#cmake -DCMAKE_BUILD_TYPE=Debug \
#      -DCUDA_VERBOSE=ON \
#      -DMADRONA_ENABLE_TRACING=ON \
#      ..

make -j 60
cd ../

pip install -e .

cd ./scripts/


###########################################################
# debug with gdb/cuda-gdb

# gpu
# cuda-gdb --args python train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model 2 --gpu-sim 

# cpu
# gdb --args python3 train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model 2 

# ============================================================
# CPU调试选项（取消注释其中一个）
# ============================================================

# 方法1: 使用gdb调试CPU版本（需要安装python-dbg或python3-dbg）
# gdb --args python3 train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model 2

# 方法2: 使用Python内置调试器pdb（在代码中设置断点：import pdb; pdb.set_trace()）
# python3 -m pdb train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model 2

# 方法3: 使用ipdb（更友好的交互式调试器，需要安装：pip install ipdb）
# python3 -m ipdb train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model 2

# 方法4: 普通运行（无调试）
# python train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file
# cuda-gdb --args 
python3 train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model $DEV_MODE