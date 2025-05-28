# MT3000 Instrumentation Tool

A powerful source code instrumentation tool designed for MT3000 processors, enabling detailed performance analysis through time profiling and memory access pattern detection.

[English](README.md) | [中文](README_zh.md)

## Features

- **Memory Access Instrumentation**: Analyzes memory access patterns
  - Tracks array accesses and pointer dereferences
  - Identifies dominant access patterns
  - Calculates access frequencies and stride patterns
  - Supports analysis of specific target functions

## Prerequisites

- LLVM and Clang (installed via Homebrew)
- C++17 compatible compiler
- MT3000 development environment

## Installation

1. Ensure LLVM and Clang are installed:
```bash
brew install llvm
```

2. Clone the repository:
```bash
git clone https://github.com/yourusername/mt3000-inst.git
cd mt3000-inst
```

3. Build the project:
```bash
make
```

## Usage

### Basic Commands

```bash
./bin/MemProfMT input.c -o instrumented_output.c
```

### Memory Instrumentation Options

```bash
-target-funcs          # Specify target functions (comma-separated)
-o <filename>          # Specify output filename
--                     # Separator for compiler options
```

## Output Analysis

The memory instrumentation provides:
- Access pattern analysis for arrays and pointers
- Stride pattern detection
- Access frequency statistics
- Per-variable memory access reports

Example output:
```
[Memory Analysis] array_a in process_data: elements=1000, accesses=5000
  Pattern 1: step=1 (95.2%)
  Pattern 2: step=32 (4.8%)
```

## Implementation Details

- Uses Clang's LibTooling for source code instrumentation
- Supports multi-threaded applications (up to 24 threads)
- Provides accurate memory access tracking with minimal overhead

## Limitations

- Currently supports C/C++ source files only
- Memory instrumentation limited to arrays, pointers, and struct members
- Maximum of 16 different access patterns per variable