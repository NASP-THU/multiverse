import networkx as nx
import numpy as np


link_weight = 1

def gen_fattree(k, out_topo_file):
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


def in_array(array, element):
    return element in array

def gen_route_table(num_receiver, k_ary, path_file, route_table):
    # route_table.fill(-1)

    print("*******Generate_route_table*******")
    num_edge_switch = k_ary*k_ary//2
    num_aggr_switch = k_ary*k_ary//2  # the number of aggregate switch
    num_core_switch = k_ary*k_ary//4

    # Read the paths from the file
    try:
        with open(path_file, 'r') as file:
            lines = file.readlines()
    except IOError:
        print("Failed to open the file.")
        return

    for line in lines:
        path = list(map(int, line.strip().split()))
        if len(path) <= 3:  # dst node locate in the same rack with the current switch 
            switch_id = path[2]-num_receiver
            dst_host_id = path[1]
            next_hop_id = switch_id+num_receiver  # same

            next_hops = route_table[switch_id][dst_host_id]
            if not in_array(next_hops, next_hop_id):
                # Find the first -1 in the array and fill it with the next hop id
                for j in range(k_ary):
                    if next_hops[j] == -1:
                        next_hops[j] = next_hop_id
                        break
        else:
            for i in range(2, len(path)-1):  # Skip the first and last nodes (hosts)
                switch_id = path[i]-num_receiver
                dst_host_id = path[1]
                next_hop_id = path[i + 1]  # -num_receiver

                next_hops = route_table[switch_id][dst_host_id]
                if not in_array(next_hops, next_hop_id):
                    # Find the first -1 in the array and fill it with the next hop id
                    for j in range(k_ary):
                        if next_hops[j] == -1:
                            next_hops[j] = next_hop_id
                            break

    # After generating the route tables
    #for i in range(num_edge_switch+num_aggr_switch+num_core_switch):
    for i in range(1):
        print(f"Switch {i+num_receiver}:")
        for j in range(num_receiver):
            print(f"  Destination host {j}: ", end='')
            next_hops = route_table[i][j]
            for k in range(k_ary):
                if next_hops[k] != -1:
                    print(f"{next_hops[k]} ", end='')
                else:
                    break
            print()


def read_topology_as_adj_matrix(filename):
    with open(filename, 'r') as f:
        num_nodes, num_links = map(int, f.readline().split())
        adj_matrix = [[0 for i in range(num_nodes)] for k in range(num_nodes)]
        for line in f:
            src, dst, weight = map(int, line.split())
            adj_matrix[src][dst] = 1
            adj_matrix[dst][src] = 1
            #print("adj_matrix: \n", adj_matrix)
            
    return adj_matrix

# find the egress port/link(in the current switch)  for the next hop
def calc_next_hop_e_port(adj_matrix, switch_id, next_hop_node_id):
    next_hop_link_id = -1 #start from 0
    switch_idx = switch_id
    for i in range(next_hop_node_id+1):
        if adj_matrix[switch_idx][i] > 0:
            next_hop_link_id += 1
    if next_hop_link_id == -1:
        print(switch_idx, "-->", next_hop_node_id, " ", adj_matrix[switch_idx], "\n")
        raise ValueError("next_hop_link_id cannot be zero")
    else:
        return next_hop_link_id

# # find the ingress port/link(in the directed_connected next hop switch) for the next hop 
# def calc_next_hop_in_port(adj_matrix, switch_id, next_hop_node_id):
#     next_hop_link_id = -1 #start from 0
#     switch_idx = next_hop_node_id # stand from the view of the next hop switch
#     for i in range(switch_id+1):
#         if adj_matrix[switch_idx][i] > 0:
#             next_hop_link_id += 1
#     if next_hop_link_id == -1:
#         print(switch_idx, "-->", next_hop_node_id, " ", adj_matrix[switch_idx], "\n")
#         raise ValueError("next_hop_link_id cannot be zero")
#     else:
#         return next_hop_link_id


