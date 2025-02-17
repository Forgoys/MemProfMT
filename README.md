# MT3000 Instrumentation Tool

A powerful source code instrumentation tool designed for MT3000 processors, enabling detailed performance analysis through time profiling and memory access pattern detection.

[English](#english) | [中文](#chinese)

<a name="english"></a>
## Features

- **Time Instrumentation**: Analyzes execution time at function granularity
  - Tracks function execution time across multiple threads
  - Identifies hot functions based on configurable thresholds
  - Generates detailed call tree visualization
  - Provides parent-child time distribution analysis

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
# Time instrumentation
./bin/MemProfMT -time-inst input.c -o instrumented_output.c

# Memory access instrumentation
./bin/MemProfMT -memory-inst input.c -o instrumented_output.c
```

### Time Instrumentation Options

```bash
-time-inst              # Enable time instrumentation
-total-time-threshold   # Set threshold for total execution time (default: 20%)
-parent-time-threshold  # Set threshold for parent function time (default: 40%)
```

### Memory Instrumentation Options

```bash
-memory-inst            # Enable memory access instrumentation
-target-funcs          # Specify target functions (comma-separated)
```

### Common Options

```bash
-o <filename>          # Specify output filename
--                     # Separator for compiler options
```

## Output Analysis

### Time Profiling Results

The time instrumentation generates a detailed report including:
- Total program execution time
- Function call tree with timing information
- Hot function analysis based on:
  - Root functions consuming ≥20% of total time
  - Called functions consuming ≥40% of caller's time

Example output:
```
═══════════════════════════════════════════════
              Timing Analysis Report              
═══════════════════════════════════════════════

Total Program Time: 125.34 ms

main
│   ├── process_data: 45.67 ms (36.4% of main)
│   └── analyze_results: 68.21 ms (54.4% of main)
```

### Memory Access Results

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
- Implements efficient time measurement using hardware clock cycles
- Provides accurate memory access tracking with minimal overhead

## Limitations

- Currently supports C/C++ source files only
- Time instrumentation overhead may affect absolute timing measurements
- Memory instrumentation limited to arrays, pointers, and struct members
- Maximum of 16 different access patterns per variable