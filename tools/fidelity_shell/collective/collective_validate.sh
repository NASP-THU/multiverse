#!/bin/bash

# Collective Communication Validation Script
# Compare ASTRA-SIM and Multiverse collective communication logs

BASE_DIR=$(cd $(dirname $0); pwd)


#CASE_NUM=20
CASE_NUM=21
#CASE_NUM=22
CASE_NUM=23
CASE_NUM=24
CASE_NUM=25
#CASE_NUM=26
#CASE_NUM=27
CASE_NUM=28
CASE_NUM=29


if [ -z "$CASE_NUM" ]
then
    # Define input log files
    ASTRA_SIM_LOG=$BASE_DIR/test_astrasim_comm_8_8_16_dim1_type2.log
    MULTIVERSE_LOG=$BASE_DIR/test_multiverse_comm_8_8_16_dim1_type2.log

    # Define output CSV files
    ASTRA_SIM_CSV=$BASE_DIR/astrasim_collective.csv
    MULTIVERSE_CSV=$BASE_DIR/multiverse_collective.csv
else
    ASTRA_SIM_LOG=/app/astra-sim/fidelity/report/${CASE_NUM}/ASTRA_SIM/console.log
    MULTIVERSE_LOG=/app/astra-sim/fidelity/report/${CASE_NUM}/Multiverse/console.log
    # Define output files
    ASTRA_SIM_CSV=$BASE_DIR/astrasim_collective_${CASE_NUM}.csv
    MULTIVERSE_CSV=$BASE_DIR/multiverse_collective_${CASE_NUM}.csv
fi




# Convert comm_type number to enum name
get_comm_type_name() {
    local type_num=$1
    case $type_num in
        0) echo "ALL_REDUCE" ;;
        1) echo "REDUCE" ;;
        2) echo "ALL_GATHER" ;;
        3) echo "GATHER" ;;
        4) echo "SCATTER" ;;
        5) echo "BROADCAST" ;;
        6) echo "ALL_TO_ALL" ;;
        7) echo "REDUCE_SCATTER" ;;
        8) echo "REDUCE_SCATTER_BLOCK" ;;
        9) echo "BARRIER" ;;
        *) echo "UNKNOWN_$type_num" ;;
    esac
}

# Process ASTRA-SIM log file
# Rules:
# 1. Filter: grep '\[TEST_RING_COLLECTIVE\]' | grep -v 'Flow #'
# 2. Start anchor: [TEST_RING_COLLECTIVE] node->id
# 3. End anchor: [TEST_RING_COLLECTIVE] comm end!
# 4. Only output complete records (with comm end!)
process_astrasim_log() {
    local LOG_FILE=$1
    local OUTPUT_CSV=$2
    
    # Filter valid lines
    local FILTERED_LINES=$(grep '\[TEST_RING_COLLECTIVE\]' "$LOG_FILE" | grep -v 'Flow #')
    
    # Write CSV header for ASTRA-SIM
    echo "npu_id,node_id,comm_type,comm_size,involved_dims,flow.index,flow.src,flow.dst,flow.msg_size" > "$OUTPUT_CSV"
    
    local npu_id=26  # Fixed npu_id for current test
    local node_id=""
    local comm_type=""
    local comm_size=""
    local involved_dims=""
    local in_record=0
    local flow_index=0
    declare -a flow_buffer=()  # Buffer to store flow data until comm end!
    
    while IFS= read -r line; do
        # Check for record end: comm end! (must check BEFORE node->id= since comm end! line also contains node->id=)
        if echo "$line" | grep -q 'comm end!' && [[ $in_record -eq 1 ]]; then
            # Write all buffered flows to CSV (complete record)
            for flow_line in "${flow_buffer[@]}"; do
                echo "$flow_line" >> "$OUTPUT_CSV"
            done
            flow_buffer=()
            in_record=0
        # Check for record start: node->id= (but NOT comm end! line)
        elif echo "$line" | grep -q 'node->id=' && ! echo "$line" | grep -q 'comm end!'; then
            # Clear previous buffer (previous record was incomplete, discard it)
            flow_buffer=()
            flow_index=0
            
            in_record=1
            
            # Extract fields from start line
            # Format: [TEST_RING_COLLECTIVE] node->id=0, node->name=AG_0_0, comm_type=2, comm_size=28316160, involved_dim(0)=0, involved_dim(1)=1, involved_dim(2)=0
            node_id=$(echo "$line" | grep -oP 'node->id=\K\d+')
            local comm_type_num=$(echo "$line" | grep -oP 'comm_type=\K\d+')
            comm_type=$(get_comm_type_name "$comm_type_num")
            comm_size=$(echo "$line" | grep -oP 'comm_size=\K\d+')
            local dim0=$(echo "$line" | grep -oP 'involved_dim\(0\)=\K\d+')
            local dim1=$(echo "$line" | grep -oP 'involved_dim\(1\)=\K\d+')
            local dim2=$(echo "$line" | grep -oP 'involved_dim\(2\)=\K\d+')
            involved_dims="${dim0}_${dim1}_${dim2}"
            
        # Check for flow line: send_flow
        elif echo "$line" | grep -q 'send_flow:' && [[ $in_record -eq 1 ]]; then
            # Extract flow info
            # Format: [TEST_RING_COLLECTIVE] send_flow: src_id = 26, dst_id = 34, msg_size = 28316160, tag = 0
            local src_id=$(echo "$line" | grep -oP 'src_id = \K\d+')
            local dst_id=$(echo "$line" | grep -oP 'dst_id = \K\d+')
            local msg_size=$(echo "$line" | grep -oP 'msg_size = \K\d+')
            
            # Buffer flow data (only write to CSV when comm end! is encountered)
            flow_buffer+=("${npu_id},${node_id},${comm_type},${comm_size},${involved_dims},${flow_index},${src_id},${dst_id},${msg_size}")
            ((flow_index++))
        fi
    done <<< "$FILTERED_LINES"
    
    echo "ASTRA-SIM CSV saved to: $OUTPUT_CSV"
}

