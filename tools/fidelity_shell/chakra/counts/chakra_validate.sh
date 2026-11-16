#!/bin/bash

BASE_DIR=$(cd $(dirname $0); pwd)



ASTRA_SIM_LOG_FILE=$BASE_DIR/test_chakra_astrasim.log
MULTIVERSE_LOG_FILE=$BASE_DIR/test_chakra_multiverse.log
# Define output files
ASTRA_SIM_CSV=$BASE_DIR/astrasim_chakra.csv
MULTIVERSE_CSV=$BASE_DIR/multiverse_chakra.csv

# Process log file to extract dependency information
# ASTRA_SIM format: [TEST_CHAKRA_DEPENDENCY] issue id = X!
# Multiverse format: [TEST CHAKRA NODE] remove node: X
process_log() {
    local LOG_FILE=$1
    local OUTPUT_CSV=$2
    local LOG_TYPE=$3
    
    # Write CSV header
    echo "node_id" > $OUTPUT_CSV
    
    # Extract node IDs based on log type
    if [ "$LOG_TYPE" = "astrasim" ]; then
        # Extract ASTRA_SIM issue IDs
        grep 'TEST_CHAKRA_DEPENDENCY' "$LOG_FILE" | grep 'issue' | \
            grep -oP 'issue id = \K\d+' | sort -n | uniq >> $OUTPUT_CSV
    else
        # Extract Multiverse remove node IDs (two-stage extraction to handle truncated logs)
        # Stage 1: Extract normal format "remove node: X"
        grep 'TEST CHAKRA NODE' "$LOG_FILE" | grep 'remove node:' | \
            grep -oP 'remove node: \K\d+' > /tmp/multiverse_stage1.txt
        
        # Stage 2: Extract truncated format "de: X" (strict line match)
        grep -E '^de: [0-9]+$' "$LOG_FILE" | \
            grep -oP 'de: \K\d+' > /tmp/multiverse_stage2.txt
        
        # Merge, sort, and deduplicate
        cat /tmp/multiverse_stage1.txt /tmp/multiverse_stage2.txt | \
            sort -n | uniq >> $OUTPUT_CSV
        
        # Cleanup temp files
        rm -f /tmp/multiverse_stage1.txt /tmp/multiverse_stage2.txt
    fi
    
    # Remove trailing empty lines
    sed -i '/^$/d' $OUTPUT_CSV
    
    echo "CSV saved to: $OUTPUT_CSV"
}

# Run processors
echo "Processing ASTRA_SIM log..."
process_log "$ASTRA_SIM_LOG_FILE" "$ASTRA_SIM_CSV" "astrasim"

echo "Processing Multiverse log..."
process_log "$MULTIVERSE_LOG_FILE" "$MULTIVERSE_CSV" "multiverse"

echo ""
echo "Done. Files generated:"
echo "  - $ASTRA_SIM_CSV"
echo "  - $MULTIVERSE_CSV"
echo ""

# Compare CSV files - data is already sorted during extraction
# Just use the CSV files directly since sorting is done in process_log
ASTRA_SIM_SORTED="$ASTRA_SIM_CSV"
MULTIVERSE_SORTED="$MULTIVERSE_CSV"

# Compare CSV files
echo "Comparing CSV files..."
if diff -q "$ASTRA_SIM_SORTED" "$MULTIVERSE_SORTED" > /dev/null 2>&1; then
    echo "PASSED: CSV files are identical"
else
    echo "FAILED: CSV files differ"
    echo ""
    echo "First 20 differences:"
    diff "$ASTRA_SIM_SORTED" "$MULTIVERSE_SORTED" | head -n 40
    exit 1
fi

