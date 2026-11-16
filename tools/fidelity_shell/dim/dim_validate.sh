#!/bin/bash

BASE_DIR=$(cd $(dirname $0); pwd)

ASTRA_SIM_LOG_FILE=$BASE_DIR/test_dim_astrasim.log
MULTIVERSE_LOG_FILE=$BASE_DIR/test_dim_multiverse.log

# Define output files
MULTIVERSE_CSV=$BASE_DIR/multiverse_dim.csv
ASTRA_SIM_CSV=$BASE_DIR/astrasim_dim.csv

# Dimensions to process
DIMENSIONS=("Local" "Horizontal" "Vertical")

# Process MULTIVERSE log file (two lines per entry)
# Strategy: Use "Dimension: Horizontal" as anchor, take -B 2 (Local) and -A 3 (Horizontal + Vertical)
process_multiverse_log() {
    local LOG_FILE=$1
    local OUTPUT_CSV=$2
    
    # Write CSV header
    echo "Dimension,Total Nodes,Current Node,Offset,Node IDs" > $OUTPUT_CSV
    
    VALID_LINES=$(grep "\[TEST DIM\]" $LOG_FILE | grep -B 2 -A 3 --no-group-separator "Dimension: Horizontal")
    # Process 6 lines at a time
    echo "$VALID_LINES" | while read -r local_info && read -r local_ids && \
                               read -r horiz_info && read -r horiz_ids && \
                               read -r vert_info && read -r vert_ids; do
        INFOS=("$local_info" "$horiz_info" "$vert_info")
        IDS=("$local_ids" "$horiz_ids" "$vert_ids")
        
        for i in 0 1 2; do
            TOTAL_NODES=$(echo "${INFOS[$i]}" | grep -oP 'Total Nodes: \K\d+')
            CURRENT_NODE=$(echo "${INFOS[$i]}" | grep -oP 'Current node: \K\d+')
            OFFSET=$(echo "${INFOS[$i]}" | grep -oP 'Offset: \K\d+')
            NODE_IDS=$(echo "${IDS[$i]}" | grep -oP 'Node IDs in ring: \K[\d ]+' | sed 's/ *$//' | tr ' ' ',')
            echo "${DIMENSIONS[$i]},$TOTAL_NODES,$CURRENT_NODE,$OFFSET,\"$NODE_IDS\"" >> $OUTPUT_CSV
        done
    done
    
    echo "CSV saved to: $OUTPUT_CSV"
}

# Process ASTRA_SIM log file (single line per entry)
# Strategy: Use "dimension: horizontal" as anchor, take -B 1 (local) and -A 1 (vertical) as valid group
process_astrasim_log() {
    local LOG_FILE=$1
    local OUTPUT_CSV=$2
    
    # Write CSV header
    echo "Dimension,Total Nodes,Current Node,Offset,Node IDs" > $OUTPUT_CSV
    
    # Extract valid 3-line groups: local, horizontal, vertical
    VALID_LINES=$(grep "\[TEST DIM 2\]" $LOG_FILE | grep -B 1 -A 1 --no-group-separator "dimension: horizontal")
    
    # Process 3 lines at a time
    echo "$VALID_LINES" | while read -r line_local && read -r line_horiz && read -r line_vert; do
        LINES=("$line_local" "$line_horiz" "$line_vert")
        
        for i in 0 1 2; do
            TOTAL_NODES=$(echo "${LINES[$i]}" | grep -oP 'total nodes in ring: \K\d+')
            CURRENT_NODE=$(echo "${LINES[$i]}" | grep -oP 'id: \K\d+')
            OFFSET=$(echo "${LINES[$i]}" | grep -oP 'offset: \K\d+')
            NODE_IDS=$(echo "${LINES[$i]}" | grep -oP 'ids=\[\K[^\]]+' | tr -d ' ')
            echo "${DIMENSIONS[$i]},$TOTAL_NODES,$CURRENT_NODE,$OFFSET,\"$NODE_IDS\"" >> $OUTPUT_CSV
        done
    done
    
    echo "CSV saved to: $OUTPUT_CSV"
}

# Run processors
echo "Processing MULTIVERSE log..."
process_multiverse_log "$MULTIVERSE_LOG_FILE" "$MULTIVERSE_CSV"

echo "Processing ASTRA_SIM log..."
process_astrasim_log "$ASTRA_SIM_LOG_FILE" "$ASTRA_SIM_CSV"

echo ""
echo "Done. Files generated:"
echo "  - $MULTIVERSE_CSV"
echo "  - $ASTRA_SIM_CSV"
echo ""

# Compare CSV files (skip last 3 lines due to potential incomplete data)
echo "Comparing CSV files..."
if diff -q <(head -n -3 "$MULTIVERSE_CSV") <(head -n -3 "$ASTRA_SIM_CSV") > /dev/null 2>&1; then
    echo "PASSED: CSV files are identical"
else
    echo "FAILED: CSV files differ"
    echo ""
    echo "First 20 differences:"
    diff <(head -n -3 "$MULTIVERSE_CSV") <(head -n -3 "$ASTRA_SIM_CSV") | head -n 40
    exit 1
fi
