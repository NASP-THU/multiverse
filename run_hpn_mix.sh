#!/bin/bash
# optional: debug compile output
export MADRONA_MWGPU_VERBOSE_COMPILE=1

# avoid redundant compilation
unset MADRONA_MWGPU_ENABLE_PGO

num_env=1
gpu_id=1
fattree_K=4
cc_method=1 #0 = dcqcn, 1 = hpcc(TBD)







############################################ case 4.1 ###################################################

DIM0=8
DIM1=2
DIM2=8
NPU_NUM=128
# ============================================================
DEV_MODE=2

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


# 替换 src/types/llm_sys_types.hpp 中的 NPU_NUM
if [ -f "$HPP_TYPES" ]; then
    sed -i "s/^#define[[:space:]]*NPU_NUM[[:space:]]*[0-9]*/#define NPU_NUM $NPU_NUM/" "$HPP_TYPES"
    echo "✓ 已更新 $HPP_TYPES"
else
    echo "警告: 未找到 $HPP_TYPES"
fi

echo ""


# 替换 src/types/global_types.hpp 中的 DEV_MODE
if [ -f "$GLOBAL_TYPES" ]; then
    sed -i "s/^#define[[:space:]]*DEV_MODE[[:space:]]*[0-9]*/#define DEV_MODE $DEV_MODE/" "$GLOBAL_TYPES"
    echo "✓ 已更新 $GLOBAL_TYPES"
else
    echo "警告: 未找到 $GLOBAL_TYPES"
fi

echo ""



# topology_file="./input/network/fattree_24_3456g_12gps_100Gbps_H100_no_scale_up"  # Default topology file
#topology_file="./input/network/fattree_16_1024g_8gps_100Gbps_H100_no_scale_up"
topo="AlibabaHPN_128g_8gps_DualToR_DualPlane_100Gbps_H100"
topology_file="./input/network/${topo}"
# workload_path="./workload/1024gpus-tp8-pp2-dp64-vpp2-gbs256-mbs1-hid_size12288-seq_len2048-json-comm_scale100-comp_scale100"
workload_path="./json/128gpus-tp8-pp2-dp8-vpp2-gbs32-mbs1-hid_size12288-seq_len2048-json-comm_scale1000-comp_scale1000"


cd ./build
cmake ..
make -j 60
cd ../
pip install -e .
cd ./scripts/

num_updates=999999 # max step number
###########################################################
python3 train_with_topo_fib_transfer.py --num-worlds $num_env --num_updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --topology-file $topology_file --workload-folder $workload_path --run_model $DEV_MODE --gpu-sim #> log_case4.1-hpn_8_2_8_128_debug.log #2>&1 &


