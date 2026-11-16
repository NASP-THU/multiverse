#!/usr/bin/env python3
"""
Time error calculation script.
Accepts two v3 files (Multiverse + Astra-Sim), compares them by chakra_node groups.

v3 file format (output of 3.filter_and_sort.py):
  <chakra_node>
  flow_id start_time src dst size standalone_fct fct
  ...
  (blank line)
  <chakra_node>
  ...

Two comparison modes:
  - Per chakra_node: first flow's fct error rate
  - End-to-End: last flow's end_time (start_time + fct) error rate
"""
import os
import sys
import csv
import argparse


def parse_data_line(line):
    """
    Parse data line: flow_id start_time src dst size standalone_fct fct
    Returns dict with parsed values
    """
    parts = line.strip().split()
    if len(parts) < 7:
        return None

    try:
        return {
            'flow_id': int(parts[0]),
            'start_time': int(parts[1]),
            'src': int(parts[2]),
            'dst': int(parts[3]),
            'size': int(parts[4]),
            'standalone_fct': int(parts[5]),
            'fct': int(parts[6])
        }
    except (ValueError, IndexError):
        return None


def parse_v3_file(filepath):
    """
    Parse a v3 file into chakra_node groups.
    Returns: OrderedDict {chakra_node(int): [list of flow dicts]}
    """
    from collections import OrderedDict

    if not os.path.exists(filepath):
        print(f"Error: file not found: {filepath}")
        return None

    groups = OrderedDict()
    current_node = None

    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            parts = line.split()
            if len(parts) == 1:
                # chakra_node header line (single integer)
                try:
                    current_node = int(parts[0])
                    if current_node not in groups:
                        groups[current_node] = []
                except ValueError:
                    pass
            elif len(parts) >= 7 and current_node is not None:
                # data line
                flow = parse_data_line(line)
                if flow:
                    groups[current_node].append(flow)

    return groups


def calc_error_rate(measured, baseline):
    """Calculate error rate: (measured - baseline) / baseline * 100"""
    if baseline == 0:
        return float('inf') if measured != 0 else 0.0
    return (measured - baseline) / baseline * 100


def analyze_per_group(astrasim_groups, multiverse_groups, result_dir):
    """
    Compare the first flow's fct in each matching chakra_node group.
    Returns list of result dicts.
    """
    common_nodes = sorted(set(astrasim_groups.keys()) & set(multiverse_groups.keys()))

    if not common_nodes:
        print("Warning: no matching chakra_node groups between the two files")
        return []

    results = []
    for node in common_nodes:
        a_flows = astrasim_groups[node]
        m_flows = multiverse_groups[node]
        if not a_flows or not m_flows:
            continue

        a_fct = a_flows[0]['fct']
        m_fct = m_flows[0]['fct']
        error = calc_error_rate(m_fct, a_fct)

        results.append({
            'chakra_node': node,
            'astrasim_fct': a_fct,
            'multiverse_fct': m_fct,
            'diff': m_fct - a_fct,
            'error_rate': error,
        })

    # Write per-group CSV
    if results:
        csv_path = os.path.join(result_dir, "per_group_error_report.csv")
        with open(csv_path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'chakra_node', 'astrasim_fct', 'multiverse_fct', 'diff', 'error_rate'
            ])
            writer.writeheader()
            for r in results:
                row = r.copy()
                row['error_rate'] = f"{r['error_rate']:.2f}%"
                writer.writerow(row)
        # per_group_error_report.csv written

    return results