def gen_fib_table(num_host, num_switch, k_ary, adj_matrix, route_table, fib_table):
    print("*******Generate_fib_table*******")
    
    for switch_id in range(num_switch):
        for dst_host_id in range(num_host):
            next_hop_node_id = route_table[switch_id][dst_host_id]
            next_hop_link_id = fib_table[switch_id][dst_host_id]
            for j in range(k_ary):
                if next_hop_node_id[j] != -1:
                    # (the actual idx)(start from the last host id) defined in topo file
                    true_id = next_hop_node_id[j]  
                    next_hop_link_id[j] = calc_next_hop_e_port(adj_matrix, switch_id+num_host, true_id)
            
            #print("next_hop_link_id: ", next_hop_link_id)

    # # After generating the FIB tables
    # for i in range(num_switch):
    #     print(f"Switch {i+num_host}:")
    #     for j in range(num_host):
    #         print(f"  Dst host {j}: ", end='')
    #         next_hop_link_id = fib_table[i][j]
    #         for k in range(k_ary):
    #             if next_hop_link_id[k] != -1:
    #                 print(f"{next_hop_link_id[k]} ", end='')
    #             else:
    #                 break
    #         print()


def gen_next_hop_table(num_host, num_switch, adj_matrix, next_hop_table_host_to_sw, next_hop_table):
    print("*******Generate_next_hop_table*******")
    
    link_num_list = []
    cnt = 0
    for i in range(num_host+num_switch):
        for k in range(num_host+num_switch):
            if adj_matrix[i][k] == 1:
                cnt += 1
        link_num_list.append(cnt)
    print("link_num_list: \n", link_num_list)
    
    for i in range(num_host+num_switch):
        for k in range(num_host+num_switch):
            if adj_matrix[i][k] == 0: #not connected link
                continue            
            
            if i < num_host:
                #a_link_idx
                local_idx = -1
                for j in range(k+1): #
                    if adj_matrix[i][j] > 0:
                        local_idx += 1 #
                a_link_idx = 0
                if i == 0:
                    a_link_idx = local_idx
                else:
                    a_link_idx = link_num_list[i-1] + local_idx

                #b_link_idx
                local_idx = -1
                for j in range(i+1):
                    if adj_matrix[k][j] > 0:
                        local_idx += 1
                        
                b_link_idx = 0
                if k == 0:
                    b_link_idx = local_idx
                elif 0 < k < num_host:
                    b_link_idx = link_num_list[k-1] + local_idx
                else:
                    b_link_idx = link_num_list[k-1] - link_num_list[num_host-1] + local_idx
                #print("link_num_list[k-1]: ", link_num_list[k-1], ", link_num_list[num_host]: ", link_num_list[num_host-1], ", local_idx: ", local_idx)
                
                
                #print("a_link_idx-->b_link_idx: ", a_link_idx, "-->", b_link_idx)
                next_hop_table_host_to_sw[a_link_idx] = b_link_idx   
                  
            elif i >= num_host and k < num_host:
                #a_link_idx
                local_idx = -1
                for j in range(k+1):
                    if adj_matrix[i][j] > 0:
                        local_idx += 1
                a_link_idx = 0
                if i == 0:
                    a_link_idx = local_idx
                elif 0 < i < num_host:
                    b_link_idx = link_num_list[i-1] + local_idx
                else:
                    a_link_idx = link_num_list[i-1] - link_num_list[num_host-1] + local_idx

                #b_link_idx                
                local_idx = -1
                for j in range(i+1):
                    if adj_matrix[k][j] > 0:
                        local_idx += 1
                b_link_idx = 0
                if k == 0:
                    b_link_idx = local_idx
                else:
                    b_link_idx = link_num_list[k-1] + local_idx
                
                next_hop_table[a_link_idx] = b_link_idx  

            elif i >= num_host and k >= num_host:
                #a_link_idx
                local_idx = -1
                for j in range(k+1):
                    if adj_matrix[i][j] > 0:
                        local_idx += 1
                a_link_idx = 0
                if i == 0:
                    a_link_idx = local_idx
                else:
                    a_link_idx = link_num_list[i-1] - link_num_list[num_host-1] + local_idx

                #b_link_idx
                local_idx = -1
                for j in range(i+1):
                    if adj_matrix[k][j] > 0:
                        local_idx += 1
                b_link_idx = 0
                if k == 0:
                    b_link_idx = local_idx
                else:
                    b_link_idx = link_num_list[k-1] - link_num_list[num_host-1] + local_idx
                
                next_hop_table[a_link_idx] = b_link_idx  


