link_weight = 1

def generate_fattree(k):
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
    with open('fattree_8.txt', 'w') as f:
        f.write(f'{num_nodes} {len(links)}\n')
        for link in links:
            f.write(f'{link[0]} {link[1]} {link_weight}\n')

generate_fattree(8)
