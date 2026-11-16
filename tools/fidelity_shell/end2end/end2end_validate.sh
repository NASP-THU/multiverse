#!/bin/bash

# End-to-End Chakra Node Validation Script
# Compare ASTRA-SIM and Multiverse logs for ALL Chakra nodes with Flow details
#
# ASTRA-SIM: Parse fct-100g-1000ns.txt (Flow Completion Time log)
# Multiverse: Parse multiverse.1229_v5.log

BASE_DIR=$(cd $(dirname $0); pwd)

# Define input log files
ASTRA_SIM_FCT=$BASE_DIR/fct-100g-1000ns.txt
MULTIVERSE_LOG=$BASE_DIR/multiverse.1229_v5.log

# Define output CSV files
ASTRA_SIM_CSV=$BASE_DIR/astrasim_end2end.csv
MULTIVERSE_CSV=$BASE_DIR/multiverse_end2end.csv
DIFF_CSV=$BASE_DIR/end2end_diff.csv
SAME_CSV=$BASE_DIR/end2end_same.csv

# Target NPU ID to filter (to match Multiverse output which logs npu_id=0)
TARGET_NPU_ID=0

# Python scripts
PARSE_FCT_SCRIPT=$BASE_DIR/parse_astrasim_fct.py
COMPARE_SCRIPT=$BASE_DIR/compare_csv.py

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

# Process Multiverse log file
# Rules:
# 1. Filter: grep '\[TEST_ALL_COLLECTIVE\]'
# 2. Start anchor: [TEST_ALL_COLLECTIVE]: npu_id[X], node_id[Y]
# 3. Flow data: [TEST_ALL_COLLECTIVE] flow info:
# 4. End anchor: next [TEST_ALL_COLLECTIVE]: npu_id[X], node_id[Y]
process_multiverse_log() {
    local LOG_FILE=$1
    local OUTPUT_CSV=$2
    
    # Filter valid lines (include both [TEST_ALL_COLLECTIVE] and [FLOW] lines)
    local FILTERED_LINES=$(grep -E '\[TEST_ALL_COLLECTIVE\]|\[FLOW\]' "$LOG_FILE")
    
    # Write CSV header (simplified format without comm_type and comm_size)
    echo "npu_id,node_id,flow.index,flow.src,flow.dst,flow.msg_size" > "$OUTPUT_CSV"
    
    local npu_id=""
    local node_id=""
    local in_record=0
    local flow_index=0
    declare -a flow_buffer=()
    
    while IFS= read -r line; do
        # Check for node header line: 
        # Format A: [TEST_ALL_COLLECTIVE]: npu_id[X], node_id[Y]
        # Format B: [TEST_ALL_COLLECTIVE]: NPU[X],chakraNode[Y]
        if echo "$line" | grep -q '\[TEST_ALL_COLLECTIVE\]:' && (echo "$line" | grep -q 'npu_id\[' || echo "$line" | grep -q 'NPU\['); then
            # If we have a previous record, output its buffered flows
            if [[ $in_record -eq 1 ]]; then
                for flow_line in "${flow_buffer[@]}"; do
                    echo "$flow_line" >> "$OUTPUT_CSV"
                done
            fi
            
            # Start new record
            flow_buffer=()
            flow_index=0
            in_record=1
            
            # Extract node information (support both formats)
            # Format A: [TEST_ALL_COLLECTIVE]: npu_id[3], node_id[0],comm_size: 1101041664, comm_type: 2
            # Format B: [TEST_ALL_COLLECTIVE]: NPU[0],chakraNode[6],comm_size: 2097152, comm_type: 0
            npu_id=$(echo "$line" | grep -oP '(npu_id|NPU)\[\K\d+')
            node_id=$(echo "$line" | grep -oP '(node_id|chakraNode)\[\K\d+')
            
        # Check for flow info line
        elif echo "$line" | grep -q 'flow info:' && [[ $in_record -eq 1 ]]; then
            # Extract flow info
            # Format 1: [TEST_ALL_COLLECTIVE] flow info: flow_id=30001, comm_src=3, comm_dst=7, msg_size=1101041664, exec_index=0
            # Format 2: [TEST_ALL_COLLECTIVE] flow info: comm_src=3, comm_dst=11, comm_size=25165824
            local flow_src=$(echo "$line" | grep -oP 'comm_src=\K\d+')
            local flow_dst=$(echo "$line" | grep -oP 'comm_dst=\K\d+')
            
            # Try msg_size first, then comm_size
            local flow_msg_size=$(echo "$line" | grep -oP 'msg_size=\K\d+')
            if [ -z "$flow_msg_size" ]; then
                flow_msg_size=$(echo "$line" | grep -oP 'comm_size=\K\d+')
            fi
            
            # Get exec_index if available, otherwise use our counter
            local exec_index=$(echo "$line" | grep -oP 'exec_index=\K\d+')
            if [ -z "$exec_index" ]; then
                exec_index=$flow_index
            fi
            
            # Buffer flow data (simplified format)
            if [ -n "$flow_src" ] && [ -n "$flow_dst" ] && [ -n "$flow_msg_size" ]; then
                flow_buffer+=("${npu_id},${node_id},${exec_index},${flow_src},${flow_dst},${flow_msg_size}")
                ((flow_index++))
            fi
        fi
    done <<< "$FILTERED_LINES"
    
    # Output the last record's buffered flows
    if [[ $in_record -eq 1 ]]; then
        for flow_line in "${flow_buffer[@]}"; do
            echo "$flow_line" >> "$OUTPUT_CSV"
        done
    fi
    
    echo "Multiverse CSV saved to: $OUTPUT_CSV"
}

