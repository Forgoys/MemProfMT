# Makefile
# 获取CPU核心数，用于并行编译
NPROCS = $(shell nproc || sysctl -n hw.ncpu || echo 2)
MAKEFLAGS += -j$(NPROCS)

# 设置本地 LLVM 和 Clang 的路径
LLVM_HOME := /opt/homebrew/opt/llvm

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

# 在运行时指定资源目录
ARGS := -resource-dir=$(LLVM_HOME)/lib/clang/19

# 源文件和目标文件
SRCS := $(SRC_DIR)/CallGraph.cpp \
        $(SRC_DIR)/TimeInstrumentation.cpp \
        $(SRC_DIR)/main.cpp
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 目标文件
TARGET := $(BIN_DIR)/mt3000-inst

.PHONY: all build clean install

all: build

build: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CLANG) $(TOOL_CLANG_FLAGS) -o $@ $^ $(TOOL_LINK_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CLANG) $(TOOL_CLANG_FLAGS) -c $< -o $@

$(BIN_DIR) $(BUILD_DIR):
	mkdir -p $@

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin
	install -d $(DESTDIR)/usr/local/include/mt3000-inst
	install -m 644 $(RUNTIME_DIR)/*.h $(DESTDIR)/usr/local/include/mt3000-inst

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# 运行目标程序
run: build
	@echo "Running mt3000-inst..."
	export LD_LIBRARY_PATH=$(LLVM_HOME)/lib:$$LD_LIBRARY_PATH; \
	$(TARGET) $(ARGS) $(RUN_ARGS)