def list_to_c_array_3d(py_list):
    c_array = "{\n"
    for sub_list_2d in py_list:
        c_array += "{\n"
        for sub_list_1d in sub_list_2d:
            c_array += "{"
            c_array += ", ".join(map(str, sub_list_1d))
            c_array += "},\n"
        c_array += "},\n"
    c_array += "};"
    return c_array


def list_to_c_array_1d(py_list):
    c_array = "{"
    for i in range(len(py_list)):
        if i < len(py_list)-1:
            c_array += str(py_list[i]) + ", "
        else:
            c_array += str(py_list[i])
    c_array += "};\n"    
    return c_array


def replace_cpp_array(file_path, c_array_fib_table, c_array_next_hop_table_host_to_sw, c_array_next_hop_table):
    with open(file_path, 'r') as file:
        cpp_code = file.read()

    # Find the line to replace
    start = cpp_code.find('const int32_t central_fib_table[num_switch][num_host][K_ARY] =')
    end = cpp_code.find(';', start)
    old_line = cpp_code[start:end+1]
    # Create the new line
    new_line = 'const int32_t central_fib_table[num_switch][num_host][K_ARY] = ' + c_array_fib_table
    # Replace the old line with the new line
    cpp_code = cpp_code.replace(old_line, new_line)



    # Find the line to replace
    start = cpp_code.find('const int32_t next_hop_table_host_to_sw[num_host] =')
    end = cpp_code.find(';', start)
    old_line = cpp_code[start:end+1]
    # Create the new line
    new_line = 'const int32_t next_hop_table_host_to_sw[num_host] = ' + c_array_next_hop_table_host_to_sw
    # Replace the old line with the new line
    cpp_code = cpp_code.replace(old_line, new_line)

    # Find the line to replace
    start = cpp_code.find('const int32_t next_hop_table[num_switch*K_ARY] =')
    end = cpp_code.find(';', start)
    old_line = cpp_code[start:end+1]
    # Create the new line
    new_line = 'const int32_t next_hop_table[num_switch*K_ARY] = ' + c_array_next_hop_table
    # Replace the old line with the new line
    cpp_code = cpp_code.replace(old_line, new_line)


    return cpp_code


if __name__=="__main__":
    k_ary = 16
    # num_switch = 80
    # num_host = 128 #k_ary*k_ary*k_ary/4
    num_switch = int(k_ary*k_ary*5/4)
    num_host = int(k_ary*k_ary*k_ary/4)

    topo_file = f'fattree_{k_ary}.txt'
    path_file = f'ecmp_paths_{k_ary}.txt'

    gen_fattree(k_ary, topo_file)

    # Read topology from file
    topo = read_topology_from_file(topo_file)
    # Calculate all shortest paths between all pairs of hosts
    path_list = calculate_all_shortest_paths(topo, k_ary**3 // 4, path_file)
    print("path_list: ", path_list)

    route_table = np.full((num_switch, num_host, k_ary), -1)
    gen_route_table(k_ary*k_ary*k_ary//4, k_ary, path_file, route_table)


    fib_table = np.full((num_switch, num_host, k_ary), -1)
    adj_matrix = read_topology_as_adj_matrix(topo_file)
    gen_fib_table(num_host, num_switch, k_ary, adj_matrix, route_table, fib_table)

    next_hop_table_host_to_sw = np.full((num_host), -1)
    next_hop_table = np.full((num_switch*k_ary), -1)
    gen_next_hop_table(num_host, num_switch, adj_matrix, next_hop_table_host_to_sw, next_hop_table)
    print("next_hop_table_host_to_sw: \n", next_hop_table_host_to_sw.tolist())
    print("next_hop_table: \n", next_hop_table.tolist())


    c_array_fib_table = list_to_c_array_3d(fib_table.tolist())
    print("c_array: \n", c_array_fib_table)
 

    c_array_next_hop_table_host_to_sw = list_to_c_array_1d(next_hop_table_host_to_sw.tolist())
    print("c_array_1d: ", c_array_next_hop_table_host_to_sw)

    c_array_next_hop_table = list_to_c_array_1d(next_hop_table.tolist())
    print("c_array_1d: ", c_array_next_hop_table)
    
    # Replace the array in the C++ code
    new_cpp_code = replace_cpp_array('../build_topo.cpp', c_array_fib_table, c_array_next_hop_table_host_to_sw, c_array_next_hop_table)

    # Write the new C++ code to the file
    with open('build_topo.cpp', 'w') as file:
        file.write(new_cpp_code)
