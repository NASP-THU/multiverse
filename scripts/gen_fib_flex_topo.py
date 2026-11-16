import heapq
from collections import defaultdict, deque
import copy



class NetworkGraph:
    def __init__(self, num_nodes, num_npus):
        self.num_nodes = num_nodes
        self.num_npus = num_npus
        self.graph = defaultdict(list)

    def add_link(self, u, v, w, bw, lat):
        self.graph[u].append((v, w, bw))
        self.graph[v].append((u, w, bw))

    def get_neighbors(self, node):
        return self.graph[node]

    def is_connected(self):
        visited = set()
        queue = deque([0])
        while queue:
            node = queue.popleft()
            if node not in visited:
                visited.add(node)
                for neighbor, _, _ in self.graph[node]:
                    queue.append(neighbor)
        return len(visited) == self.num_nodes
    
    # def is_connected(self, verbose=True):
    #     """
    #     检查图是否连通，如果不连通则输出不连通的部分
        
    #     Args:
    #         verbose: 是否输出详细的不连通信息
        
    #     Returns:
    #         tuple: (is_connected: bool, disconnected_components: list)
    #     """
    #     if self.num_nodes == 0:
    #         return True, []
        
    #     visited = set()
    #     queue = deque([0])
        
    #     # 从节点0开始BFS遍历
    #     while queue:
    #         node = queue.popleft()
    #         if node not in visited:
    #             visited.add(node)
    #             for neighbor, _, _ in self.graph[node]:
    #                 if neighbor not in visited:
    #                     queue.append(neighbor)
        
    #     # 检查是否连通
    #     is_fully_connected = len(visited) == self.num_nodes
        
    #     if is_fully_connected:
    #         return True, []
        
    #     # 如果不连通，找出所有不连通的组件
    #     disconnected_components = []
    #     unvisited = set(range(self.num_nodes)) - visited
        
    #     while unvisited:
    #         # 找出每个不连通组件
    #         component = set()
    #         start_node = next(iter(unvisited))
    #         component_queue = deque([start_node])
            
    #         while component_queue:
    #             node = component_queue.popleft()
    #             if node not in component:
    #                 component.add(node)
    #                 unvisited.discard(node)
    #                 for neighbor, _, _ in self.graph[node]:
    #                     if neighbor in unvisited:
    #                         component_queue.append(neighbor)
            
    #         disconnected_components.append(sorted(list(component)))
        
    #     if verbose:
    #         print(f"图不连通！发现 {len(disconnected_components) + 1} 个连通分量：")
    #         print(f"主要连通分量 (从节点0可达): {sorted(list(visited))}")
    #         for i, component in enumerate(disconnected_components):
    #             print(f"不连通分量 {i+1}: {component}")
            
    #         # 输出每个不连通分量的连接信息
    #         for i, component in enumerate(disconnected_components):
    #             print(f"\n不连通分量 {i+1} 的内部连接:")
    #             for node in component:
    #                 neighbors = [neighbor for neighbor, _, _ in self.graph[node]]
    #                 if neighbors:
    #                     print(f"  节点 {node} 连接到: {neighbors}")
    #                 else:
    #                     print(f"  节点 {node} 没有连接")
        
    #     return False, disconnected_components


def dijkstra_ecmp(graph, src):
    dist = [float('inf')] * graph.num_nodes
    prev_hops = [set() for _ in range(graph.num_nodes)]
    dist[src] = 0
    heap = [(0, src)]

    while heap:
        current_dist, u = heapq.heappop(heap)
        if current_dist > dist[u]:
            continue
        for v, weight, _ in graph.get_neighbors(u):
            path_cost = dist[u] + weight
            if path_cost < dist[v]:
                dist[v] = path_cost
                prev_hops[v] = {u}
                heapq.heappush(heap, (dist[v], v))
            elif path_cost == dist[v]:
                prev_hops[v].add(u)

    # Build next hops based on predecessor nodes
    next_hops = defaultdict(set)
    for dst in range(graph.num_nodes):
        if dst == src:
            continue
        queue = deque([(dst, None)])
        visited = set()
        while queue:
            node, via = queue.popleft()
            for prev in prev_hops[node]:
                if prev == src:
                    next_hops[dst].add(node)
                elif prev not in visited:
                    visited.add(prev)
                    queue.append((prev, node))
    return next_hops

