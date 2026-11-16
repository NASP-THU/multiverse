import numpy as np
import sys

def read_topology(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()
    num_nodes, num_links = map(int, lines[0].split())
    links = [tuple(map(int, line.split())) for line in lines[1:]]
    return num_nodes, links


def floyd_warshall(num_nodes, links):
    dist = np.full((num_nodes, num_nodes), np.inf)
    next_node = [[[] for _ in range(num_nodes)] for _ in range(num_nodes)]
    for i in range(num_nodes):
        dist[i, i] = 0
    for i, j in links:
        dist[i, j] = 1
        dist[j, i] = 1
        next_node[i][j] = [j]
        next_node[j][i] = [i]
    for k in range(num_nodes):
        for i in range(num_nodes):
            for j in range(num_nodes):
                if dist[i, k] + dist[k, j] < dist[i, j]:
                    dist[i, j] = dist[i, k] + dist[k, j]
                    next_node[i][j] = [k]
                elif dist[i, k] + dist[k, j] == dist[i, j]:
                    if k not in next_node[i][j]:
                        next_node[i][j].append(k)
    return dist, next_node

def reconstruct_path(i, j, next_node):
    if i == j:
        return [[i]]
    if not next_node[i][j]:
        return []
    paths = []
    for intermediate_node in next_node[i][j]:
        for path in reconstruct_path(intermediate_node, j, next_node):
            paths.append([i] + path)
    return paths




def write_paths(filename, num_hosts, dist, next_node):
    with open(filename, 'w') as f:
        for i in range(num_hosts):
            for j in range(num_hosts):
                if i == j:
                    continue
                paths = reconstruct_path(i, j, next_node)
                for path in paths:
                    f.write(f'{i} {j} {" ".join(map(str, path))}\n')


def main():
    num_nodes, links = read_topology('fattree.txt')
    k = 4 # Calculate k from the total number of nodes
    num_hosts = k**3 // 4  # Corrected host count for Fat-Tree topology
    dist, next_node = floyd_warshall(num_nodes, links)
    write_paths('ecmp_paths.txt', num_hosts, dist, next_node)

if __name__ == '__main__':
    main()
