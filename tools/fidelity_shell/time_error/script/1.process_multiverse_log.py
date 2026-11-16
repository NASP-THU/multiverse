#!/usr/bin/env python3
"""
处理日志文件脚本：
1. 仅保留以 [FCT] 和 [FLOW] 开头的行
2. 去重
3. 从[FLOW]行提取flow_id和chakraNode，为[FCT]行添加chakraNode字段
4. 只保留[FCT]的行（包含chakraNode字段的）
"""
import re
import sys
import os

def extract_flow_info(line):
    """从[FLOW]行提取flow_id和chakraNode"""
    # 匹配 chakraNode[x]
    chakra_match = re.search(r'chakraNode\[(\d+)\]', line)
    if not chakra_match:
        return None, None
    
    chakra_node = chakra_match.group(1)
    
    # 匹配 flow_id=x
    flow_id_match = re.search(r'flow_id=(\d+)', line)
    if not flow_id_match:
        return None, None
    
    flow_id = int(flow_id_match.group(1))
    return flow_id, chakra_node

def extract_fct_flow_id(line):
    """从[FCT]行提取flow id"""
    match = re.search(r'flow id (\d+):', line)
    if match:
        return int(match.group(1))
    return None

def process_log_file(input_file, output_file):
    """
    处理日志文件
    """
    if not os.path.exists(input_file):
        print(f"错误: 输入文件不存在: {input_file}")
        return False
    
    print(f"开始处理文件: {input_file}")
    
    # 第一步：提取[FCT]和[FLOW]行，并去重
    print("步骤1: 提取[FCT]和[FLOW]行并去重...")
    filtered_lines = []
    seen = set()
    
    with open(input_file, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.rstrip('\n\r')
            if line.startswith('[FCT]') or line.startswith('[FLOW]'):
                # 去重
                if line not in seen:
                    seen.add(line)
                    filtered_lines.append(line)
    
    print(f"  提取了 {len(filtered_lines)} 行（去重后）")
    
    # 第二步：建立flow_id到chakraNode的映射
    print("步骤2: 建立flow_id到chakraNode的映射...")
    flow_to_chakra = {}
    
    for line in filtered_lines:
        if line.startswith('[FLOW]'):
            flow_id, chakra_node = extract_flow_info(line)
            if flow_id is not None and chakra_node is not None:
                # 如果同一个flow_id出现多次，保留第一个
                if flow_id not in flow_to_chakra:
                    flow_to_chakra[flow_id] = chakra_node
    
    print(f"  建立了 {len(flow_to_chakra)} 个flow_id到chakraNode的映射")
    
    # 第三步：处理所有行，为[FCT]行添加chakraNode字段，并只保留[FCT]行
    print("步骤3: 为[FCT]行添加chakraNode字段...")
    fct_lines_with_chakra = []
    fct_lines_without_chakra = []
    
    for line in filtered_lines:
        if line.startswith('[FCT]'):
            flow_id = extract_fct_flow_id(line)
            if flow_id is not None and flow_id in flow_to_chakra:
                chakra_node = flow_to_chakra[flow_id]
                # 在行末尾添加 chakraNode字段
                line_with_chakra = f"{line} chakraNode: {chakra_node}"
                fct_lines_with_chakra.append(line_with_chakra)
            else:
                fct_lines_without_chakra.append(line)
    
    print(f"  成功添加chakraNode的[FCT]行: {len(fct_lines_with_chakra)}")
    if fct_lines_without_chakra:
        print(f"  未匹配到chakraNode的[FCT]行: {len(fct_lines_without_chakra)}")
        print("\n未匹配的行详情:")
        for i, line in enumerate(fct_lines_without_chakra, 1):
            flow_id = extract_fct_flow_id(line)
            if flow_id is not None:
                print(f"  {i}. flow_id={flow_id}: {line}")
            else:
                print(f"  {i}. (无法提取flow_id): {line}")
        print()
    
    # 第四步：只保留有chakraNode的[FCT]行，写入输出文件
    print("步骤4: 写入输出文件...")
    with open(output_file, 'w', encoding='utf-8') as f_out:
        for line in fct_lines_with_chakra:
            f_out.write(line + '\n')
    
    print(f"处理完成！输出文件: {output_file}")
    print(f"输出行数: {len(fct_lines_with_chakra)}")
    
    return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法: python3 process_log.py <输入文件> [输出文件]")
        print("示例: python3 process_log.py data/multiverse.1229_v5.log")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        # 如果没有指定输出文件，自动生成
        base_name = os.path.splitext(input_file)[0]
        output_file = f"{base_name}_fct_only.log"
    
    success = process_log_file(input_file, output_file)
    sys.exit(0 if success else 1)

