from enum import Enum
from typing import List, Optional
from dataclasses import dataclass, field
from parse_chakra import Node,Attribute,BoolList,Parse
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed
import os
import re

NODE_NAME_LENGTH = 20
NODE_DATA_DEPS_LENGTH = 10
NODE_ATTR_PER_LENGTH = 3
NODE_DURATION_MICROS_LENGTH = 1
placeholder = 2147483646  # Safe int32 value


class NodeType(Enum):
    COMP_NODE = 1
    COMM_SEND_NODE = 2
    COMM_RECV_NODE=3
    COMM_COLL_NODE=4
    
class AttributeKey(Enum):
    comm_para = 1
    comm_size = 2
    comm_src = 3
    comm_dst = 4
    comm_type = 5
    involved_dim=6

# Data type enumeration for identifying stored data types
class DataType(Enum):
    BOOL_VAL = 0      # boolVal
    UINT32_VAL = 1    # uint32Val  
    INT64_VAL = 2     # int64Val
    UINT64_VAL = 3    # uint64Val
    BOOL_LIST = 4     # boolList

# Define placeholder - for accurate conversion of 64-bit data, use safe int32 value


# 64-bit data processing functions
def split_int64_to_32bit(value: int) -> tuple[int, int]:
    """Split signed 64-bit integer into two 32-bit integers (ultra-simplified version)"""
    # Direct bitwise operations, C++ side will handle type conversion correctly
    low_32 = value & 0xFFFFFFFF  # Low 32 bits
    high_32 = (value >> 32) & 0xFFFFFFFF  # High 32 bits
    return low_32, high_32

def split_uint64_to_32bit(value: int) -> tuple[int, int]:
    """Split unsigned 64-bit integer into two 32-bit integers (ultra-simplified version)"""
    # Basic validation
    if value < 0:
        raise ValueError(f"uint64 value cannot be negative: {value}")
    
    # Direct bitwise operations
    low_32 = value & 0xFFFFFFFF  # Low 32 bits
    high_32 = (value >> 32) & 0xFFFFFFFF  # High 32 bits
    return low_32, high_32

def reconstruct_int64_from_32bit(low_32: int, high_32: int) -> int:
    """Reconstruct signed 64-bit integer from two 32-bit integers (ultra-simplified version)"""
    # Direct reconstruction of 64-bit value, then convert to signed
    value = (high_32 << 32) | low_32
    
    # If it exceeds int64 maximum value, it means it was originally negative
    if value > 9223372036854775807:
        value = value - 18446744073709551616
    
    return value

def reconstruct_uint64_from_32bit(low_32: int, high_32: int) -> int:
    """Reconstruct unsigned 64-bit integer from two 32-bit integers (ultra-simplified version)"""
    # Direct reconstruction of 64-bit value (keep as unsigned)
    value = (high_32 << 32) | low_32
    
    return value

# Keep old function names for backward compatibility (default to int64 processing)
def split_64bit_to_32bit(value: int) -> tuple[int, int]:
    """Backward compatibility function, default to int64 processing"""
    return split_int64_to_32bit(value)

def reconstruct_64bit_from_32bit(low_32: int, high_32: int) -> int:
    """Backward compatibility function, default to int64 processing"""
    return reconstruct_int64_from_32bit(low_32, high_32)

