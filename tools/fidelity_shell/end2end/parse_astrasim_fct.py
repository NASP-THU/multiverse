#!/usr/bin/env python3
"""
Parse ASTRA-SIM fct.txt file and generate CSV for end-to-end validation.

fct.txt format (from fprintf):
  fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu %d %lu %d %d\n",
      q->sip.Get(), q->dip.Get(),                    # $1=sip, $2=dip (hex IP)
      q->sport, q->dport, q->m_size,                 # $3=sport, $4=dport, $5=m_size
      q->startTime.GetTimeStep(),                    # $6=startTime
      (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct,  # $7=fct, $8=standalone_fct
      flow_info.npu_id, flow_info.chakra_node_id,    # $9=npu_id, $10=chakra_node_id
      flow_info.is_collective ? 1 : 0, flow_info.flow_index);  # $11=is_collective, $12=flow_index

IP format: 0b00XX01 where XX is NPU ID in hex (e.g., 0b000301 = NPU 3)
"""

import sys
import argparse


def ip_to_npu(ip_hex: str) -> int:
    """Extract NPU ID from IP address (0b00XX01 format)."""
    try:
        ip_int = int(ip_hex, 16)
        return (ip_int >> 8) & 0xff
    except ValueError:
        return -1


def parse_fct_file(fct_file: str, output_csv: str, target_npu: int, 
                   collective_only: bool = False) -> int:
    """
    Parse fct.txt and generate CSV file.
    
    Args:
        fct_file: Input fct.txt file path
        output_csv: Output CSV file path
        target_npu: Target NPU ID to filter (-1 for all NPUs)
        collective_only: If True, only include collective flows (is_collective=1)
    
    Returns the number of records processed.
    """
    records = []
    
    with open(fct_file, 'r') as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) < 12:
                continue
            
            sip = parts[0]           # hex IP
            dip = parts[1]           # hex IP
            m_size = parts[4]        # message size
            npu_id = int(parts[8])   # npu_id
            node_id = int(parts[9])  # chakra_node_id
            is_collective = int(parts[10])
            flow_index = int(parts[11])
            
            # Filter by target NPU if specified
            if target_npu >= 0 and npu_id != target_npu:
                continue
            
            # Filter by collective only if specified
            if collective_only and is_collective != 1:
                continue
            
            flow_src = ip_to_npu(sip)
            flow_dst = ip_to_npu(dip)
            
            if flow_src < 0 or flow_dst < 0:
                continue
            
            # Simplified record without comm_type and comm_size
            records.append({
                'npu_id': npu_id,
                'node_id': node_id,
                'flow_index': flow_index,
                'flow_src': flow_src,
                'flow_dst': flow_dst,
                'flow_msg_size': m_size
            })
    
    # Sort by (node_id, flow_index) for consistent ordering
    records.sort(key=lambda x: (x['node_id'], x['flow_index']))
    
    # Convert global flow_index to per-node index (starting from 0)
    # Group by (npu_id, node_id), then re-index within each group
    from collections import defaultdict
    grouped = defaultdict(list)
    for r in records:
        key = (r['npu_id'], r['node_id'])
        grouped[key].append(r)
    
    # Write to CSV (simplified format without comm_type and comm_size)
    with open(output_csv, 'w') as f:
        f.write("npu_id,node_id,flow.index,flow.src,flow.dst,flow.msg_size\n")
        
        # Sort groups by (npu_id, node_id), then re-index flows within each group
        for key in sorted(grouped.keys(), key=lambda k: (k[0], k[1])):
            flows = grouped[key]
            # Sort by original flow_index within group
            flows.sort(key=lambda x: x['flow_index'])
            # Assign new index starting from 0
            for idx, r in enumerate(flows):
                f.write(f"{r['npu_id']},{r['node_id']},{idx},"
                        f"{r['flow_src']},{r['flow_dst']},{r['flow_msg_size']}\n")
    
    return len(records)


def main():
    parser = argparse.ArgumentParser(description='Parse ASTRA-SIM fct.txt file')
    parser.add_argument('fct_file', help='Input fct.txt file path')
    parser.add_argument('output_csv', help='Output CSV file path')
    parser.add_argument('--target-npu', type=int, default=3, 
                        help='Target NPU ID to filter (-1 for all NPUs, default: 3)')
    parser.add_argument('--collective-only', action='store_true',
                        help='Only include collective flows (default: include all)')
    
    args = parser.parse_args()
    
    count = parse_fct_file(args.fct_file, args.output_csv, args.target_npu, 
                           args.collective_only)
    
    npu_info = f"NPU {args.target_npu}" if args.target_npu >= 0 else "all NPUs"
    flow_type = "collective flows" if args.collective_only else "all flows (collective + compute)"
    print(f"Processed {count} {flow_type} for {npu_info}")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())

