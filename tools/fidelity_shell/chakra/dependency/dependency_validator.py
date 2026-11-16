#!/usr/bin/env python3
"""
Chakra Dependency Validator

Validate that ASTRA-SIM and Multiverse execution sequences respect
the topological order defined in Chakra ET files.

Validation Logic:
    1. Parse ET files to build DAG: {parent: [children]}
    2. Parse simulator logs to extract remove sequence
    3. For each edge (parent -> child), verify parent is removed before child
    4. Output violations to CSV
"""

import sys
import re
import csv
import argparse
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Set, Tuple


# ============================================================================
# 1. Parse ET files to build DAG
# ============================================================================

def parse_et_dag(et_dir: str, npu_id: int = None) -> Dict[int, List[int]]:
    """
    Parse Chakra ET files and build DAG (predecessor -> successors)
    
    In topological order: predecessor must be removed before successor.
    
    Args:
        et_dir: Directory containing npu.*.et files
        npu_id: Optional specific NPU to parse (default: all)
    
    Returns:
        Dict mapping predecessor_id -> list of successor_ids
    """
    et_path = Path(et_dir)
    
    # Setup protobuf imports
    chakra_paths = [
        Path('/app/astra-sim/astra-sim-2.0-flowsim/extern/chakra/src'),
        Path('/app/astra-sim/aicb-plus/workload_generator/chakra/schema/protobuf'),
        Path('/app/astra-sim/aicb-plus/workload_generator/chakra'),
    ]
    for p in chakra_paths:
        if p.exists() and str(p) not in sys.path:
            sys.path.insert(0, str(p))
    
    try:
        from et_def_pb2 import Node as ChakraNode, GlobalMetadata
        from third_party.utils.protolib import openFileRd, decodeMessage
    except ImportError as e:
        print(f"Error: Failed to import Chakra protobuf: {e}")
        sys.exit(1)
     
    # Find ET files
    if npu_id is not None:
        et_files = [et_path / f"npu.{npu_id}.et"]
    else:
        et_files = sorted(et_path.glob("npu.*.et"))
    
    if not et_files:
        print(f"Error: No ET files found in {et_dir}")
        sys.exit(1)
    
    # Build DAG: predecessor -> [successors]
    dag = defaultdict(list)
    total_nodes = 0
    total_edges = 0
    
    for et_file in et_files:
        if not et_file.exists():
            continue
        
        proto_in = openFileRd(str(et_file))
        
        # Read global metadata
        global_metadata = GlobalMetadata()
        decodeMessage(proto_in, global_metadata)
        
        # Read nodes
        node = ChakraNode()
        while decodeMessage(proto_in, node):
            node_id = node.id
            total_nodes += 1
            
            # data_deps: node depends on these predecessors
            for pred_id in node.data_deps:
                dag[pred_id].append(node_id)
                total_edges += 1
            
            # ctrl_deps: node depends on these predecessors
            for pred_id in node.ctrl_deps:
                dag[pred_id].append(node_id)
                total_edges += 1
            
            node = ChakraNode()
        
        proto_in.close()
    
    print(f"DAG: {total_nodes} nodes, {total_edges} edges")
    return dict(dag)


# ============================================================================
# 2. Parse simulator logs to extract remove sequence
# ============================================================================

# Regex patterns
ASTRASIM_REMOVE_PATTERN = re.compile(r'\[TEST_CHAKRA_DEPENDENCY\]\s+remove\d+\s+id\s*=\s*(\d+)')
MULTIVERSE_REMOVE_PATTERN = re.compile(r'\[TEST CHAKRA NODE\]\s+remove node:\s*(-?\d+)')
# Truncated Multiverse log format: "de: X" (line starts with "de:")
MULTIVERSE_TRUNCATED_PATTERN = re.compile(r'^de:\s*(\d+)$')


