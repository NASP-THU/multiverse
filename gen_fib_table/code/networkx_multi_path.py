import networkx as nx

link_weight = 1

def generate_fattree(k, out_topo_file):
    num_hosts = k**3 // 4
    num_edge_switches = k * k // 2
    num_agg_switches = num_edge_switches
    num_core_switches = (k // 2)**2

    num_nodes = num_hosts + num_edge_switches + num_agg_switches + num_core_switches

    links = []

    # Add links between hosts and edge switches
    for i in range(num_hosts):
        links.append((i, num_hosts + i // (k // 2)))

    # Add links between edge switches and agg switches
    for i in range(num_edge_switches):
        for j in range(k // 2):
            links.append((num_hosts + i, num_hosts + num_edge_switches + i // (k // 2) * (k // 2) + j))

    # Add links between agg switches and core switches
    for i in range(num_agg_switches):
        for j in range(k // 2):
            links.append((num_hosts + num_edge_switches + i, num_hosts + num_edge_switches + num_agg_switches + i % (k // 2) * (k // 2) + j))

    # Write to file
    with open(out_topo_file, 'w') as f:
        f.write(f'{num_nodes} {len(links)}\n')
        for link in links:
            f.write(f'{link[0]} {link[1]} {link_weight}\n')


def read_topology_from_file(filename):
    topo = nx.MultiGraph()
    with open(filename, 'r') as f:
        num_nodes, num_links = map(int, f.readline().split())
        for line in f:
            src, dest, weight = map(int, line.split())
            topo.add_edge(src, dest, weight=weight)
    return topo


def calculate_all_shortest_paths(topo, num_hosts, output_filename):
    with open(output_filename, 'w') as f:
        path_list = []
        for i in range(num_hosts):
            for j in range(num_hosts):
                if i == j:
                    continue
                for path in nx.all_shortest_paths(topo, i, j, weight='weight'):
                    path_list.append(path)
                    f.write(f'{i} {j} {" ".join(map(str, path[1:-1]))}\n')
        return path_list


k_ary = 8

out_topo_file = f'fattree_{k_ary}.txt'
out_path_file = f'ecmp_paths_{k_ary}.txt'

generate_fattree(8, out_topo_file)

# Read topology from file
topo = read_topology_from_file(out_topo_file)

# Calculate all shortest paths between all pairs of hosts
path_list = calculate_all_shortest_paths(topo, k_ary**3 // 4, out_path_file)

print("path_list: ", path_list)