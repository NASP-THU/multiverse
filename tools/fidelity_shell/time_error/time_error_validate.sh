#!/bin/bash

# Time Error Validation Script
# Both Multiverse and Astra-Sim raw logs are required.
# - Multiverse: steps 1 -> 2 -> 3 -> 4
# - Astra-Sim:  steps 2 -> 3 -> 4
# Step 4 pairs the two results by filename (_m_ / _a_) and calculates time error.

set -e

BASE_DIR=$(cd $(dirname $0); pwd)
SCRIPT_DIR=$BASE_DIR/script

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_step() {
    echo -e "${BLUE}[STEP $1]${NC} $2"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# ======================== Macro Config ========================
# Raw log paths: both are REQUIRED

# Case 1.5.3
# MULTIVERSE_LOG_FILE=${MULTIVERSE_LOG_FILE:-"/app/astra-sim/multiverse-dev/4216_128.log"}
# ASTRA_SIM_LOG_FILE=${ASTRA_SIM_LOG_FILE:-"/app/astra-sim/astra-sim-2.0-flowsim/extern/network_backend/ns-3/scratch/output/fct_a_4216.txt"}

# # Case 1.5.4
MULTIVERSE_LOG_FILE=${MULTIVERSE_LOG_FILE:-"/app/astra-sim/multiverse-dev/828_128.log"}
ASTRA_SIM_LOG_FILE=${ASTRA_SIM_LOG_FILE:-"/app/astra-sim/astra-sim-2.0-flowsim/extern/network_backend/ns-3/scratch/output/fct_a_828.txt"}

# Output directory for intermediate and final results
RESULT_DIR=${RESULT_DIR:-"$BASE_DIR/result"}

# NPU ID for filtering (step 3)
NPU_ID=${NPU_ID:-0}

# Group selection for error calculation (step 4):
#   end_to_end  - only End-to-End summary (default)
#   all         - End-to-End + per chakra_node detail table
#   collective  - only per chakra_node detail table
GROUP=${GROUP:-"end_to_end"}
# GROUP=all
# ==============================================================

usage() {
    echo "Usage: $0 -m <multiverse_log> -a <astrasim_log> [OPTIONS]"
    echo ""
    echo "Calculate FCT time error between Multiverse and Astra-Sim."
    echo "Both -m and -a are REQUIRED."
    echo ""
    echo "Options:"
    echo "  -m, --multiverse FILE   Multiverse raw log (pipeline: steps 1->2->3->4)"
    echo "  -a, --astrasim FILE     Astra-Sim raw log  (pipeline: steps 2->3->4)"
    echo "  -r, --result-dir DIR    Output directory (default: $BASE_DIR/result)"
    echo "  -n, --npu-id ID         NPU ID for filtering (default: 0)"
    echo "  -g, --group GROUP       Error group: all, collective, end_to_end (default: all)"
    echo "  -h, --help              Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  MULTIVERSE_LOG_FILE  Multiverse raw log path"
    echo "  ASTRA_SIM_LOG_FILE   Astra-Sim raw log path"
    echo "  RESULT_DIR           Output directory"
    echo "  NPU_ID               NPU ID for filtering"
    echo "  GROUP                Error group (all/collective/end_to_end)"
    echo ""
    echo "Examples:"
    echo "  $0 -m path/to/multiverse.log -a path/to/astrasim_fct.txt"
    echo "  $0 -m mv.log -a as.txt -n 0 -g collective -r /tmp/result"
    echo "  MULTIVERSE_LOG_FILE=mv.log ASTRA_SIM_LOG_FILE=as.txt $0"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -m|--multiverse)
            MULTIVERSE_LOG_FILE="$2"
            shift 2
            ;;
        -a|--astrasim)
            ASTRA_SIM_LOG_FILE="$2"
            shift 2
            ;;
        -r|--result-dir)
            RESULT_DIR="$2"
            shift 2
            ;;
        -n|--npu-id)
            NPU_ID="$2"
            shift 2
            ;;
        -g|--group)
            GROUP="$2"
            if [[ ! "$GROUP" =~ ^(all|collective|end_to_end)$ ]]; then
                print_error "Invalid group: $GROUP (must be: all, collective, end_to_end)"
                exit 1
            fi
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Validate: both logs are required
if [ -z "$MULTIVERSE_LOG_FILE" ] || [ -z "$ASTRA_SIM_LOG_FILE" ]; then
    print_error "Both -m (Multiverse) and -a (Astra-Sim) log paths are required."
    echo ""
    usage
    exit 1