# Main execution
echo "=============================================="
echo "End-to-End Chakra Node Validation"
echo "=============================================="
echo ""

# Check input files exist
if [ ! -f "$ASTRA_SIM_FCT" ]; then
    echo "ERROR: ASTRA-SIM fct.txt not found: $ASTRA_SIM_FCT"
    exit 1
fi

if [ ! -f "$MULTIVERSE_LOG" ]; then
    echo "ERROR: Multiverse log not found: $MULTIVERSE_LOG"
    exit 1
fi

# Check Python scripts exist
if [ ! -f "$PARSE_FCT_SCRIPT" ]; then
    echo "ERROR: Python script not found: $PARSE_FCT_SCRIPT"
    exit 1
fi

if [ ! -f "$COMPARE_SCRIPT" ]; then
    echo "ERROR: Python script not found: $COMPARE_SCRIPT"
    exit 1
fi

# Process ASTRA-SIM fct.txt using Python script
# Default: include all flows (collective + compute) for target NPU
# Add --collective-only flag to only include collective flows
echo "Processing logs..."
python3 "$PARSE_FCT_SCRIPT" "$ASTRA_SIM_FCT" "$ASTRA_SIM_CSV" --target-npu "$TARGET_NPU_ID" > /dev/null
process_multiverse_log "$MULTIVERSE_LOG" "$MULTIVERSE_CSV" > /dev/null

# Check if CSV files exist and have content
if [ ! -f "$ASTRA_SIM_CSV" ]; then
    echo "FAILED: ASTRA-SIM CSV file not found: $ASTRA_SIM_CSV"
    exit 1
fi

if [ ! -f "$MULTIVERSE_CSV" ]; then
    echo "FAILED: Multiverse CSV file not found: $MULTIVERSE_CSV"
    exit 1
fi

ASTRA_LINE_COUNT=$(wc -l < "$ASTRA_SIM_CSV")
MULTIVERSE_LINE_COUNT=$(wc -l < "$MULTIVERSE_CSV")

if [ "$ASTRA_LINE_COUNT" -le 1 ]; then
    echo "FAILED: ASTRA-SIM CSV has no data"
    exit 1
fi

if [ "$MULTIVERSE_LINE_COUNT" -le 1 ]; then
    echo "FAILED: Multiverse CSV has no data"
    exit 1
fi

# Compare CSV files using Python script
python3 "$COMPARE_SCRIPT" "$ASTRA_SIM_CSV" "$MULTIVERSE_CSV" "$DIFF_CSV" "$SAME_CSV"

exit $?