def build_global_routing_table(graph, port_mapping):
    if not graph.is_connected():
        raise ValueError("Network is disconnected. Cannot build full routing table.")
    # is_connected_result, disconnected_components = graph.is_connected()
    # if not is_connected_result:
    #     error_msg = f"网络不连通，无法构建完整的路由表。发现 {len(disconnected_components)} 个不连通分量"
    #     for i, component in enumerate(disconnected_components):
    #         error_msg += f"\n不连通分量 {i+1}: {component}"
    #     raise ValueError(error_msg)

    global_table = []
    for src in range(graph.num_nodes):
        if src not in port_mapping:
            global_table.append([])
            continue
        routing_table = []
        next_hops = dijkstra_ecmp(graph, src)
        for dst in range(graph.num_nodes):
            if dst in next_hops:
                hops = sorted(
                    [port_mapping[src]['local'][nhop] for nhop in next_hops[dst] if nhop in port_mapping[src]['local']]
                )
            else:
                hops = []
            routing_table.append((dst, hops))
        global_table.append(routing_table)
    return global_table

def build_global_routing_table_with_array(graph, port_mapping):
    global_table = build_global_routing_table(graph, port_mapping)
    num_nodes = graph.num_nodes
    max_destinations = graph.num_nodes
    max_next_hops = max(len(entry[1]) for table in global_table for entry in table)

    global_routing_table_array = [[[-1 for _ in range(max_next_hops)] 
                                   for _ in range(max_destinations)] 
                                  for _ in range(num_nodes)]

    for src, table_entries in enumerate(global_table):
        for dst, (destination, next_hops) in enumerate(table_entries):
            if dst < len(global_routing_table_array[src]):
                for idx, hop in enumerate(next_hops):
                    if idx < len(global_routing_table_array[src][dst]):
                        global_routing_table_array[src][dst][idx] = hop

    next_hop_num = [[0 for _ in range(max_destinations)] for _ in range(num_nodes)]

    for src, table_entries in enumerate(global_table):
        for dst, (destination, next_hops) in enumerate(table_entries):
            if dst < len(global_routing_table_array[src]):
                for idx, hop in enumerate(next_hops):
                    if idx < len(global_routing_table_array[src][dst]):
                        global_routing_table_array[src][dst][idx] = hop
                next_hop_num[src][dst] = sum(1 for hop in global_routing_table_array[src][dst] if hop >= 0)

    return global_table, global_routing_table_array, next_hop_num

def parse_topology_with_ports(file_path):
    with open(file_path, 'r') as f:
        lines = f.readlines()
    
    parts = lines[0].strip().split()
    num_nodes = int(parts[0])
    sw_num = int(parts[3])
    num_links = int(parts[4])
    
    switch_ids = list(map(int, lines[1].strip().split()))
    
    graph = NetworkGraph(num_nodes, num_nodes-sw_num)
    aj_link = []
    link_set = set()
    
    for line in lines[2:]:
        parts = line.strip().split()
        if parts[6] == "ETH" or parts[6] == "IB":
            u = int(parts[0])
            v = int(parts[1])
            w = int(parts[5])
            bw = int(parts[2].replace("Gbps", ""))
            lat = int(parts[3].replace("ns", ""))
            graph.add_link(u, v, w, bw, lat)
            if (u, v) not in link_set:
                aj_link.append([u, v, w, bw, lat])
                link_set.add((u, v))
            if (v, u) not in link_set:
                aj_link.append([v, u, w, bw, lat])
                link_set.add((v, u))
    
    aj_link = sorted(aj_link, key=lambda x: (x[0], x[1]))
    
    port_mapping = {}
    global_port_counter = 0
    for node in range(num_nodes):
        neighbors = graph.get_neighbors(node)
        sorted_neighbors = sorted(neighbors, key=lambda x: x[0])
        local_ports = {neighbor[0]: idx for idx, neighbor in enumerate(sorted_neighbors)}
        global_ports = {}
        for neighbor in local_ports:
            global_ports[neighbor] = global_port_counter
            global_port_counter += 1
        if node == (num_nodes - sw_num - 1):
            global_port_counter = 0
        port_mapping[node] = {
            'local': local_ports,
            'global': global_ports
        }
    
    # --- New logic: create a separate_port_mapping for switches with unique global port IDs ---
    # 1. Find max global_port_counter among all net_npu (non-switch) nodes
    max_net_npu_port = 0
    for node in range(num_nodes):
        if node not in switch_ids:
            max_net_npu_port = max(max_net_npu_port, max(port_mapping[node]['global'].values(), default=-1))
    start_switch_port_id = max_net_npu_port + 1

    # 2. Make a deep copy for separate_port_mapping
    separate_port_mapping = copy.deepcopy(port_mapping)
    switch_global_port_counter = start_switch_port_id
    for node in range(num_nodes):
        if node in switch_ids:
            sorted_neighbors = sorted(graph.get_neighbors(node), key=lambda x: x[0])
            for neighbor in separate_port_mapping[node]['global']:
                separate_port_mapping[node]['global'][neighbor] = switch_global_port_counter
                switch_global_port_counter += 1

    return graph, port_mapping, separate_port_mapping, switch_ids, aj_link, num_nodes, sw_num