# Process Multiverse log file
# Rules:
# 1. Filter: grep '\[TEST_RING_COLLECTIVE\]'
# 2. Start anchor: npu_id[
# 3. End anchor: flow end.
process_multiverse_log() {
    local LOG_FILE=$1
    local OUTPUT_CSV=$2
    
    # Filter valid lines
    local FILTERED_LINES=$(grep '\[TEST_RING_COLLECTIVE\]' "$LOG_FILE")
    
    # Write CSV header for Multiverse
    echo "npu_id,node_id,comm_type,comm_size,involved_dims,flow.index,flow.src,flow.dst,flow.msg_size" > "$OUTPUT_CSV"
    
    local npu_id=26  # Fixed npu_id for current test
    local in_record=0
    local node_id=""
    local comm_size=""
    local comm_type=""
    local involved_dims=""
    
    while IFS= read -r line; do
        # Check for record start: npu_id[
        if echo "$line" | grep -q 'npu_id\['; then
            in_record=1
            # Extract fields from header line
            # Format: [TEST_RING_COLLECTIVE]: npu_id[26], node_id[0],comm_size: 28316160, comm_type: 2, dims: [8, 8, 16], involved_dim_1: 0, involved_dim_2: 1, involved_dim_3: 0
            node_id=$(echo "$line" | grep -oP 'node_id\[\K\d+')
            comm_size=$(echo "$line" | grep -oP 'comm_size: \K\d+')
            local comm_type_num=$(echo "$line" | grep -oP 'comm_type: \K\d+')
            comm_type=$(get_comm_type_name "$comm_type_num")
            local dim1=$(echo "$line" | grep -oP 'involved_dim_1: \K\d+')
            local dim2=$(echo "$line" | grep -oP 'involved_dim_2: \K\d+')
            local dim3=$(echo "$line" | grep -oP 'involved_dim_3: \K\d+')
            involved_dims="${dim1}_${dim2}_${dim3}"
            
        # Check for flow info line
        elif echo "$line" | grep -q 'flow info:' && [[ $in_record -eq 1 ]]; then
            # Extract flow info
            # Format: [TEST_RING_COLLECTIVE] flow info: flow_id=260001, comm_src=26, comm_dst=34, msg_size=28316160, exec_index=0
            local comm_src=$(echo "$line" | grep -oP 'comm_src=\K\d+')
            local comm_dst=$(echo "$line" | grep -oP 'comm_dst=\K\d+')
            local msg_size=$(echo "$line" | grep -oP 'msg_size=\K\d+')
            local exec_index=$(echo "$line" | grep -oP 'exec_index=\K\d+')
            
            # Write CSV line
            echo "${npu_id},${node_id},${comm_type},${comm_size},${involved_dims},${exec_index},${comm_src},${comm_dst},${msg_size}" >> "$OUTPUT_CSV"
            
        # Check for record end: flow end.
        elif echo "$line" | grep -q 'flow end\.' && [[ $in_record -eq 1 ]]; then
            in_record=0
        fi
    done <<< "$FILTERED_LINES"
    
    echo "Multiverse CSV saved to: $OUTPUT_CSV"
}

# Main execution
echo "=============================================="
echo "Collective Communication Validation"
echo "=============================================="
echo ""

echo "Processing ASTRA-SIM log..."
process_astrasim_log "$ASTRA_SIM_LOG" "$ASTRA_SIM_CSV"

echo ""
echo "Processing Multiverse log..."
process_multiverse_log "$MULTIVERSE_LOG" "$MULTIVERSE_CSV"

echo ""
echo "=============================================="
echo "Generated CSV files:"
echo "  - $ASTRA_SIM_CSV"
echo "  - $MULTIVERSE_CSV"
echo "=============================================="
echo ""

# Compare CSV files
echo "Comparing CSV files..."
if diff -q "$ASTRA_SIM_CSV" "$MULTIVERSE_CSV" > /dev/null 2>&1; then
    echo "PASSED: CSV files are identical"
else
    echo "FAILED: CSV files differ"
    echo ""
    echo "Differences:"
    diff "$ASTRA_SIM_CSV" "$MULTIVERSE_CSV"
    exit 1
fi