def parse_remove_sequence(log_file: str, simulator: str) -> List[int]:
    """
    Parse simulator log and extract remove sequence
    
    Args:
        log_file: Path to console log
        simulator: 'astrasim' or 'multiverse'
    
    Returns:
        List of node_ids in removal order
    
    Note:
        For Multiverse, handles truncated log format where 
        "[TEST CHAKRA NODE] remove node: X" may appear as "de: X"
    """
    log_path = Path(log_file)
    if not log_path.exists():
        print(f"Warning: Log file not found: {log_file}")
        return []
    
    sequence = []
    seen = set()  # Deduplicate
    
    with open(log_path, 'r', errors='ignore') as f:
        for line in f:
            node_id = None
            
            if simulator == 'astrasim':
                match = ASTRASIM_REMOVE_PATTERN.search(line)
                if match:
                    node_id = int(match.group(1))
            else:  # multiverse
                # Stage 1: Try normal format
                match = MULTIVERSE_REMOVE_PATTERN.search(line)
                if match:
                    node_id = int(match.group(1))
                else:
                    # Stage 2: Try truncated format "de: X"
                    match = MULTIVERSE_TRUNCATED_PATTERN.match(line.strip())
                    if match:
                        node_id = int(match.group(1))
            
            if node_id is not None:
                # Skip invalid nodes (Multiverse uses -1 for init)
                if node_id < 0:
                    continue
                
                # Deduplicate: only first occurrence counts
                if node_id not in seen:
                    sequence.append(node_id)
                    seen.add(node_id)
    
    return sequence


# ============================================================================
# 3. Validate topological order
# ============================================================================

def get_all_dag_nodes(dag: Dict[int, List[int]]) -> Set[int]:
    """Get all nodes in DAG (predecessors and successors)"""
    all_nodes = set(dag.keys())
    for successors in dag.values():
        all_nodes.update(successors)
    return all_nodes


def validate_nodes(dag: Dict[int, List[int]], sequence: List[int], verbose: bool = False) -> Tuple[bool, Set[int], Set[int]]:
    """
    Validate that sequence contains all DAG nodes
    
    Returns:
        (passed, missing_nodes, extra_nodes)
    """
    dag_nodes = get_all_dag_nodes(dag)
    seq_nodes = set(sequence)
    
    missing = dag_nodes - seq_nodes  # In DAG but not in sequence
    extra = seq_nodes - dag_nodes    # In sequence but not in DAG
    
    passed = len(missing) == 0
    
    if verbose:
        print(f"  DAG nodes: {len(dag_nodes)}, Sequence nodes: {len(seq_nodes)}")
        if missing:
            print(f"  ⚠ Missing nodes (in DAG but not removed): {sorted(missing)}")
        if extra:
            print(f"  ⚠ Extra nodes (removed but not in DAG): {sorted(extra)}")
        if passed:
            print(f"  Node count check: ✓")
        else:
            print(f"  Node count check: ✗")
    
    return passed, missing, extra


def validate_edges(dag: Dict[int, List[int]], sequence: List[int], verbose: bool = False) -> List[Tuple[int, int, int, int]]:
    """
    Validate that remove sequence respects DAG dependencies (topological order)
    
    Args:
        dag: {predecessor: [successors]} from ET files
        sequence: remove order from log
        verbose: print detailed validation log
    
    Returns:
        List of violations: [(pred, succ, pred_pos, succ_pos), ...]
        A violation means successor was removed before predecessor
    """
    # Build position index: node -> position in sequence
    position = {node_id: idx for idx, node_id in enumerate(sequence)}
    
    violations = []
    checked = 0
    first_edge = True
    
    # Sort by predecessor for consistent output
    for pred in sorted(dag.keys()):
        successors = dag[pred]
        pred_pos = position.get(pred, -1)
        if pred_pos == -1:
            continue  # Predecessor not in sequence
        
        for succ in sorted(successors):
            succ_pos = position.get(succ, -1)
            if succ_pos == -1:
                continue  # Successor not in sequence
            
            checked += 1
            
            # Violation: successor removed before predecessor
            if succ_pos < pred_pos:
                violations.append((pred, succ, pred_pos, succ_pos))
                if verbose:
                    line = f"    edge {pred:>3}->{succ:<3}  pos[{pred_pos:>3}, {succ_pos:>3}]  ✗"
                    if first_edge:
                        line += "  # pos[pred, succ], succ before pred = violation"
                        first_edge = False
                    print(line)
            else:
                if verbose:
                    line = f"    edge {pred:>3}->{succ:<3}  pos[{pred_pos:>3}, {succ_pos:>3}]  ✓"
                    if first_edge:
                        line += "  # pos[pred, succ], pred before succ = OK"
                        first_edge = False
                    print(line)
    
    if verbose:
        print(f"  Edges checked: {checked}")
    
    return violations