def print_port_mappings(port_mapping, label=""):
    if label:
        print(f"==== {label} ====")
    for switch, ports in port_mapping.items():
        print(f"Node {switch}:")
        print(f"  Local Ports: {ports['local']}")
        print(f"  Global Ports: {ports['global']}")
        print("")

def print_routing_table(table):
    for src, table_entries in enumerate(table):
        print(f"Node {src} Routing Table:")
        for dst, next_hops in table_entries:
            print(f"  -> Destination {dst}: Next Hops (ports) = {next_hops}")
        print("")

def generate_port_connection_lists(port_mapping, graph, switch_ids, use_separate=False, separate_port_mapping=None):
    """
    If use_separate is True, use separate_port_mapping for switches' global port IDs.
    """
    # Use the correct mapping for global port range calculations
    if use_separate and separate_port_mapping is not None:
        max_global_port = 0
        for node, ports in port_mapping.items():
            if node in switch_ids:
                max_global_port = max(max_global_port, max(separate_port_mapping[node]['global'].values(), default=-1))
            else:
                max_global_port = max(max_global_port, max(ports['global'].values(), default=-1))
    else:
        max_global_port = max(max(ports['global'].values()) for ports in port_mapping.values())
    switch_next_hop_port = [-1] * (max_global_port + 1)
    net_npu_next_hop_port = [-1] * (max_global_port + 1)
    
    for node, ports in port_mapping.items():
        is_switch = node in switch_ids
        for neighbor, local_port in ports['local'].items():
            # Use separate global port mapping for switches
            if use_separate and separate_port_mapping is not None and is_switch:
                global_port = separate_port_mapping[node]['global'][neighbor]
                neighbor_global_port = (separate_port_mapping[neighbor]['global'][node]
                                        if neighbor in switch_ids
                                        else port_mapping[neighbor]['global'][node])
            else:
                global_port = ports['global'][neighbor]
                neighbor_global_port = port_mapping[neighbor]['global'][node]
            if is_switch:
                switch_next_hop_port[global_port] = neighbor_global_port
            else:
                net_npu_next_hop_port[global_port] = neighbor_global_port

    net_npu_port_count = sum(1 for port in net_npu_next_hop_port if port >= 0)
    switch_port_count = sum(1 for port in switch_next_hop_port if port >= 0)
    
    return switch_next_hop_port, net_npu_next_hop_port, switch_port_count, net_npu_port_count

def generate_node_port_counts(port_mapping):
    return [len(ports['local']) for node, ports in sorted(port_mapping.items())]

def parse_topo(topology_file):
    graph, port_mapping, separate_port_mapping, switch_ids, aj_link, num_node, sw_num  = parse_topology_with_ports(topology_file)
    port_num = generate_node_port_counts(port_mapping)
    switch_next_hop_port, net_npu_next_hop_port, switch_port_num, net_npu_port_num = generate_port_connection_lists(
        port_mapping, graph, switch_ids, use_separate=True, separate_port_mapping=separate_port_mapping
    )
    link_num = len(aj_link)
    net_npu_num = num_node-sw_num 
    return aj_link, link_num, net_npu_num, sw_num, port_num, switch_next_hop_port, net_npu_next_hop_port, switch_port_num, net_npu_port_num, graph, port_mapping, separate_port_mapping

if __name__ == "__main__":
    topology_file = "./fattree_4_16g_2gps_100Gbps_H100_no_scale_up"
    
    graph, port_mapping, separate_port_mapping, switch_ids, aj_link, num_node, sw_num = parse_topology_with_ports(topology_file)
    print_port_mappings(port_mapping, label="Original port_mapping")
    print_port_mappings(separate_port_mapping, label="Separate Switch port_mapping")
    
    node_port_counts = generate_node_port_counts(port_mapping)
    print("Node Port Counts:", node_port_counts)
    
    # Use the new mapping for correct switch port ID separation
    switch_next_hop_port, net_npu_next_hop_port, switch_port_num, net_npu_port_num = generate_port_connection_lists(
        port_mapping, graph, switch_ids, use_separate=True, separate_port_mapping=separate_port_mapping
    )
    print("Switch Port Connections:", switch_next_hop_port)
    print("net_npu Port Connections:", net_npu_next_hop_port)
    print("switch Port Count:", switch_port_num)
    print("net_npu Port Count:", net_npu_port_num)
    
    global_routing_table, global_routing_table_array, _ = build_global_routing_table_with_array(graph, port_mapping)
    
    node_idx = 0
    for routing_table_array in global_routing_table_array:
        print("node: ", node_idx)
        node_idx += 1 
        for item in routing_table_array:
            print(item)