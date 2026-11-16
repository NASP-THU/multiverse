#!/usr/bin/env python3
"""
拓扑生成脚本 V3
线性拓扑，偶数位置的交换机拆分
"""

import sys
import argparse
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch


def generate_topology(diameter, num_gpus, output_file, link_bandwidth=100, link_latency=1000):
    """
    生成拓扑文件
    偶数位置的交换机（第2、4、6...个）拆分为 GPU数/2 个
    
    Args:
        diameter: 网络直径（交换机层级数）
        num_gpus: GPU节点总数
        output_file: 输出文件路径
        link_bandwidth: 链路带宽 (Gbps)
        link_latency: 链路延迟 (ns)
    """
    if num_gpus % 2 != 0:
        print(f"警告: GPU节点总数({num_gpus})不是2的倍数")
    
    gpus_per_split = 2  # 每个拆分交换机连接的GPU数
    num_split_switches = num_gpus // 2  # 拆分出的交换机数量
    
    # 构建交换机层级信息
    # 索引从0开始，偶数位置（索引1,3,5...）的交换机拆分
    switch_layers = []
    switch_id_counter = num_gpus
    
    for layer_idx in range(diameter):
        if layer_idx % 2 == 1:  # 第2、4、6...个（索引1、3、5...）
            # 拆分
            num_switches = num_split_switches
        else:
            # 不拆分
            num_switches = 1
        
        switch_ids = list(range(switch_id_counter, switch_id_counter + num_switches))
        switch_layers.append({
            'index': layer_idx,
            'split': (layer_idx % 2 == 1),
            'num_switches': num_switches,
            'switch_ids': switch_ids
        })
        switch_id_counter += num_switches
    
    # 所有GPU和交换机ID
    gpu_ids = list(range(num_gpus))
    all_switch_ids = []
    for layer in switch_layers:
        all_switch_ids.extend(layer['switch_ids'])
    
    # 生成链路
    links = []
    link_id = 0
    
    # 1. 首交换机（第1个，索引0）连接前一半GPU
    first_layer = switch_layers[0]
    if first_layer['split']:
        print("错误：第1个交换机不应该拆分")
        return
    
    first_switch = first_layer['switch_ids'][0]
    gpus_per_first = num_gpus // 2
    
    for i in range(gpus_per_first):
        gpu_id = gpu_ids[i]
        # 双向链路
        links.append({'id': link_id, 'src': gpu_id, 'dst': first_switch, 
                     'bandwidth': link_bandwidth, 'latency': link_latency})
        link_id += 1
        # links.append({'id': link_id, 'src': first_switch, 'dst': gpu_id,
        #              'bandwidth': link_bandwidth, 'latency': link_latency})
        # link_id += 1
    
    # 2. 尾交换机（最后一个）连接后一半GPU
    last_layer = switch_layers[-1]
    last_switches = last_layer['switch_ids']
    
    if last_layer['split']:
        # 直径为偶数：最后一个交换机拆分了，每个拆分交换机只连1个GPU
        for i, last_switch in enumerate(last_switches):
            if gpus_per_first + i < num_gpus:
                gpu_id = gpu_ids[gpus_per_first + i]
                # 双向链路
                links.append({'id': link_id, 'src': gpu_id, 'dst': last_switch,
                             'bandwidth': link_bandwidth, 'latency': link_latency})
                link_id += 1
                # links.append({'id': link_id, 'src': last_switch, 'dst': gpu_id,
                #              'bandwidth': link_bandwidth, 'latency': link_latency})
                # link_id += 1
    else:
        # 直径为奇数：最后一个交换机不拆分，连接所有后半GPU
        last_switch = last_switches[0]
        for i in range(gpus_per_first, num_gpus):
            gpu_id = gpu_ids[i]
            # 双向链路
            links.append({'id': link_id, 'src': gpu_id, 'dst': last_switch,
                         'bandwidth': link_bandwidth, 'latency': link_latency})
            link_id += 1
            # links.append({'id': link_id, 'src': last_switch, 'dst': gpu_id,
            #              'bandwidth': link_bandwidth, 'latency': link_latency})
            # link_id += 1
    
    # 3. 交换机之间的连接（相邻层）
    for i in range(len(switch_layers) - 1):
        curr_layer = switch_layers[i]
        next_layer = switch_layers[i + 1]
        
        curr_switches = curr_layer['switch_ids']
        next_switches = next_layer['switch_ids']
        
        # 每个当前层交换机连接到下一层的所有交换机
        for curr_sw in curr_switches:
            for next_sw in next_switches:
                # 双向链路
                links.append({'id': link_id, 'src': curr_sw, 'dst': next_sw,
                             'bandwidth': link_bandwidth, 'latency': link_latency})
                link_id += 1
                # links.append({'id': link_id, 'src': next_sw, 'dst': curr_sw,
                #              'bandwidth': link_bandwidth, 'latency': link_latency})
                # link_id += 1
    
    # 计算总数
    total_switches = len(all_switch_ids)
    total_nodes = num_gpus + total_switches
    total_links = len(links)
    
    # 写入文件
    with open(output_file, 'w') as f:
        f.write(f"{total_nodes}\t0\t0\t{total_switches}\t{total_links}\tH00\t{num_gpus}\n")
        f.write('\t'.join(map(str, all_switch_ids)) + '\n')
        for link in links:
            f.write(f"{link['src']}\t{link['dst']}\t{link['bandwidth']}Gbps\t{link['latency']}\t0.000\t100\tETH\n")
    
    print(f"拓扑文件已生成: {output_file}")
    print(f"总节点数: {total_nodes} (GPU: {num_gpus}, 交换机: {total_switches})")
    print(f"总链路数: {total_links}")
    print(f"链路带宽: {link_bandwidth}Gbps, 延迟: {link_latency}ns")
    print(f"\n交换机层级:")
    for layer in switch_layers:
        layer_type = "拆分" if layer['split'] else "不拆分"
        print(f"  第{layer['index']+1}个交换机 ({layer_type}): {layer['num_switches']}个, ID: {layer['switch_ids']}")
    
    return {
        'num_gpus': num_gpus,
        'total_switches': total_switches,
        'gpu_ids': gpu_ids,
        'all_switch_ids': all_switch_ids,
        'switch_layers': switch_layers,
        'links': links,
        'link_bandwidth': link_bandwidth,
        'link_latency': link_latency
    }


