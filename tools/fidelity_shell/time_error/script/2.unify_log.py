#!/usr/bin/env python3
"""
统一日志格式脚本：
将两种不同格式的日志文件统一为key=value格式，保留共同字段
"""
import re
import sys
import os

def parse_ip_hex(ip_hex):
    """
    解析十六进制IP地址，提取节点编号
    例如: 0b000001 -> 节点0, 0b000401 -> 节点4
    IP地址格式: 第三个字节（从右往左第二个字节）表示节点号
    """
    try:
        # 移除0b前缀（如果有）
        ip_hex = ip_hex.replace('0b', '')
        # 转换为整数
        ip_int = int(ip_hex, 16)
        # 提取第三个字节（从右往左第二个字节）作为节点号
        # 例如: 0x00000401 -> (0x00000401 >> 8) & 0xFF = 0x04 = 4
        node_id = (ip_int >> 8) & 0xFF
        return node_id
    except:
        return None

def parse_out1_line(line):
    """
    解析out1.log格式：
    [FCT] npu_id: 0, check flow finish: flow id 1: src 0-> dst 4, flow size: 2097152, start_time: 1000, stop_time: 177739, durationMicros: 176739  chakraNode: 0
    """
    result = {}
    
    # 提取 npu_id
    npu_match = re.search(r'npu_id:\s*(\d+)', line)
    if npu_match:
        result['npu_id'] = int(npu_match.group(1))
    
    # 提取 flow id
    flow_id_match = re.search(r'flow id (\d+):', line)
    if flow_id_match:
        result['flow_id'] = int(flow_id_match.group(1))
    
    # 提取 src
    src_match = re.search(r'src (\d+)', line)
    if src_match:
        result['src'] = int(src_match.group(1))
    
    # 提取 dst
    dst_match = re.search(r'dst (\d+)', line)
    if dst_match:
        result['dst'] = int(dst_match.group(1))
    
    # 提取 flow size
    size_match = re.search(r'flow size:\s*(\d+)', line)
    if size_match:
        result['size'] = int(size_match.group(1))
    
    # 提取 start_time
    start_match = re.search(r'start_time:\s*(\d+)', line)
    if start_match:
        result['start_time'] = int(start_match.group(1))
    
    # 提取 durationMicros (fct)
    fct_match = re.search(r'durationMicros:\s*(\d+)', line)
    if fct_match:
        result['fct'] = int(fct_match.group(1))
    
    # 提取 chakraNode
    chakra_match = re.search(r'chakraNode:\s*(\d+)', line)
    if chakra_match:
        result['chakra_node'] = int(chakra_match.group(1))
    
    # multiverse格式没有standalone_fct，设置为0
    result['standalone_fct'] = 0
    
    return result

def parse_fct_line(line):
    """
    解析fct-100g-1000ns.txt格式：
    0b000001 0b000401 10000 100 2097152 10 186449 179988 0 0 1 0
    格式: sip dip sport dport m_size startTime duration standalone_fct npu_id chakra_node_id is_collective flow_index
    """
    parts = line.strip().split()
    if len(parts) < 12:
        return None
    
    result = {}
    
    try:
        # sip -> src
        sip_hex = parts[0]
        src = parse_ip_hex(sip_hex)
        if src is not None:
            result['src'] = src
        
        # dip -> dst
        dip_hex = parts[1]
        dst = parse_ip_hex(dip_hex)
        if dst is not None:
            result['dst'] = dst
        
        # sport (跳过)
        # dport (跳过)
        
        # m_size -> size
        result['size'] = int(parts[4])
        
        # startTime -> start_time
        result['start_time'] = int(parts[5])
        
        # duration -> fct (使用duration作为fct)
        result['fct'] = int(parts[6])
        
        # standalone_fct
        result['standalone_fct'] = int(parts[7])
        
        # npu_id
        result['npu_id'] = int(parts[8])
        
        # chakra_node_id -> chakra_node
        result['chakra_node'] = int(parts[9])
        
        # is_collective (跳过)
        
        # flow_index -> flow_id
        result['flow_id'] = int(parts[11])
        
    except (ValueError, IndexError) as e:
        return None
    
    return result

