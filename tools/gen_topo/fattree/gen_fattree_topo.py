# link_weight = 1

# def generate_fattree(k):
#     num_hosts = k**3 // 4
#     num_edge_switches = k * k // 2
#     num_agg_switches = num_edge_switches
#     num_core_switches = (k // 2)**2

#     num_nodes = num_hosts + num_edge_switches + num_agg_switches + num_core_switches
#     num_sws = num_edge_switches + num_agg_switches + num_core_switches
    
#     links = []



#     # Add links among gpu 
    

#     # Add links between hosts and edge switches
#     for i in range(num_hosts):
#         links.append((i, num_hosts + i // (k // 2)))

#     # Add links between edge switches and agg switches
#     for i in range(num_edge_switches):
#         for j in range(k // 2):
#             links.append((num_hosts + i, num_hosts + num_edge_switches + i // (k // 2) * (k // 2) + j))

#     # Add links between agg switches and core switches
#     for i in range(num_agg_switches):
#         for j in range(k // 2):
#             links.append((num_hosts + num_edge_switches + i, num_hosts + num_edge_switches + num_agg_switches + i % (k // 2) * (k // 2) + j))

#     # Write to file
#     with open('fattree_8_test.txt', 'w') as f:
#         f.write(f'{num_nodes} {num_sws} {len(links)}\n')
        
#         for i in range(num_sws):
#             f.write(f'{num_hosts + i} ')
        
#         f.write(f'\n')
        
#         for link in links:
#             f.write(f'{link[0]} {link[1]} 100Gbps 1000ns 0.000000\n')


# generate_fattree(8)


import argparse

link_weight = 1

def generate_fattree_scale_up(k, gpu_num, gpu_num_per_host, sw_bw, nvl_bw, gpu_type):
    # num_hosts = k**3 // 4
    num_nvs_switches = int(gpu_num / gpu_num_per_host)
    print("gpu_num_per_host: ", gpu_num_per_host)
    print("num_nvs_switches: ", num_nvs_switches)
    
    num_edge_switches = k * k // 2
    num_agg_switches = num_edge_switches
    num_core_switches = (k // 2)**2

    num_nodes = gpu_num + num_nvs_switches + num_edge_switches + num_agg_switches + num_core_switches
    num_sws = num_nvs_switches + num_edge_switches + num_agg_switches + num_core_switches
    
    links = []

    # Add links among gpu 
    for i in range(gpu_num):
        links.append((i, int(gpu_num+i/gpu_num_per_host)))

    nvl_num = len(links)
    print("nvl_num: ", nvl_num)


    # Add links between hosts and edge switches
    for i in range(gpu_num):
        links.append((i, gpu_num + num_nvs_switches +  i // (k // 2)))

    # Add links between edge switches and agg switches
    for i in range(num_edge_switches):
        for j in range(k // 2):
            links.append((gpu_num + num_nvs_switches + i, gpu_num + num_nvs_switches + num_edge_switches + i // (k // 2) * (k // 2) + j))

    # Add links between agg switches and core switches
    for i in range(num_agg_switches):
        for j in range(k // 2):
            links.append((gpu_num + num_nvs_switches + num_edge_switches + i, gpu_num + num_nvs_switches + num_edge_switches + num_agg_switches + i % (k // 2) * (k // 2) + j))

    # Write to file
    with open(f'fattree_{K}_{gpu_num}g_{gpu_num_per_host}gps_{sw_bw}_{gpu_type}', 'w') as f:
        f.write(f'{num_nodes} {gpu_num_per_host} {num_nvs_switches} {num_sws} {len(links)} {gpu_type} {gpu_num}\n')
        
        for i in range(num_sws):
            f.write(f'{gpu_num + i} ')
        
        f.write(f'\n')

        for idx in range(nvl_num):
            f.write(f'{links[idx][0]} {links[idx][1]} {nvl_bw} {delay} {loss_rate} {nvl_weight} {"NVL"}\n')
        
        for idx in range(nvl_num, len(links)):
            f.write(f'{links[idx][0]} {links[idx][1]} {sw_bw} {delay} {loss_rate} {eth_weiht} {"ETH"}\n')



def generate_fattree(k, gpu_num, gpu_num_per_host, sw_bw, nvl_bw, gpu_type):
    # num_hosts = k**3 // 4
    # num_nvs_switches = int(gpu_num / gpu_num_per_host)
    num_nvs_switches = 0
    print("gpu_num_per_host: ", gpu_num_per_host)
    print("num_nvs_switches: ", num_nvs_switches)
    
    num_edge_switches = k * k // 2
    num_agg_switches = num_edge_switches
    num_core_switches = (k // 2)**2

    num_nodes = gpu_num + num_nvs_switches + num_edge_switches + num_agg_switches + num_core_switches
    num_sws = num_nvs_switches + num_edge_switches + num_agg_switches + num_core_switches
    
    links = []

    # Add links among gpu 
    # for i in range(gpu_num):
    #     links.append((i, int(gpu_num+i/gpu_num_per_host)))

    nvl_num = len(links)
    print("nvl_num: ", nvl_num)


    # Add links between hosts and edge switches
    for i in range(gpu_num):
        links.append((i, gpu_num + num_nvs_switches +  i // (k // 2)))

    # Add links between edge switches and agg switches
    for i in range(num_edge_switches):
        for j in range(k // 2):
            links.append((gpu_num + num_nvs_switches + i, gpu_num + num_nvs_switches + num_edge_switches + i // (k // 2) * (k // 2) + j))

    # Add links between agg switches and core switches
    for i in range(num_agg_switches):
        for j in range(k // 2):
            links.append((gpu_num + num_nvs_switches + num_edge_switches + i, gpu_num + num_nvs_switches + num_edge_switches + num_agg_switches + i % (k // 2) * (k // 2) + j))

    # Write to file
    with open(f'fattree_{K}_{gpu_num}g_{gpu_num_per_host}gps_{sw_bw}_{gpu_type}_no_scale_up', 'w') as f:
        f.write(f'{num_nodes} {gpu_num_per_host} {num_nvs_switches} {num_sws} {len(links)} {gpu_type} {gpu_num}\n')
        
        for i in range(num_sws):
            f.write(f'{gpu_num + i} ')
        
        f.write(f'\n')

        for idx in range(nvl_num):
            f.write(f'{links[idx][0]} {links[idx][1]} {nvl_bw} {delay} {loss_rate} {nvl_weight} {"NVL"}\n')
        
        for idx in range(nvl_num, len(links)):
            f.write(f'{links[idx][0]} {links[idx][1]} {sw_bw} {delay} {loss_rate} {eth_weiht} {"ETH"}\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate FatTree topology.")
    parser.add_argument("--K", type=int, required=True, help="FatTree parameter K")
    args = parser.parse_args()
    K = args.K
    gpu_num = K**3 // 4
    gpu_num_per_host = int(K/2)
    nvl_bw = "2880Gbps"

    sw_bw = "400Gbps"

    delay = "1000ns"
    loss_rate = "0.000"
    nvl_weight = 1
    eth_weiht = 100
    
    gpu_type = "H100"



    generate_fattree(K, gpu_num, gpu_num_per_host, sw_bw, nvl_bw, gpu_type)
    
    