def visualize_topology(topo_info, output_image):
    """
    可视化拓扑结构
    """
    num_gpus = topo_info['num_gpus']
    total_switches = topo_info['total_switches']
    gpu_ids = topo_info['gpu_ids']
    switch_layers = topo_info['switch_layers']
    links = topo_info['links']
    link_bandwidth = topo_info['link_bandwidth']
    
    # 计算布局
    num_layers = len(switch_layers)
    max_switches = max(layer['num_switches'] for layer in switch_layers)
    
    # 创建图形
    fig, ax = plt.subplots(figsize=(max(16, max_switches * 3), max(12, num_layers * 4)))
    width = max_switches * 3
    height = (num_layers + 1) * 4
    ax.set_xlim(-1, width + 1)
    ax.set_ylim(-1, height + 1)
    ax.axis('off')
    
    # 存储所有节点位置
    node_positions = {}
    
    # 绘制GPU（顶部和底部）
    gpus_per_half = num_gpus // 2
    
    # 顶部GPU（连接第1个交换机）
    for i in range(gpus_per_half):
        x = (i + 0.5) * width / gpus_per_half
        y = height - 1
        node_positions[gpu_ids[i]] = (x, y)
        circle = plt.Circle((x, y), 0.3, facecolor='lightgreen', 
                           edgecolor='darkgreen', linewidth=2)
        ax.add_patch(circle)
        ax.text(x, y, f'G{gpu_ids[i]}', ha='center', va='center', 
               fontsize=8, fontweight='bold')
    
    # 底部GPU（连接最后一个交换机）
    for i in range(gpus_per_half, num_gpus):
        idx = i - gpus_per_half
        x = (idx + 0.5) * width / gpus_per_half
        y = 1
        node_positions[gpu_ids[i]] = (x, y)
        circle = plt.Circle((x, y), 0.3, facecolor='lightgreen',
                           edgecolor='darkgreen', linewidth=2)
        ax.add_patch(circle)
        ax.text(x, y, f'G{gpu_ids[i]}', ha='center', va='center',
               fontsize=8, fontweight='bold')
    
    # 绘制交换机层
    y_start = height - 3
    y_spacing = (height - 4) / num_layers if num_layers > 1 else 2
    
    for layer_idx, layer in enumerate(switch_layers):
        y = y_start - layer_idx * y_spacing
        num_sw = layer['num_switches']
        switch_ids = layer['switch_ids']
        
        for i, sw_id in enumerate(switch_ids):
            x = (i + 0.5) * width / num_sw
            node_positions[sw_id] = (x, y)
            
            # 颜色：拆分的用蓝色，不拆分的用橙色
            color = 'lightblue' if layer['split'] else 'lightyellow'
            edge_color = 'darkblue' if layer['split'] else 'darkorange'
            
            rect = FancyBboxPatch((x - 0.4, y - 0.3), 0.8, 0.6,
                                  boxstyle="round,pad=0.05",
                                  facecolor=color,
                                  edgecolor=edge_color,
                                  linewidth=2)
            ax.add_patch(rect)
            
            label = f'S{layer_idx}_{i}\nID:{sw_id}'
            ax.text(x, y, label, ha='center', va='center',
                   fontsize=8, fontweight='bold')
    
    # 绘制链路
    for link in links:
        src = link['src']
        dst = link['dst']
        if src in node_positions and dst in node_positions:
            src_pos = node_positions[src]
            dst_pos = node_positions[dst]
            arrow = FancyArrowPatch(src_pos, dst_pos,
                                   arrowstyle='-',
                                   color='gray',
                                   linewidth=0.8,
                                   alpha=0.3,
                                   zorder=1)
            ax.add_patch(arrow)
    
    # 标题
    plt.title(f'Linear Topology with Split Switches (Diameter={num_layers}, GPUs={num_gpus})',
             fontsize=16, fontweight='bold', pad=20)
    
    # 图例
    gpu_patch = mpatches.Circle((0, 0), 0.3, facecolor='lightgreen',
                               edgecolor='darkgreen', linewidth=2, label='GPU')
    normal_sw = mpatches.Rectangle((0, 0), 0.8, 0.6, facecolor='lightyellow',
                                  edgecolor='darkorange', linewidth=2, label='Normal Switch')
    split_sw = mpatches.Rectangle((0, 0), 0.8, 0.6, facecolor='lightblue',
                                 edgecolor='darkblue', linewidth=2, label='Split Switch')
    ax.legend(handles=[gpu_patch, normal_sw, split_sw], loc='upper right', fontsize=10)
    
    # 信息文本
    layer_desc = "\n".join([f"Layer{l['index']+1}: {l['num_switches']} switches ({'split' if l['split'] else 'normal'})"
                            for l in switch_layers])
    info_text = (f"Total Nodes: {num_gpus + total_switches}\n"
                f"GPUs: {num_gpus}\n"
                f"Switches: {total_switches}\n"
                f"Links: {len(links)}\n"
                f"Bandwidth: {link_bandwidth}Gbps\n"
                f"\n{layer_desc}")
    
    ax.text(0.02, 0.5, info_text,
           transform=ax.transAxes,
           fontsize=9,
           verticalalignment='center',
           bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(output_image, dpi=300, bbox_inches='tight')
    print(f"拓扑图已生成: {output_image}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(description='生成线性拓扑，偶数位置交换机拆分')
    parser.add_argument('diameter', type=int, help='网络直径（交换机层级数）')
    parser.add_argument('num_gpus', type=int, help='GPU节点总数')
    parser.add_argument('-o', '--output', default='topology.txt',
                       help='输出拓扑文件名')
    parser.add_argument('-i', '--image', default='topology.png',
                       help='输出图片文件名')
    parser.add_argument('-b', '--bandwidth', type=int, default=100,
                       help='链路带宽 (Gbps)')
    parser.add_argument('-l', '--latency', type=int, default=1000,
                       help='链路延迟 (ns)')
    
    args = parser.parse_args()
    
    topo_info = generate_topology(args.diameter, args.num_gpus, args.output,
                                  args.bandwidth, args.latency)
    visualize_topology(topo_info, args.image)
    
    print("\n完成!")


if __name__ == '__main__':
    main()

