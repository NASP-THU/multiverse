#!/bin/bash

# Loop K from 8 to 16, step size 2
for K in $(seq 8 2 8); do
    echo "Running gen_fattree_topo.py with K = $K"
    python gen_fattree_topo.py --K $K
done

# python gen_fattree_topo.py --K 8

echo "All runs completed!"