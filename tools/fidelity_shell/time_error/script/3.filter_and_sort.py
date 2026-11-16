#!/usr/bin/env python3
"""
筛选和排序日志脚本：
1. 根据npu_id筛选
2. 按照chakra_node递增排列
3. 每个chakra_node内的流按照start_time排列
4. 输出格式：只显示value，按照 flow_id, start_time, src, 
dst, size, standalone_fct, fct 的顺序
"""
import sys
import os

def parse_line(line):
    """解析key=value格式的行"""
    result = {}
    for pair in line.strip().split():
        if '=' in pair:
            key, value = pair.split('=', 1)
            try:
                # 尝试转换为整数
                result[key] = int(value)
            except ValueError:
                result[key] = value
    return result

def format_output(flow):
    """格式化输出：只显示value，按照 flow_id, start_time, src, dst, size, standalone_fct, fct 的顺序"""
    standalone_fct = flow.get('standalone_fct', 'x')
    return f"{flow['flow_id']} {flow['start_time']} {flow['src']} {flow['dst']} {flow['size']} {standalone_fct} {flow['fct']}"

def process_file(input_file, npu_id, output_file=None):
    """
    处理文件
    """
    if not os.path.exists(input_file):
        print(f"错误: 输入文件不存在: {input_file}")
        return False
    
    print(f"处理文件: {input_file}")
    print(f"筛选条件: npu_id={npu_id}")
    
    flows = []
    
    # 读取并解析文件
    with open(input_file, 'r', encoding='utf-8', errors='ignore') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            
            flow = parse_line(line)
            
            # 筛选npu_id
            if flow.get('npu_id') == npu_id:
                # 检查必需字段（standalone_fct是可选的）
                required_fields = ['chakra_node', 'flow_id', 'src', 'dst', 'size', 'start_time', 'fct']
                if all(field in flow for field in required_fields):
                    flows.append(flow)
                else:
                    print(f"警告: 行 {line_num} 缺少必需字段: {line[:80]}")
    
    if not flows:
        print(f"未找到 npu_id={npu_id} 的数据")
        return False
    
    print(f"找到 {len(flows)} 条流")
    
    # 排序：先按chakra_node递增，再按flow_id, start_time, src, dst, size, standalone_fct, fct排序
    def sort_key(flow):
        standalone_fct = flow.get('standalone_fct', 0)  # 如果没有standalone_fct，用0作为排序值
        return (flow['chakra_node'], 
                flow['flow_id'], 
                flow['start_time'], 
                flow['src'], 
                flow['dst'], 
                flow['size'], 
                standalone_fct, 
                flow['fct'])
    flows.sort(key=sort_key)
    
    # 输出结果
    if output_file:
        f_out = open(output_file, 'w', encoding='utf-8')
    else:
        f_out = sys.stdout
    
    current_chakra = None
    for flow in flows:
        chakra_node = flow['chakra_node']
        
        # 如果是新的chakra_node，打印标题和分隔符
        if current_chakra is not None and chakra_node != current_chakra:
            f_out.write('\n')
        
        if current_chakra is None or chakra_node != current_chakra:
            f_out.write(f"{chakra_node}\n")  # 只显示chakra_node的值，不显示key
            current_chakra = chakra_node
        
        # 打印流信息
        f_out.write(format_output(flow) + '\n')
    
    if output_file:
        f_out.close()
        print(f"输出文件: {output_file}")
    else:
        print(f"\n共输出 {len(flows)} 条流")
    
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("用法: python3 filter_and_sort.py <输入文件> <npu_id> [输出文件]")
        print("示例:")
        print("  python3 filter_and_sort.py data/multiverse_v2.log 0")
        print("  python3 filter_and_sort.py data/multiverse_v2.log 0 output.txt")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    try:
        npu_id = int(sys.argv[2])
    except ValueError:
        print(f"错误: npu_id必须是整数: {sys.argv[2]}")
        sys.exit(1)
    
    output_file = sys.argv[3] if len(sys.argv) >= 4 else None
    
    success = process_file(input_file, npu_id, output_file)
    sys.exit(0 if success else 1)