# ============================================================================
# 4. Output
# ============================================================================

def write_violations_csv(violations: List[Tuple], output_file: str):
    """Write violations to CSV file"""
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['predecessor', 'successor', 'pred_pos', 'succ_pos'])
        for v in violations:
            writer.writerow(v)


def write_chakra_standard_csv(dag: Dict[int, List[int]], output_file: str):
    """
    Write Chakra standard dependency to CSV file
    
    Format: predecessor_id, successors (nodes that depend on this predecessor)
    """
    # Get all node IDs
    all_nodes = set(dag.keys())
    for successors in dag.values():
        all_nodes.update(successors)
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['predecessor', 'successors'])
        
        for node_id in sorted(all_nodes):
            successors = dag.get(node_id, [])
            successors_str = ','.join(map(str, sorted(successors))) if successors else ''
            writer.writerow([node_id, successors_str])


def main():
    # Default paths
    BASE_DIR = Path(__file__).parent
    DEFAULT_ET_DIR = '/app/astra-sim/astra-sim-2.0-flowsim/input/workload/workload_13'
    DEFAULT_ASTRASIM_LOG = BASE_DIR / 'test_dp_astrasim_v1222.log'
    DEFAULT_MULTIVERSE_LOG = BASE_DIR / 'test_dp_multiverse_v2.log'
    DEFAULT_NPU_ID = 26
    
    parser = argparse.ArgumentParser(description='Validate Chakra dependency topological order')
    parser.add_argument('--et-dir', default=DEFAULT_ET_DIR, help='Directory containing ET files')
    parser.add_argument('--astrasim-log', default=str(DEFAULT_ASTRASIM_LOG), help='ASTRA-SIM console log')
    parser.add_argument('--multiverse-log', default=str(DEFAULT_MULTIVERSE_LOG), help='Multiverse console log')
    parser.add_argument('--output-dir', default=str(BASE_DIR), help='Output directory for CSV files')
    parser.add_argument('--npu-id', type=int, default=DEFAULT_NPU_ID, help='Specific NPU ID to validate (default: 26)')
    parser.add_argument('-q', '--quiet', action='store_true', help='Hide detailed validation log (show only summary)')
    
    args = parser.parse_args()
    
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("=" * 60)
    print("Chakra Dependency Validator")
    print("=" * 60)
    print(f"ET Directory: {args.et_dir}")
    print(f"ASTRA-SIM Log: {args.astrasim_log}")
    print(f"Multiverse Log: {args.multiverse_log}")
    print()
    
    # 1. Parse ET files -> DAG
    print("[1/4] Parsing ET files...")
    dag = parse_et_dag(args.et_dir, args.npu_id)
    
    # Output Chakra standard dependency
    chakra_csv = output_dir / 'chakra_standard.csv'
    write_chakra_standard_csv(dag, chakra_csv)
    print(f"  Chakra standard: {chakra_csv}")
    print()
    
    # 2. Validate Multiverse (first for screenshot: header shows Multiverse)
    print("[2/4] Validating Multiverse...")
    multiverse_seq = parse_remove_sequence(args.multiverse_log, 'multiverse')
    
    # Step 1: Validate nodes
    print("  [Step 1] Node count validation:")
    multiverse_nodes_ok, multiverse_missing, multiverse_extra = validate_nodes(dag, multiverse_seq, verbose=not args.quiet)
    
    # Step 2: Validate edges (only if nodes match)
    multiverse_violations = []
    if multiverse_nodes_ok:
        print("  [Step 2] Edge validation:")
        multiverse_violations = validate_edges(dag, multiverse_seq, verbose=not args.quiet)
        multiverse_csv = output_dir / 'multiverse_violations.csv'
        write_violations_csv(multiverse_violations, multiverse_csv)
    else:
        print("  [Step 2] Edge validation: SKIPPED (node count mismatch)")
    
    # Summary
    print("  [Result]", end=" ")
    if multiverse_nodes_ok and not multiverse_violations:
        print("PASSED ✓")
    else:
        print("FAILED ✗")
        if multiverse_violations:
            print(f"    Edge violations: {len(multiverse_violations)}")
            print(f"    Output: {multiverse_csv}")
    print()
    
    # 3. Validate ASTRA-SIM (second for screenshot: footer shows ASTRA-SIM summary)
    print("[3/4] Validating ASTRA-SIM...")
    astrasim_seq = parse_remove_sequence(args.astrasim_log, 'astrasim')
    
    # Step 1: Validate nodes
    print("  [Step 1] Node count validation:")
    astrasim_nodes_ok, astrasim_missing, astrasim_extra = validate_nodes(dag, astrasim_seq, verbose=not args.quiet)
    
    # Step 2: Validate edges (only if nodes match)
    astrasim_violations = []
    if astrasim_nodes_ok:
        print("  [Step 2] Edge validation:")
        astrasim_violations = validate_edges(dag, astrasim_seq, verbose=not args.quiet)
        astrasim_csv = output_dir / 'astrasim_violations.csv'
        write_violations_csv(astrasim_violations, astrasim_csv)
    else:
        print("  [Step 2] Edge validation: SKIPPED (node count mismatch)")
    
    # Summary
    print("  [Result]", end=" ")
    if astrasim_nodes_ok and not astrasim_violations:
        print("PASSED ✓")
    else:
        print("FAILED ✗")
        if astrasim_violations:
            print(f"    Edge violations: {len(astrasim_violations)}")
            print(f"    Output: {astrasim_csv}")
    print()
    
    # 4. Summary
    print("[4/4] Summary")
    print("=" * 60)
    
    # Get DAG stats
    dag_nodes = get_all_dag_nodes(dag)
    dag_node_count = len(dag_nodes)
    dag_edge_count = sum(len(children) for children in dag.values())
    
    print(f"Chakra Standard: {dag_node_count} nodes, {dag_edge_count} edges")
    print()
    
    # Multiverse (first)
    multiverse_passed = multiverse_nodes_ok and not multiverse_violations
    print(f"Multiverse:")
    print(f"  Nodes: {len(multiverse_seq)}/{dag_node_count} {'✓' if multiverse_nodes_ok else '✗'}", end="")
    if not multiverse_nodes_ok:
        print(f" (missing: {len(multiverse_missing)}, extra: {len(multiverse_extra)})")
    else:
        print()
    print(f"  Edges: {dag_edge_count - len(multiverse_violations)}/{dag_edge_count} {'✓' if not multiverse_violations else '✗'}", end="")
    if multiverse_violations:
        print(f" ({len(multiverse_violations)} violations)")
    else:
        print()
    print(f"  Status: {'PASSED' if multiverse_passed else 'FAILED'}")
    print()
    
    # ASTRA-SIM (second)
    astrasim_passed = astrasim_nodes_ok and not astrasim_violations
    print(f"ASTRA-SIM:")
    print(f"  Nodes: {len(astrasim_seq)}/{dag_node_count} {'✓' if astrasim_nodes_ok else '✗'}", end="")
    if not astrasim_nodes_ok:
        print(f" (missing: {len(astrasim_missing)}, extra: {len(astrasim_extra)})")
    else:
        print()
    print(f"  Edges: {dag_edge_count - len(astrasim_violations)}/{dag_edge_count} {'✓' if not astrasim_violations else '✗'}", end="")
    if astrasim_violations:
        print(f" ({len(astrasim_violations)} violations)")
    else:
        print()
    print(f"  Status: {'PASSED' if astrasim_passed else 'FAILED'}")
    
    print("=" * 60)
    
    # Exit code
    if not astrasim_passed or not multiverse_passed:
        sys.exit(1)
    sys.exit(0)


if __name__ == '__main__':
    main()

