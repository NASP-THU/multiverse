import json
from dataclasses import dataclass, field
from typing import List, Optional, Union, Any

@dataclass
class BoolList:
    values: List[bool]

@dataclass
class Attribute:
    name: str
    boolVal: Optional[bool] = None
    uint32Val: Optional[int] = None
    boolList: Optional[BoolList] = None
    int64Val: Optional[int] = None
    uint64Val: Optional[int] = None

    def __post_init__(self):
        if isinstance(self.int64Val, str):
            try:
                self.int64Val = int(self.int64Val)
            except (ValueError, TypeError):
                print(f"Warning: Unable to convert int64Val '{self.int64Val}' to integer.")
                self.int64Val = None
        if isinstance(self.uint64Val, str):
            try:
                self.uint64Val = int(self.uint64Val)
            except (ValueError, TypeError):
                print(f"Warning: Unable to convert uint64Val '{self.uint64Val}' to integer.")
                self.uint64Val = None

@dataclass
class Node:
    name: str
    type: str
    durationMicros: int
    attr: List[Attribute]
    id: Optional[str] = None
    dataDeps: Optional[List[str]] = field(default_factory=list)

    def __post_init__(self):
        if isinstance(self.durationMicros, str):
            try:
                self.durationMicros = int(self.durationMicros)
            except (ValueError, TypeError):
                print(f"Warning: Unable to convert durationMicros '{self.durationMicros}' to integer.")
                self.durationMicros = 0

# --- Helper functions for parsing JSON ---
# (Keep the same definition as before)

def parse_attribute(attr_data: dict) -> Attribute:
    """Parse dictionary to Attribute object"""
    name = attr_data.get("name")
    bool_val = attr_data.get("boolVal")
    uint32_val = attr_data.get("uint32Val")
    int64_val = attr_data.get("int64Val")
    uint64_val = attr_data.get("uint64Val")
    bool_list_data = attr_data.get("boolList")

    bool_list_obj = None
    if bool_list_data and isinstance(bool_list_data, dict):
        values = bool_list_data.get("values")
        if isinstance(values, list):
            bool_list_obj = BoolList(values=values)

    return Attribute(
        name=name,
        boolVal=bool_val,
        uint32Val=uint32_val,
        boolList=bool_list_obj,
        int64Val=int64_val, # Pass to __post_init__ for processing
        uint64Val=uint64_val # Pass to __post_init__ for processing
    )

def parse_node(node_data: dict) -> Node:
    """Parse dictionary to Node object"""
    attributes = [parse_attribute(attr) for attr in node_data.get("attr", [])]

    return Node(
        id=node_data.get("id"),
        name=node_data.get("name"),
        type=node_data.get("type"),
        dataDeps=node_data.get("dataDeps"),
        durationMicros=node_data.get("durationMicros"), # Pass to __post_init__ for processing
        attr=attributes
    )

def parse_json_data(json_string: str) -> List[Node]:
    """Parse JSON string and return list of Node objects"""
    try:
        data = json.loads(json_string)
        if not isinstance(data, list):
            raise ValueError("JSON top-level structure is not a list")
        return [parse_node(node_data) for node_data in data]
    except json.JSONDecodeError as e:
        print(f"JSON parsing error: {e}")
        return []
    except Exception as e:
        print(f"Error occurred during parsing: {e}")
        return []

def test():
    # 1. Define JSON file path
    #    Please replace 'your_file.json' with your actual filename and path
    json_file_path = '/home/zhangran/madrona2/2/madrona_simple_example/scripts/input/npu.0.json'
    parsed_nodes = [] # Initialize as empty list

    # 2. Read file content and parse
    try:
        # Use 'with' statement to ensure file is properly closed
        with open(json_file_path, 'r', encoding='utf-8') as f:
            json_string_from_file = f.read()
            # Call the previous parsing function
            parsed_nodes = parse_json_data(json_string_from_file)

    except FileNotFoundError:
        print(f"Error: File '{json_file_path}' not found")
    except Exception as e:
        print(f"Error occurred while reading or parsing file: {e}")

    # 3. (If parsing successful) Print parsing results for verification
    if parsed_nodes:
        print(f"Successfully loaded and parsed {len(parsed_nodes)} nodes from '{json_file_path}'.\n")
        for i, node in enumerate(parsed_nodes):
            print(f"--- Node {i} ---")
            print(f"ID: {node.id}")
            print(f"Name: {node.name}")
            print(f"Type: {node.type}")
            print(f"Duration (micros): {node.durationMicros} (Type: {type(node.durationMicros)})")
            print(f"Data Dependencies: {node.dataDeps}")
            print("Attributes:")
            for attr in node.attr:
                value = None
                value_type = None
                if attr.boolVal is not None:
                    value = attr.boolVal
                    value_type = type(value)
                elif attr.uint32Val is not None:
                    value = attr.uint32Val
                    value_type = type(value)
                elif attr.int64Val is not None:
                    value = attr.int64Val
                    value_type = type(value) # Verify if it's int
                elif attr.uint64Val is not None:
                    value = attr.uint64Val
                    value_type = type(value) # Verify if it's int
                elif attr.boolList is not None:
                    value = attr.boolList.values
                    value_type = f"BoolList({type(value)})"

                print(f"  - Name: {attr.name}, Value: {value} (Type: {value_type})")
            print("-" * 20)

        # Example of accessing specific nodes and attributes
        if len(parsed_nodes) > 0:
            first_node = parsed_nodes[0]
            print(f"\nFirst node name: {first_node.name}")
            if len(first_node.attr) > 2 and first_node.attr[2].boolList:
                print(f"First node's third attribute boolList values: {first_node.attr[2].boolList.values}")
    else:
        print("Failed to successfully parse node data.")
                
def Parse(json_file_path):
    try:
        # Use 'with' statement to ensure file is properly closed
        with open(json_file_path, 'r', encoding='utf-8') as f:
            json_string_from_file = f.read()
            # Call the previous parsing function
            parsed_nodes = parse_json_data(json_string_from_file)
            return parsed_nodes
    except FileNotFoundError:
        print(f"Error: File '{json_file_path}' not found")
    except Exception as e:
        print(f"Error occurred while reading or parsing file: {e}")
        
# 2 int