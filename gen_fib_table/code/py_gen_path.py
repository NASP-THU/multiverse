import numpy as np


def in_array(array, element):
    return element in array

def generate_route_table(num_receiver, k_ary, path_file, fib_table):
    
    # fib_table.fill(-1)

    print("*******Generate_fib_table*******")
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
            next_hop_id = switch_id  # same

            next_hops = fib_table[switch_id][dst_host_id]
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

                next_hops = fib_table[switch_id][dst_host_id]
                if not in_array(next_hops, next_hop_id):
                    # Find the first -1 in the array and fill it with the next hop id
                    for j in range(k_ary):
                        if next_hops[j] == -1:
                            next_hops[j] = next_hop_id
                            break

    # After generating the FIB tables
    for i in range(num_edge_switch + num_aggr_switch + num_core_switch):
        print(f"Switch {i+num_receiver}:")
        for j in range(num_receiver):
            print(f"  Destination host {j}: ", end='')
            next_hops = fib_table[i][j]
            for k in range(k_ary):
                if next_hops[k] != -1:
                    print(f"{next_hops[k]} ", end='')
                else:
                    break
            print()


k_ary = 4
num_switch = 20  # k_ary*k_ary*5/4
num_host = 16  # k_ary*k_ary*k_ary/4

path_file = "ecmp_paths_4.txt"
fib_table = np.full((num_switch, num_host, k_ary), -1)

generate_route_table(k_ary*k_ary*k_ary//4, k_ary, path_file, fib_table)
