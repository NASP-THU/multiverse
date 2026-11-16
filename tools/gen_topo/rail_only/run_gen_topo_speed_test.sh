#!/bin/bash

# Topology Generator Examples
# ============================

# Output directory
OUTPUT_DIR="./output/accuracy_test"
mkdir -p $OUTPUT_DIR



# Hybrid topology: 16 hosts, 8 GPUs/host, 1 NVSwitch/host, 8 rail switches
# Node IDs: GPU 0-127, NVSwitch 128-143, Rail switches 144-151
# Each GPU i connects to rail switch (i % 8)

# nvl_bandwidth=400
# net_bandwidth=100

# for hosts in 32; do
#     for gpus_per_host in 4; do
#         echo "Generating topology for ${hosts} hosts, ${gpus_per_host} GPUs per host, ${switch_num} switches"
#         num_gpus=$((hosts * gpus_per_host))
#         switch_num=${gpus_per_host}
#         python gen_ns3_roft.py --type hybrid_no_nvs --hosts ${hosts} --gpus-per-host ${gpus_per_host} --switches ${switch_num} --chain -o $OUTPUT_DIR/ns3_rail_${num_gpus}gpus-switch_${switch_num}-net_bw${net_bandwidth}_with_no_nvs_chain.txt
#         python gen_multiverse_roft.py --type hybrid_no_nvs --hosts ${hosts} --gpus-per-host ${gpus_per_host} --switches ${switch_num} --chain -o $OUTPUT_DIR/multiverse_rail_${num_gpus}gpus-switch_${switch_num}-net_bw${net_bandwidth}_with_no_nvs_chain.txt
#     done
# done



#no chain
#no nvs
nvl_bandwidth=400
net_bandwidth=100



#hybrid w/哦chian
for hosts in 16; do
    for gpus_per_host in 8; do
#         echo "Generating topology for ${hosts} hosts, ${gpus_per_host} GPUs per host, ${switch_num} switches"
        num_gpus=$((hosts * gpus_per_host))
        switch_num=${gpus_per_host}

#         python gen_ns3_roft.py --type hybrid --hosts ${hosts} --gpus-per-host ${gpus_per_host} --switches ${switch_num} --chain -o $OUTPUT_DIR/ns3_rail_${num_gpus}gpus-nvl_bw${nvl_bandwidth}-switch_${switch_num}-net_bw${net_bandwidth}_chain.txt
        # no chain
        
        python gen_multiverse_roft.py --type hybrid --hosts ${hosts} --gpus-per-host ${gpus_per_host} --switches ${switch_num} -o $OUTPUT_DIR/multiverse_rail_${num_gpus}gpus-nvl_bw${nvl_bandwidth}-switch_${switch_num}-net_bw${net_bandwidth}.txt
    done
done







# for hosts in 32; do
#     for gpus_per_host in 4; do
#         echo "Generating topology for ${hosts} hosts, ${gpus_per_host} GPUs per host, ${switch_num} switches"
#         num_gpus=$((hosts * gpus_per_host))
#         switch_num=${gpus_per_host}
#         python gen_ns3_roft.py --type hybrid_no_nvs --hosts ${hosts} --gpus-per-host ${gpus_per_host} --switches ${switch_num} --chain -o $OUTPUT_DIR/ns3_rail_${num_gpus}gpus-switch_${switch_num}-net_bw${net_bandwidth}_chain.txt
#         python gen_multiverse_roft.py --type hybrid_no_nvs --hosts ${hosts} --gpus-per-host ${gpus_per_host} --switches ${switch_num} --chain -o $OUTPUT_DIR/multiverse_rail_${num_gpus}gpus-switch_${switch_num}-net_bw${net_bandwidth}_chain.txt
#     done
# done




# python gen_multiverse_roft.py --type hybrid_no_nvs --hosts 256 --gpus-per-host 8 --switches 8 --chain -o $OUTPUT_DIR/multiverse_rail_2048gpus-net_bw${net_bandwidth}_with_no_nvs_chain.txt



# python gen_ns3_roft.py --type hybrid_no_nvs --hosts 128 --gpus-per-host 8 --switches 8 --chain -o $OUTPUT_DIR/ns3_rail_1024gpus-net_bw100_with_no_nvs_chain.txt


# for k in 8 16 20 24; do
#     num_gpus=$((k * k * k / 4))
#     echo "Generating topology for ${num_gpus} GPUs, fattree k=${k}"
#     python gen_ns3_roft.py --type fattree --fattree-k $k -o $OUTPUT_DIR/ns3_fattree_k$k.txt
# done



# for k in 16 20 24 32 36; do
#     num_gpus=$((k * k * k / 4))
#     echo "Generating topology for ${num_gpus} GPUs, fattree k=${k}"
#     python gen_multiverse_roft.py --type fattree --fattree-k $k -o $OUTPUT_DIR/multiverse_fattree_k$k.txt
# done



# ========================================
# Rail + NVLink topology (convenience wrapper)
# ========================================

# 128 hosts, 8 GPUs/host, 1 NVSwitch/host, 8 rail switches with chain
# Total: 1024 GPUs, 128 NVSwitches, 8 rail switches
# python gen_roft.py --type rail_nvlink \
#     --hosts 128 \
#     --gpus-per-host 8 \
#     --nvswitches 1 \
#     --switches 8 \
#     --chain \
#     --bandwidth 100Gbps \
#     --latency 1000 \
#     --gpu-type H100 \
#     -o $OUTPUT_DIR/topo_rail_nvlink_128h_8g.txt





# ========================================
# Large scale examples
# ========================================

# 256 hosts, 8 GPUs/host = 2048 GPUs total
# python gen_roft.py --type rail_nvlink \
#     --hosts 256 \
#     --gpus-per-host 8 \
#     --nvswitches 1 \
#     --switches 8 \
#     --chain \
#     -o $OUTPUT_DIR/topo_rail_nvlink_2048g.txt

# ========================================
# Without NVSwitch (direct GPU-to-GPU NVLink mesh within host)
# ========================================

# python gen_roft.py --type hybrid \
#     --hosts 16 \
#     --gpus-per-host 8 \
#     --nvswitches 0 \
#     --switches 8 \
#     --chain \
#     -o $OUTPUT_DIR/topo_hybrid_no_nvswitch.txt

echo "Topology generation complete!"
echo "Output files are in: $OUTPUT_DIR"
