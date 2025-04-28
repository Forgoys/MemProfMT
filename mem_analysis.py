#!/usr/bin/env python3
import sys
import re
from collections import defaultdict
import csv
from typing import Dict, List, NamedTuple, Set

class Pattern:
    def __init__(self, step: int, percentage: float):
        self.step = step
        self.percentage = percentage
        
    @property
    def access_count(self) -> float:
        """
        This is a placeholder property that will be set externally
        when we need to calculate actual access counts
        """
        return getattr(self, '_access_count', 0)
    
    @access_count.setter
    def access_count(self, value: float):
        self._access_count = value

class MemoryAccess:
    def __init__(self, thread_id: int, var_name: str, func_name: str, elements: int, accesses: int):
        self.thread_id = thread_id
        self.var_name = var_name
        self.func_name = func_name
        self.elements = elements
        self.accesses = accesses
        self.patterns: List[Pattern] = []
    
    def calculate_pattern_access_counts(self):
        """Calculate actual access counts for each pattern based on percentage"""
        for pattern in self.patterns:
            pattern.access_count = self.accesses * (pattern.percentage / 100.0)

def parse_memory_analysis(filepath: str) -> List[MemoryAccess]:
    accesses = []
    
    header_pattern = r'\[Memory Analysis\] thread (\d+): (\w+) in (\w+): elements=(\d+), accesses=(\d+)'
    pattern_line = r'Pattern \d+: step=(\d+) \(([\d.]+)%\)'
    
    current_access = None
    
    # 以二进制模式读取文件
    with open(filepath, 'rb') as f:
        accumulated_line = bytearray()
        
        while True:
            chunk = f.read(1024)  # 每次读取1KB
            if not chunk:
                break
                
            for byte in chunk:
                # 如果是正常的ASCII字符或换行符
                if byte < 0x80 or byte == 0x0a:
                    accumulated_line.append(byte)
                    
                    # 如果是换行符，处理这一行
                    if byte == 0x0a:
                        try:
                            line = accumulated_line.decode('ascii')
                            
                            header_match = re.search(header_pattern, line)
                            if header_match:
                                if current_access and current_access.accesses > 0:
                                    accesses.append(current_access)
                                
                                thread_id, var_name, func_name, elements, accesses_count = header_match.groups()
                                current_access = MemoryAccess(
                                    thread_id=int(thread_id),
                                    var_name=var_name,
                                    func_name=func_name,
                                    elements=int(elements),
                                    accesses=int(accesses_count)
                                )
                            else:
                                pattern_match = re.search(pattern_line, line)
                                if pattern_match and current_access:
                                    step, percentage = pattern_match.groups()
                                    current_access.patterns.append(Pattern(
                                        step=int(step),
                                        percentage=float(percentage)
                                    ))
                        except Exception as e:
                            print(f"警告：处理行时出错 {e}, 跳过此行")
                        
                        accumulated_line = bytearray()  # 清空累积的行
                # 忽略非ASCII字符
                
    # 处理最后一行（如果有的话）
    if current_access and current_access.accesses > 0:
        accesses.append(current_access)
    
    return accesses

def merge_memory_analysis(accesses: List[MemoryAccess]) -> List[MemoryAccess]:
    # 按线程号、变量名和函数名分组
    groups = defaultdict(list)
    for access in accesses:
        # 确保只处理访问次数大于0的记录
        if access.accesses > 0:
            key = (access.thread_id, access.var_name, access.func_name)
            groups[key].append(access)
    
    merged_results = []
    
    # 按变量名和函数名的组合进行合并
    thread_var_func_groups = defaultdict(list)
    for (thread_id, var_name, func_name), group in groups.items():
        key = (var_name, func_name)
        thread_var_func_groups[key].extend(group)
    
    for (var_name, func_name), group in thread_var_func_groups.items():
        # 取最大元素大小
        max_elements = max(access.elements for access in group)
        
        # 访问次数取和
        total_accesses = sum(access.accesses for access in group)
        
        # 收集所有步长的访问次数
        step_access_counts = defaultdict(float)
        
        # 对每个访问记录计算实际的访问次数
        for access in group:
            access.calculate_pattern_access_counts()
            for pattern in access.patterns:
                step_access_counts[pattern.step] += pattern.access_count
        
        # 创建合并后的访问记录
        merged_access = MemoryAccess(0, var_name, func_name, max_elements, total_accesses)
        
        # 计算新的访问模式百分比，并且只保留大于等于5%的模式
        for step, access_count in step_access_counts.items():
            percentage = (access_count / total_accesses) * 100
            if percentage >= 5.0:  # 只保留百分比大于等于5%的模式
                merged_access.patterns.append(Pattern(step, percentage))
        
        # 按百分比降序排序模式
        merged_access.patterns.sort(key=lambda x: x.percentage, reverse=True)
        merged_results.append(merged_access)
    
    return merged_results

def write_csv(accesses: List[MemoryAccess], output_file: str):
    if not accesses:
        print("警告：没有找到有效的访存分析数据")
        return
        
    # 找出最大模式数量
    max_patterns = max(len(access.patterns) for access in accesses)
    
    # 准备表头
    headers = ['Variable', 'Function', 'Elements', 'Accesses']
    for i in range(max_patterns):
        headers.extend([f'Pattern_{i+1}_Step', f'Pattern_{i+1}_Percentage'])
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        
        for access in accesses:
            row = [
                access.var_name,
                access.func_name,
                access.elements,
                access.accesses
            ]
            
            # 添加模式信息
            for pattern in access.patterns:
                row.extend([pattern.step, f"{pattern.percentage:.1f}"])
            
            # 用空值填充剩余的模式列
            remaining_patterns = max_patterns - len(access.patterns)
            row.extend([''] * (remaining_patterns * 2))
            
            writer.writerow(row)

def main():
    if len(sys.argv) != 2:
        print("使用方法: python3 analyze_memory.py <input_file>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    try:
        # 解析内存分析输出
        accesses = parse_memory_analysis(input_file)
        
        if not accesses:
            print("警告：未找到任何有效的访存分析数据")
            sys.exit(0)
            
        # 合并结果
        merged_results = merge_memory_analysis(accesses)
        
        # 写入CSV
        output_file = "memory_analysis.csv"
        write_csv(merged_results, output_file)
        
        print(f"\n分析完成. 结果已写入 {output_file}")
        
    except Exception as e:
        print(f"错误: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()