def analyze_end_to_end(astrasim_groups, multiverse_groups, result_dir):
    """
    End-to-End: find the last flow (max start_time+fct) across ALL groups,
    compare end_time between Astra-Sim and Multiverse.
    """
    def get_global_last(groups):
        last = None
        for flows in groups.values():
            for fl in flows:
                end_time = fl['start_time'] + fl['fct']
                if last is None or end_time > last['end_time']:
                    last = {**fl, 'end_time': end_time}
        return last

    a_last = get_global_last(astrasim_groups)
    m_last = get_global_last(multiverse_groups)

    if not a_last or not m_last:
        print("Warning: cannot determine end-to-end timing")
        return None

    error = calc_error_rate(m_last['end_time'], a_last['end_time'])
    result = {
        'astrasim_end_time': a_last['end_time'],
        'multiverse_end_time': m_last['end_time'],
        'diff': m_last['end_time'] - a_last['end_time'],
        'error_rate': error,
    }

    csv_path = os.path.join(result_dir, "end_to_end_error_report.csv")
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=[
            'astrasim_end_time', 'multiverse_end_time', 'diff', 'error_rate'
        ])
        writer.writeheader()
        row = result.copy()
        row['error_rate'] = f"{result['error_rate']:.2f}%"
        writer.writerow(row)
    # end_to_end_error_report.csv written

    return result


def print_summary(group_results, e2e_result):
    """Print summary conclusion to console"""
    print("\n" + "=" * 70)
    print("                         Summary")
    print("=" * 70)

    if group_results:
        print(f"\n[ Per chakra_node FCT error ({len(group_results)} groups) ]")
        print("-" * 70)
        print(f"{'chakra_node':>12} {'AstraSim FCT':>14} {'Multiverse FCT':>16} {'Error Rate':>12}")
        print("-" * 70)

        for r in group_results:
            print(f"{r['chakra_node']:>12} {r['astrasim_fct']:>14} {r['multiverse_fct']:>16} {r['error_rate']:>+11.2f}%")

        # Average error rate
        avg_err = sum(abs(r['error_rate']) for r in group_results) / len(group_results)
        print("-" * 70)
        print(f"{'Avg |error|':>12} {'':>14} {'':>16} {avg_err:>11.2f}%")

    if e2e_result:
        print(f"\n[ End-to-End ]")
        print("-" * 70)
        print(f"AstraSim   End-Time: {e2e_result['astrasim_end_time']}")
        print(f"Multiverse End-Time: {e2e_result['multiverse_end_time']}")
        print(f"Diff:  {e2e_result['diff']:+d}")
        print(f"Error: {e2e_result['error_rate']:+.2f}%")

    print("=" * 70 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Calculate FCT time error between Multiverse and Astra-Sim v3 files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Usage (positional, consistent with scripts 1-3):
  python3 4.calc_error_rate.py <multiverse_v3> <astrasim_v3>
  python3 4.calc_error_rate.py 828_128_v3.log fct_a_828_v3.txt
  python3 4.calc_error_rate.py 828_128_v3.log fct_a_828_v3.txt -r /tmp/result -g collective
'''
    )
    parser.add_argument('multiverse_v3', help='Multiverse v3 file')
    parser.add_argument('astrasim_v3', help='Astra-Sim v3 file')
    parser.add_argument('-r', '--result-dir', default=None,
                        help='Output directory for CSV reports (default: same dir as multiverse file)')
    parser.add_argument('-g', '--group', choices=['all', 'collective', 'end_to_end'],
                        default='all',
                        help='all=per-group+e2e, collective=per-group only, end_to_end=e2e only')

    args = parser.parse_args()

    mv_file = args.multiverse_v3
    as_file = args.astrasim_v3

    # Parse both v3 files
    m_groups = parse_v3_file(mv_file)
    a_groups = parse_v3_file(as_file)

    if m_groups is None or a_groups is None:
        sys.exit(1)

    # Determine output directory
    result_dir = args.result_dir or os.path.dirname(os.path.abspath(mv_file))
    os.makedirs(result_dir, exist_ok=True)

    group_results = []
    e2e_result = None

    if args.group in ['all', 'collective']:
        group_results = analyze_per_group(a_groups, m_groups, result_dir)

    if args.group in ['all', 'end_to_end']:
        e2e_result = analyze_end_to_end(a_groups, m_groups, result_dir)

    print_summary(group_results, e2e_result)

    return 0 if (group_results or e2e_result) else 1


if __name__ == '__main__':
    sys.exit(main())
