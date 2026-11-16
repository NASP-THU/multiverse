#!/usr/bin/env python3
"""
Compare ASTRA-SIM and Multiverse CSV files for end-to-end validation.

Compares flow data by (npu_id, node_id) groups, checking:
- flow_src, flow_dst, flow_msg_size

Note: comm_type and comm_size are ignored since fct.txt doesn't have them.
"""

import csv
import sys
import argparse
from collections import defaultdict


def read_csv_grouped(csv_file: str) -> dict:
    """
    Read CSV file and group by (npu_id, node_id).
    
    Returns:
        grouped_data: dict of (npu_id, node_id) -> list of flow records
    """
    grouped = defaultdict(list)
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row['npu_id'], row['node_id'])
            grouped[key].append({
                'flow_index': row['flow.index'],
                'flow_src': row['flow.src'],
                'flow_dst': row['flow.dst'],
                'flow_msg_size': row['flow.msg_size']
            })
    
    return grouped


def compare_csvs(astrasim_csv: str, multiverse_csv: str, 
                 diff_csv: str, same_csv: str) -> tuple:
    """
    Compare two CSV files and generate diff/same reports.
    
    Returns:
        (differences, matches) lists
    """
    astrasim = read_csv_grouped(astrasim_csv)
    multiverse = read_csv_grouped(multiverse_csv)
    
    # Get all unique keys
    all_keys = sorted(
        set(astrasim.keys()) | set(multiverse.keys()),
        key=lambda x: (int(x[0]), int(x[1]))
    )
    
    differences = []
    matches = []
    
    for key in all_keys:
        npu_id, node_id = key
        a_flows = astrasim.get(key, [])
        m_flows = multiverse.get(key, [])
        
        if not a_flows and m_flows:
            differences.append({
                'npu_id': npu_id,
                'node_id': node_id,
                'status': 'MISSING_IN_ASTRASIM',
                'astrasim_flow_count': 0,
                'multiverse_flow_count': len(m_flows),
                'details': f"Multiverse has {len(m_flows)} flows"
            })
        elif a_flows and not m_flows:
            differences.append({
                'npu_id': npu_id,
                'node_id': node_id,
                'status': 'MISSING_IN_MULTIVERSE',
                'astrasim_flow_count': len(a_flows),
                'multiverse_flow_count': 0,
                'details': f"ASTRA-SIM has {len(a_flows)} flows"
            })
        elif len(a_flows) != len(m_flows):
            differences.append({
                'npu_id': npu_id,
                'node_id': node_id,
                'status': 'FLOW_COUNT_MISMATCH',
                'astrasim_flow_count': len(a_flows),
                'multiverse_flow_count': len(m_flows),
                'details': f"ASTRA-SIM: {len(a_flows)}, Multiverse: {len(m_flows)}"
            })
        else:
            # Sort flows by flow_index for consistent comparison
            a_sorted = sorted(a_flows, key=lambda x: int(x['flow_index']))
            m_sorted = sorted(m_flows, key=lambda x: int(x['flow_index']))
            
            # Compare each flow
            flow_match = True
            mismatch_details = []
            for i, (a, m) in enumerate(zip(a_sorted, m_sorted)):
                if (str(a['flow_src']) != str(m['flow_src']) or
                    str(a['flow_dst']) != str(m['flow_dst']) or
                    str(a['flow_msg_size']) != str(m['flow_msg_size'])):
                    flow_match = False
                    mismatch_details.append(
                        f"idx[{a['flow_index']}vs{m['flow_index']}]: "
                        f"A({a['flow_src']}->{a['flow_dst']},{a['flow_msg_size']}) vs "
                        f"M({m['flow_src']}->{m['flow_dst']},{m['flow_msg_size']})"
                    )
            
            if not flow_match:
                details = '; '.join(mismatch_details[:3])
                if len(mismatch_details) > 3:
                    details += '...'
                differences.append({
                    'npu_id': npu_id,
                    'node_id': node_id,
                    'status': 'FLOW_DATA_MISMATCH',
                    'astrasim_flow_count': len(a_flows),
                    'multiverse_flow_count': len(m_flows),
                    'details': details
                })
            else:
                matches.append({
                    'npu_id': npu_id,
                    'node_id': node_id,
                    'flow_count': len(a_flows)
                })
    
    # Write differences to CSV
    with open(diff_csv, 'w', newline='') as f:
        if differences:
            writer = csv.DictWriter(f, fieldnames=[
                'npu_id', 'node_id', 'status',
                'astrasim_flow_count', 'multiverse_flow_count', 'details'
            ])
            writer.writeheader()
            writer.writerows(differences)
    
    # Write matches to CSV
    with open(same_csv, 'w', newline='') as f:
        if matches:
            writer = csv.DictWriter(f, fieldnames=[
                'npu_id', 'node_id', 'flow_count'
            ])
            writer.writeheader()
            writer.writerows(matches)
    
    return differences, matches


def print_results(astrasim: dict, multiverse: dict, 
                  differences: list, matches: list):
    """Print comparison results to stdout."""
    print("============================================")
    print("Validation Results")
    print("============================================")
    print(f"ASTRA-SIM nodes: {len(astrasim)}")
    print(f"Multiverse nodes: {len(multiverse)}")
    print(f"Total ASTRA-SIM flows: {sum(len(v) for v in astrasim.values())}")
    print(f"Total Multiverse flows: {sum(len(v) for v in multiverse.values())}")
    print(f"Matching nodes: {len(matches)}")
    print(f"Different nodes: {len(differences)}")
    
    if not differences:
        print("PASSED: All Chakra nodes and flows match!")
    else:
        print(f"FAILED: {len(differences)} node differences found")
        pass
        print("Differences:")
        print("-" * 130)
        print(f"{'npu_id':<8} {'node_id':<10} {'status':<25} "
              f"{'astra_flows':<12} {'multi_flows':<12} {'details':<60}")
        print("-" * 130)
        
        for diff in differences[:30]:
            details = diff['details']
            if len(details) > 60:
                details = details[:58] + '..'
            print(f"{diff['npu_id']:<8} {diff['node_id']:<10} {diff['status']:<25} "
                  f"{diff['astrasim_flow_count']:<12} {diff['multiverse_flow_count']:<12} "
                  f"{details:<60}")
        
        if len(differences) > 30:
            print(f"... and {len(differences) - 30} more differences")
        
        print("-" * 130)


def main():
    parser = argparse.ArgumentParser(description='Compare ASTRA-SIM and Multiverse CSV files')
    parser.add_argument('astrasim_csv', help='ASTRA-SIM CSV file path')
    parser.add_argument('multiverse_csv', help='Multiverse CSV file path')
    parser.add_argument('diff_csv', help='Output diff CSV file path')
    parser.add_argument('same_csv', help='Output same CSV file path')
    
    args = parser.parse_args()
    
    # Read data for stats
    astrasim = read_csv_grouped(args.astrasim_csv)
    multiverse = read_csv_grouped(args.multiverse_csv)
    
    # Compare
    differences, matches = compare_csvs(
        args.astrasim_csv, args.multiverse_csv,
        args.diff_csv, args.same_csv
    )
    
    # Print results
    print_results(astrasim, multiverse, differences, matches)
    
    # Return exit code
    return 1 if differences else 0


if __name__ == '__main__':
    sys.exit(main())