fi

if [ ! -f "$MULTIVERSE_LOG_FILE" ]; then
    print_error "Multiverse log file not found: $MULTIVERSE_LOG_FILE"
    exit 1
fi

if [ ! -f "$ASTRA_SIM_LOG_FILE" ]; then
    print_error "Astra-Sim log file not found: $ASTRA_SIM_LOG_FILE"
    exit 1
fi

# Derive base names for pipeline outputs (_m_ / _a_ required for step 4 pairing)
multiverse_base=$(basename "$MULTIVERSE_LOG_FILE" .log)
astra_base=$(basename "$ASTRA_SIM_LOG_FILE")
astra_base="${astra_base%.txt}"
astra_base="${astra_base%.log}"

echo "=============================================================="
echo "          Time Error Validation Script"
echo "=============================================================="
echo ""
echo "Configuration:"
echo "  MULTIVERSE:   $MULTIVERSE_LOG_FILE"
echo "  ASTRA_SIM:    $ASTRA_SIM_LOG_FILE"
echo "  RESULT_DIR:   $RESULT_DIR"
echo "  NPU_ID:       $NPU_ID"
echo "  GROUP:        $GROUP"
echo ""

# Ensure result directory exists
mkdir -p "$RESULT_DIR"

# Multiverse pipeline: Step 1 -> 2 -> 3
print_step 1 "[Multiverse] Preprocessing raw log"
MULTIVERSE_PROCESSED="$RESULT_DIR/${multiverse_base}_m_processed.log"
python3 "$SCRIPT_DIR/1.process_multiverse_log.py" "$MULTIVERSE_LOG_FILE" "$MULTIVERSE_PROCESSED" > /dev/null

print_step 2 "[Multiverse] Unifying log format"
MULTIVERSE_UNIFIED="$RESULT_DIR/${multiverse_base}_m_unified.txt"
python3 "$SCRIPT_DIR/2.unify_log.py" "$MULTIVERSE_PROCESSED" "$MULTIVERSE_UNIFIED" out1 > /dev/null

print_step 3 "[Multiverse] Filtering and sorting"
MULTIVERSE_V3="$RESULT_DIR/${multiverse_base}_m_v3.log"
python3 "$SCRIPT_DIR/3.filter_and_sort.py" "$MULTIVERSE_UNIFIED" "$NPU_ID" "$MULTIVERSE_V3" > /dev/null

# Astra-Sim pipeline: Step 2 -> 3
print_step 2 "[Astra-Sim] Unifying log format"
ASTRA_UNIFIED="$RESULT_DIR/${astra_base}_a_unified.txt"
python3 "$SCRIPT_DIR/2.unify_log.py" "$ASTRA_SIM_LOG_FILE" "$ASTRA_UNIFIED" fct > /dev/null

print_step 3 "[Astra-Sim] Filtering and sorting"
ASTRA_V3="$RESULT_DIR/${astra_base}_a_v3.log"
python3 "$SCRIPT_DIR/3.filter_and_sort.py" "$ASTRA_UNIFIED" "$NPU_ID" "$ASTRA_V3" > /dev/null

# Step 4: Calculate time error
print_step 4 "Calculating time error"
python3 "$SCRIPT_DIR/4.calc_error_rate.py" "$MULTIVERSE_V3" "$ASTRA_V3" -r "$RESULT_DIR" -g "$GROUP"