def format_output(result):
    """
    将解析结果格式化为key=value格式
    """
    # 按顺序输出共同字段
    fields = ['npu_id', 'chakra_node', 'src', 'dst', 'size', 'start_time', 'fct', 'standalone_fct', 'flow_id']
    parts = []
    
    for field in fields:
        if field in result:
            parts.append(f"{field}={result[field]}")
    
    return ' '.join(parts)

def process_file(input_file, output_file, file_type):
    """
    处理文件
    file_type: 'out1' 或 'fct'
    """
    if not os.path.exists(input_file):
        print(f"错误: 输入文件不存在: {input_file}")
        return False
    
    print(f"处理文件: {input_file} (格式: {file_type})")
    
    parse_func = parse_out1_line if file_type == 'out1' else parse_fct_line
    
    total_lines = 0
    parsed_lines = 0
    
    with open(input_file, 'r', encoding='utf-8', errors='ignore') as f_in, \
         open(output_file, 'w', encoding='utf-8') as f_out:
        
        for line in f_in:
            line = line.strip()
            if not line:
                continue
            
            total_lines += 1
            
            # 对于out1格式，只处理[FCT]开头的行
            if file_type == 'out1' and not line.startswith('[FCT]'):
                continue
            
            result = parse_func(line)
            
            if result and all(key in result for key in ['npu_id', 'chakra_node', 'src', 'dst', 'size', 'start_time', 'fct', 'standalone_fct', 'flow_id']):
                formatted = format_output(result)
                f_out.write(formatted + '\n')
                parsed_lines += 1
            else:
                # 打印所有解析失败的行
                missing_fields = []
                required_fields = ['npu_id', 'chakra_node', 'src', 'dst', 'size', 'start_time', 'fct', 'standalone_fct', 'flow_id']
                if result:
                    for field in required_fields:
                        if field not in result:
                            missing_fields.append(field)
                    print(f"  警告: 解析失败 (行 {total_lines}): 缺少字段 {missing_fields}")
                    print(f"        行内容: {line[:100]}")
                else:
                    print(f"  警告: 解析失败 (行 {total_lines}): 无法解析")
                    print(f"        行内容: {line[:100]}")
    
    print(f"  总行数: {total_lines}")
    print(f"  成功解析: {parsed_lines}")
    print(f"  输出文件: {output_file}")
    
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("用法: python3 unify_log_format.py <输入文件> <输出文件> [文件类型]")
        print("  文件类型: 'out1' 或 'fct' (默认自动检测)")
        print("示例:")
        print("  python3 unify_log_format.py data/out1.log data/out1_unified.txt out1")
        print("  python3 unify_log_format.py data/fct-100g-1000ns.txt data/fct_unified.txt fct")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    # 自动检测文件类型
    if len(sys.argv) >= 4:
        file_type = sys.argv[3]
    else:
        # 根据文件名或内容自动检测
        if 'out1' in input_file.lower() or 'multiverse' in input_file.lower():
            file_type = 'out1'
        elif 'fct' in input_file.lower():
            file_type = 'fct'
        else:
            # 读取第一行检测
            with open(input_file, 'r', encoding='utf-8', errors='ignore') as f:
                first_line = f.readline()
                if first_line.startswith('[FCT]'):
                    file_type = 'out1'
                else:
                    file_type = 'fct'
            print(f"自动检测文件类型: {file_type}")
    
    if file_type not in ['out1', 'fct']:
        print(f"错误: 不支持的文件类型: {file_type}")
        sys.exit(1)
    
    success = process_file(input_file, output_file, file_type)
    sys.exit(0 if success else 1)