def node_to_int_array(node: Node) -> List[int]:
    int_array = []

    # Convert name to 20 int types
    name_ints = [ord(c) for c in node.name[:NODE_NAME_LENGTH]]  # Get ASCII values of first 20 characters
    name_ints += [placeholder] * (NODE_NAME_LENGTH - len(name_ints))  # Pad with placeholder if less than 20 characters
    int_array.extend(name_ints)

    # Convert type to int type
    node_type = NodeType[node.type].value if node.type in NodeType.__members__ else placeholder
    int_array.append(node_type)

    # Convert id to one int type
    node_id = int(node.id) if node.id is not None and node.id.isdigit() else placeholder
    int_array.append(node_id)

    # Ensure dataDeps is not None
    data_deps = node.dataDeps if node.dataDeps is not None else []
    # Convert data_deps to 10 int types
    data_deps_ints = [int(dep) if dep.isdigit() else placeholder for dep in data_deps[:NODE_DATA_DEPS_LENGTH]]
    data_deps_ints += [placeholder] * (NODE_DATA_DEPS_LENGTH - len(data_deps_ints))  # Pad with placeholder if less than 10
    int_array.extend(data_deps_ints)

    # Convert attr to 18 int types (6 attributes × 3 int/attribute)
    attr_ints = []  # Store all 18 ints for AttributeKey

    for key in AttributeKey:
            # Find if current AttributeKey exists in node.attr
            matching_attr = next((attr for attr in node.attr if attr.name == key.name), None)
            # If AttributeKey matches, set corresponding value
            if key.value in {1, 2, 3, 4, 5}:  # Corresponding to 3 ints (data1, data2, type)
                key_ints = [placeholder] * NODE_ATTR_PER_LENGTH
                if matching_attr:
                    if matching_attr.boolVal is not None:
                        key_ints[0] = int(matching_attr.boolVal)
                        key_ints[1] = placeholder  # bool only needs 1 int
                        key_ints[2] = DataType.BOOL_VAL.value
                    elif matching_attr.uint32Val is not None:
                        key_ints[0] = int(matching_attr.uint32Val)
                        key_ints[1] = placeholder  # uint32 only needs 1 int
                        key_ints[2] = DataType.UINT32_VAL.value
                    elif matching_attr.int64Val is not None:
                        # Use int64-specific split function
                        low_32, high_32 = split_int64_to_32bit(matching_attr.int64Val)
                        key_ints[0] = low_32
                        key_ints[1] = high_32
                        key_ints[2] = DataType.INT64_VAL.value
                    elif matching_attr.uint64Val is not None:
                        # Use uint64-specific split function
                        low_32, high_32 = split_uint64_to_32bit(matching_attr.uint64Val)
                        key_ints[0] = low_32
                        key_ints[1] = high_32
                        key_ints[2] = DataType.UINT64_VAL.value
            elif key.value == 6:  # involved_dim: pack 3 bools into 1 int
                key_ints = [placeholder] * NODE_ATTR_PER_LENGTH
                if matching_attr:
                    if matching_attr.boolList is not None:
                        # Pack up to 3 bool values into one int
                        bool_list_vals = matching_attr.boolList.values[:3]  # Take at most first 3 boolean values
                        packed_bools = 0
                        for i, val in enumerate(bool_list_vals):
                            if val:
                                packed_bools |= (1 << i)  # Set 1 at i-th bit
                        
                        key_ints[0] = packed_bools  # Packed bool value
                        key_ints[1] = placeholder   # Unused
                        key_ints[2] = DataType.BOOL_LIST.value  # 3rd position stores type

            # Add current AttributeKey's 3 ints to attr_ints
            attr_ints.extend(key_ints)

    # Add result to int_array
    int_array.extend(attr_ints)

    # Add durationMicros to end of array
    int_array.append(node.durationMicros)

    return int_array


# def int_array_to_node(int_array: List[int]) -> Node:
#     # Convert int array back to Node object

#     # 1. Parse name
#     name = ''.join([chr(i) for i in int_array[:20] if i != placeholder])

#     # 2. Parse type
#     node_type = NodeType(int_array[20]).name if int_array[20] in NodeType._value2member_map_ else 'UNKNOWN'

#     # 3. Parse id
#     node_id = str(int_array[21]) if int_array[21] != placeholder else None

#     # 4. Parse dataDeps
#     data_deps = [str(i) for i in int_array[22:32] if i != placeholder]

#     # 5. Parse attr
#     attrs = []
#     attr_start = 32  # Starting position of attribute values
#     for key in AttributeKey:
#         if key.value in {1, 2, 3, 4, 5, 6}:  # All attributes occupy 3 ints
#             key_ints = int_array[attr_start:attr_start + 3]
#             attr_start += 3
#         else:
#             continue

#         # Skip AttributeKey that is entirely placeholders (but check first 2, 3rd is type identifier)
#         if key_ints[0] == placeholder and key_ints[1] == placeholder and key_ints[2] == placeholder:
#             continue

#         # Parse data based on type identifier
#         if key_ints[2] != placeholder:
#             data_type = key_ints[2]
            
#             if data_type == DataType.BOOL_VAL.value:
#                 # Parse boolVal
#                 attrs.append(Attribute(name=key.name, boolVal=bool(key_ints[0])))
                
#             elif data_type == DataType.UINT32_VAL.value:
#                 # Parse uint32Val
#                 attrs.append(Attribute(name=key.name, uint32Val=key_ints[0]))
                
#             elif data_type == DataType.INT64_VAL.value:
#                 # Parse int64Val
#                 int64_val = reconstruct_int64_from_32bit(key_ints[0], key_ints[1])
#                 attrs.append(Attribute(name=key.name, int64Val=int64_val))
                
#             elif data_type == DataType.UINT64_VAL.value:
#                 # Parse uint64Val
#                 uint64_val = reconstruct_uint64_from_32bit(key_ints[0], key_ints[1])
#                 attrs.append(Attribute(name=key.name, uint64Val=uint64_val))
                
#             elif data_type == DataType.BOOL_LIST.value:
#                 # Parse boolList: unpack 3 bool values from packed int
#                 packed_bools = key_ints[0]
#                 if packed_bools != placeholder:
#                     bool_list_vals = []
#                     for i in range(3):  # Unpack 3 bool values
#                         bool_val = bool(packed_bools & (1 << i))
#                         bool_list_vals.append(bool_val)
#                     attrs.append(Attribute(name=key.name, boolList=BoolList(values=bool_list_vals)))

