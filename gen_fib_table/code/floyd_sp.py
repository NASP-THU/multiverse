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
    next_node = np.zeros((num_nodes, num_nodes), dtype=int)
    for i in range(num_nodes):
        dist[i, i] = 0
    for i, j in links:
        dist[i, j] = 1
        dist[j, i] = 1
        next_node[i, j] = j
        next_node[j, i] = i
    for k in range(num_nodes):
        for i in range(num_nodes):
            for j in range(num_nodes):
                if dist[i, k] + dist[k, j] < dist[i, j]:
                    dist[i, j] = dist[i, k] + dist[k, j]
                    next_node[i, j] = next_node[i, k]
    return dist, next_node

def reconstruct_path(i, j, next_node):
    if next_node[i, j] == 0:
        return []
    path = [i]
    while i != j:
        i = next_node[i, j]
        path.append(i)
    return path

def write_paths(filename, num_hosts, dist, next_node):
    with open(filename, 'w') as f:
        for i in range(num_hosts):
            for j in range(i + 1, num_hosts):
                path = reconstruct_path(i, j, next_node)
                f.write(f'{i} {j} {" ".join(map(str, path))}\n')

def main():
    num_nodes, links = read_topology('fattree.txt')
    k = 4
    num_hosts = k**3 // 4  # Corrected host count for Fat-Tree topology
    dist, next_node = floyd_warshall(num_nodes, links)
    write_paths('paths.txt', num_hosts, dist, next_node)

if __name__ == '__main__':
    main()
