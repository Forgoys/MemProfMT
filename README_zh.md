# MT3000 插桩工具

专为 MT3000 处理器设计的源代码插桩工具，通过时间分析和内存访问模式检测实现详细的性能分析。

[English](README.md) | [中文](README_zh.md)

## 功能特点

- **内存访问插桩**：内存访问模式分析
  - 跟踪数组访问和指针解引用
  - 识别主要访问模式
  - 计算访问频率和步长模式
  - 支持特定目标函数分析

## 环境要求

- LLVM 和 Clang（通过 Homebrew 安装）
- 支持 C++17 的编译器
- MT3000 开发环境

## 安装步骤

1. 安装 LLVM 和 Clang：
```bash
brew install llvm
```

2. 克隆仓库：
```bash
git clone https://github.com/yourusername/mt3000-inst.git
cd mt3000-inst
```

3. 构建项目：
```bash
make
```

## 使用方法

### 基本命令

```bash
./bin/MemProfMT -memory-inst input.c -o instrumented_output.c
```

### 内存插桩选项

```bash
-memory-inst            # 启用内存访问插桩
-target-funcs          # 指定目标函数（逗号分隔）
-o <filename>          # 指定输出文件名
--                     # 编译器选项分隔符
```

## 输出分析

内存插桩提供：
- 数组和指针的访问模式分析
- 步长模式检测
- 访问频率统计
- 每个变量的内存访问报告

输出示例：
```
[内存分析] array_a in process_data: 元素数=1000, 访问次数=5000
  模式1: 步长=1 (95.2%)
  模式2: 步长=32 (4.8%)
```

## 实现细节

- 使用 Clang 的 LibTooling 进行源代码插桩
- 支持多线程应用（最多24个线程）
- 提供低开销的精确内存访问跟踪

## 限制条件

- 当前仅支持 C/C++ 源文件
- 内存插桩仅限于数组、指针和结构体成员
- 每个变量最多支持16种不同的访问模式