#     # 6. Parse durationMicros
#     duration_micros = int_array[attr_start]

#     # Construct Node object
#     return Node(
#         name=name,
#         type=node_type,
#         id=node_id,
#         dataDeps=data_deps,
#         attr=attrs,
#         durationMicros=duration_micros
#     )


# # Test example
# def test_conversion():
#     json_file_path = '/home/zhangran/madrona2/2/madrona_simple_example/scripts/input/npu.1.json'
#     nodes = Parse(json_file_path)
    
#     if nodes:
#         first_node = nodes[1]
#         print(f"Node object before conversion: {first_node}")
#         int_array = node_to_int_array(first_node)
#         print(f"Integer array after conversion: {int_array}")

#         node_back = int_array_to_node(int_array)
#         print(f"Node object after conversion back: {node_back}")

# Execute test
# test_conversion()

def nodes_to_int_array(json_file_path):
    # json_file_path = '/home/zhangran/madrona2/2/madrona_simple_example/scripts/input/npu.0.json'
    nodes = Parse(json_file_path)
    result=[]
    if nodes:
        for node in nodes:
            result.extend(node_to_int_array(node))
    return result
 
 

 
def extract_number_from_filename(filename):
    match = re.search(r'npu\.(\d+)\.json', Path(filename).name)
    return int(match.group(1)) if match else 0

def folder_to_int_array(folder_path, max_workers=None, show_progress=True):
    """
    Process all JSON files in a folder and convert to integer arrays.
    
    Args:
        folder_path: Path to the folder containing JSON files
        max_workers: Maximum number of worker processes (None for auto, 0 for single-threaded)
        show_progress: Whether to display progress information
    
    Returns:
        List of integer arrays, one per JSON file
    """
    folder = Path(folder_path)
    file_paths = [str(file) for file in folder.rglob('*.json')]
    file_paths = sorted(file_paths, key=extract_number_from_filename)
    
    # Check if any JSON files found
    if not file_paths:
        print(f"No JSON files found in {folder_path}")
        return []
    
    print(f"Found {len(file_paths)} JSON files")
    
    # Handle single file case
    if len(file_paths) <= 1:
        if show_progress:
            print("Processing single file...")
        return [nodes_to_int_array(file_paths[0])] if file_paths else []
    
    # Force single-threaded mode if max_workers is 0 or if environment variable is set
    use_single_thread = (max_workers == 0) or os.environ.get('FORCE_SINGLE_THREAD', 'false').lower() == 'true'
    
    if use_single_thread:
        print("Using SINGLE-THREADED mode (no multiprocessing)")
        results = []
        for i, file_path in enumerate(file_paths):
            try:
                results.append(nodes_to_int_array(file_path))
                if show_progress and ((i + 1) % 100 == 0 or (i + 1) == len(file_paths)):
                    progress_percent = (i + 1) / len(file_paths) * 100
                    print(f"Progress: {i + 1}/{len(file_paths)} ({progress_percent:.1f}%)")
            except Exception as e:
                print(f"Error processing file {file_path}: {e}")
                results.append([])
        print(f"Successfully processed {len(results)} files")
        return results
    
    # Determine number of workers for multiprocessing
    if max_workers is None:
        # Limit to 8 workers to avoid overwhelming the system
        max_workers = min(len(file_paths), os.cpu_count()) 
    
    print(f"Using {max_workers} worker processes")
    
    if show_progress:
        print("Starting parallel processing...")
        
        # Pre-allocate results list to maintain order
        results = [None] * len(file_paths)
        completed = 0
        
        with ProcessPoolExecutor(max_workers=max_workers) as executor:
            # Submit all tasks and track their indices
            future_to_index = {
                executor.submit(nodes_to_int_array, file_paths[i]): i 
                for i in range(len(file_paths))
            }
            
            # Collect results and show progress
            for future in as_completed(future_to_index):
                index = future_to_index[future]
                try:
                    results[index] = future.result()
                except Exception as e:
                    print(f"Error processing file {file_paths[index]}: {e}")
                    results[index] = []
                
                completed += 1
                
                # Display progress
                progress_percent = completed / len(file_paths) * 100
                
                if (completed%10 == 0) or (completed == len(file_paths)):
                    print(f"Progress: {completed}/{len(file_paths)} ({progress_percent:.1f}%)")
    else:
        # Process without progress display
        with ProcessPoolExecutor(max_workers=max_workers) as executor:
            results = list(executor.map(nodes_to_int_array, file_paths))
    
    print(f"Successfully processed {len(results)} files")
    return results