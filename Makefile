# Makefile
# 获取CPU核心数，用于并行编译
NPROCS = $(shell nproc || sysctl -n hw.ncpu || echo 2)
MAKEFLAGS += -j$(NPROCS)

# 根据操作系统设置LLVM路径
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LLVM_HOME := /usr/lib/llvm-19
else
    LLVM_HOME := /opt/homebrew/opt/llvm
endif

# 使用本地的 clang++
CLANG := $(LLVM_HOME)/bin/clang++
CLANG_FLAGS := -std=c++17 -fno-color-diagnostics -fno-rtti

# 使用本地的 llvm-config
LLVM_CONFIG := $(LLVM_HOME)/bin/llvm-config
LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --link-shared --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --link-shared --libs)
LLVM_SYSTEM_LIBS := $(shell $(LLVM_CONFIG) --system-libs)
LLVM_INCLUDEDIR := $(shell $(LLVM_CONFIG) --includedir)

# Clang 的包含目录和库
CLANG_INCLUDEDIR := $(LLVM_HOME)/include
CLANG_LIBS := -lclang-cpp

# 项目目录结构
SRC_DIR := src
INC_DIR := include
RUNTIME_DIR := runtime
BUILD_DIR := build
BIN_DIR := bin

# 合并所有编译选项
TOOL_CLANG_FLAGS := $(CLANG_FLAGS) $(LLVM_CXXFLAGS) -I$(CLANG_INCLUDEDIR) -I$(INC_DIR)
TOOL_LINK_FLAGS := -L$(LLVM_HOME)/lib -Wl,-rpath,$(LLVM_HOME)/lib $(LLVM_LDFLAGS) $(LLVM_LIBS) $(CLANG_LIBS) $(LLVM_SYSTEM_LIBS)

# 源文件和目标文件
SRCS := $(SRC_DIR)/CallGraph.cpp \
        $(SRC_DIR)/TimeInstrumentation.cpp \
        $(SRC_DIR)/MemoryInstrumentation.cpp \
        $(SRC_DIR)/main.cpp \
        $(SRC_DIR)/FrontendAction.cpp \
        $(SRC_DIR)/CommandLineOptions.cpp
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 目标文件
TARGET := $(BIN_DIR)/MemProfMT

.PHONY: all clean
.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/. $(BIN_DIR)/.

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)/.
	$(CLANG) $(TOOL_CLANG_FLAGS) -o $@ $^ $(TOOL_LINK_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)/.
	$(CLANG) $(TOOL_CLANG_FLAGS) -c $< -o $@

%/.:
	mkdir -p $(@D)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)