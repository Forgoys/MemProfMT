#ifndef MEMORY_PROFILER_H
#define MEMORY_PROFILER_H

#include <sstream>
#include <vector> 
#include <string> 

// 内存访问分析代码生成器
class MemoryCodeGenerator
{
public:
    // 常量定义
    constexpr static unsigned NUM_THREADS = 24;      // MT3000的线程数
    constexpr static unsigned MAX_PATTERNS = 16;     // 记录的最大访存模式数
    constexpr static unsigned NAME_SIZE = 64;        // 名称最大长度
    constexpr static unsigned PATTERN_THRESHOLD = 5; // 访存模式识别阈值(%)

    // 生成访存分析的基本数据结构
    static std::string generateBaseStructures(const std::vector<std::string> &includes)
    {
        std::stringstream ss;

        // 检查是否需要添加头文件
        bool hasStdio = false;
        bool hasString = false;
        bool hasHthreadDevice = false;
        for (const auto &inc : includes) {
            if (inc == "stdio.h")
                hasStdio = true;
            if (inc == "string.h")
                hasString = true;
            if (inc == "hthread_device.h")
                hasHthreadDevice = true;
        }

        if (!hasStdio)
            ss << "#include <stdio.h>\n";
        if (!hasString)
            ss << "#include <string.h>\n";
        if (!hasHthreadDevice)
            ss << "#include \"hthread_device.h\"\n";

        // 定义常量
        ss << "#ifndef MEM_PROFILER_DEFS\n"
           << "#define MEM_PROFILER_DEFS\n"
           << "#define MEM_MAX_PATTERNS " << MAX_PATTERNS << "\n"
           << "#define MEM_NAME_SIZE " << NAME_SIZE << "\n"
           << "#define MEM_NUM_THREADS " << NUM_THREADS << "\n"
           << "#define MEM_TOP_PATTERNS 3\n\n";

        // 定义数据结构
        ss << "typedef struct {\n"
           << "    char var_name[MEM_NAME_SIZE];            // 变量名\n"
           << "    char func_name[MEM_NAME_SIZE];           // 所在函数名\n"
           << "    size_t base_addr;                 // 变量基地址\n"
           << "    size_t end_addr;                  // 变量访存范围结尾地址\n"
           << "    size_t total_accesses;            // 总访问次数\n"
           << "    size_t patterns[MEM_MAX_PATTERNS];       // 访存步长模式\n"
           << "    size_t pattern_counts[MEM_MAX_PATTERNS]; // 各模式出现次数\n"
           << "    size_t last_addr;                 // 上次访问地址\n"
           << "    size_t var_size;                  // 变量大小\n"
           << "    size_t type_size;                 // 变量类型大小\n"
           << "} mem_profile_t;\n\n"
           << "#endif // MEM_PROFILER_DEFS\n\n";

        return ss.str();
    }

    // 生成访存分析器的初始化函数
    static std::string generateInitFunction()
    {
        std::stringstream ss;
        ss << "// 初始化访存分析器\n"
           << "static inline void __mem_init(mem_profile_t* prof,\n"
           << "                             const char* var_name,\n"
           << "                             const char* func_name,\n"
           << "                             void* addr,\n"
           << "                             size_t type_size) {\n"
           << "    strncpy(prof->var_name, var_name, MEM_NAME_SIZE-1);\n"
           << "    strncpy(prof->func_name, func_name, MEM_NAME_SIZE-1);\n"
           << "    prof->base_addr = (size_t)addr;\n"
           << "    prof->end_addr = prof->base_addr;\n"
           << "    prof->total_accesses = 0;\n"
           << "    prof->last_addr = prof->base_addr;\n"
           << "    prof->var_size = 0;\n"
           << "    prof->type_size = type_size;\n"
           << "    memset(prof->patterns, -1, sizeof(prof->patterns));\n"
           << "    memset(prof->pattern_counts, 0, sizeof(prof->pattern_counts));\n"
           << "}\n\n";
        return ss.str();
    }

    // 生成访存记录函数
    static std::string generateRecordFunction()
    {
        std::stringstream ss;
        ss << "// 记录一次内存访问\n"
           << "static inline void __mem_record(mem_profile_t* prof, void* addr) {\n"
           << "    size_t step;\n"
           << "    size_t curr_addr = (size_t)addr;\n"
           << "    \n"
           << "    // 如果是第一次访问，更新last_addr为第一次访存地址\n"
           << "    if (prof->total_accesses == 0) {\n"
           << "        prof->last_addr = curr_addr;\n"
           << "        prof->base_addr = curr_addr;\n"
           << "        prof->end_addr = curr_addr;\n"
           << "    }\n"
           << "    prof->total_accesses++;\n"
           << "    \n"
           << "    // 计算归一化访存步长\n"
           << "    step = curr_addr < prof->last_addr ? (prof->last_addr - curr_addr) : (curr_addr - "
              "prof->last_addr);\n"
           << "    step /= prof->type_size;\n"
           << "    prof->last_addr = curr_addr;\n"
           << "    prof->end_addr = curr_addr > prof->end_addr ? curr_addr : prof->end_addr;\n"
           << "    prof->base_addr = curr_addr < prof->base_addr ? curr_addr : prof->base_addr;\n"
           << "    \n"
           << "    // 判断跨步过大的访问\n"
           << "    if (step >= 65536) return;\n"
           << "    \n"
           << "    // 记录访存模式\n"
           << "    for(int i = 0; i < MEM_MAX_PATTERNS; i++) {\n"
           << "        if(prof->patterns[i] == step) {\n"
           << "            prof->pattern_counts[i]++;\n"
           << "            return;\n"
           << "        }else if(prof->patterns[i] == 0xFFFFFFFFFFFFFFFF) {\n"
           << "            prof->patterns[i] = step;\n"
           << "            prof->pattern_counts[i] = 1;\n"
           << "            return;\n"
           << "        }\n"
           << "    }\n"
           << "}\n\n";
        return ss.str();
    }

    // 生成结果分析函数
    static std::string generateAnalysisFunction()
    {
        std::stringstream ss;
        ss << "// 分析访存结果\n"
           << "static inline void __mem_analyze(mem_profile_t* prof) {\n"
           << "    int i, j;\n"
           << "    if(prof->total_accesses == 0) return;\n"
           << "    \n"
           << "    // 计算变量大小（以Bytes为单位）\n"
           << "    prof->var_size = (prof->end_addr - prof->base_addr + prof->type_size);\n"
           << "    \n"
           << "    // 选择排序，按照pattern_counts从大到小排序，同时调整patterns数组\n"
           << "    for(i = 0; i < MEM_TOP_PATTERNS && i < MEM_MAX_PATTERNS - 1; i++) {\n"
           << "        int max_idx = i;\n"
           << "        for(j = i + 1; j < MEM_MAX_PATTERNS; j++) {\n"
           << "            if(prof->pattern_counts[j] > prof->pattern_counts[max_idx]) {\n"
           << "                max_idx = j;\n"
           << "            }\n"
           << "        }\n"
           << "        if(max_idx != i) {\n"
           << "            // 交换pattern_counts\n"
           << "            size_t temp_count = prof->pattern_counts[i];\n"
           << "            prof->pattern_counts[i] = prof->pattern_counts[max_idx];\n"
           << "            prof->pattern_counts[max_idx] = temp_count;\n"
           << "            \n"
           << "            // 同步交换patterns\n"
           << "            size_t temp_pattern = prof->patterns[i];\n"
           << "            prof->patterns[i] = prof->patterns[max_idx];\n"
           << "            prof->patterns[max_idx] = temp_pattern;\n"
           << "        }\n"
           << "    }\n"
           << "}\n\n"
           << "// 打印分析结果\n"
           << "static inline void __mem_print_analysis(mem_profile_t* prof) {\n"
           << "    if(prof->total_accesses == 0) return;\n"
           << "    \n"
           << "    // 创建输出缓冲区\n"
           << "    char buffer[512];\n"
           << "    int offset = 0;\n"
           << "    \n"
           << "    // 写入基本信息\n"
           << "    offset += snprintf(buffer + offset, sizeof(buffer) - offset,\n"
           << "        \"[Memory Analysis] thread %d: %s in %s: elements=%zu, accesses=%zu\\n\",\n"
           << "        get_thread_id(), prof->var_name, prof->func_name, prof->var_size, prof->total_accesses);\n"
           << "    \n"
           << "    // 输出主要访存模式\n"
           << "    for(int i = 0; i < MEM_TOP_PATTERNS && i < MEM_MAX_PATTERNS; i++) {\n"
           << "        if(prof->pattern_counts[i] > prof->total_accesses * 5 / 100) {\n"
           << "            offset += snprintf(buffer + offset, sizeof(buffer) - offset,\n"
           << "                \"  Pattern %d: step=%zu (%.1f%%)\\n\",\n"
           << "                i + 1,\n"
           << "                prof->patterns[i],\n"
           << "                (float)prof->pattern_counts[i] * 100 / prof->total_accesses);\n"
           << "        }\n"
           << "    }\n"
           << "    \n"
           << "    // 一次性输出所有内容\n"
           << "    hthread_printf(\"%s\", buffer);\n"
           << "}\n\n";
        return ss.str();
    }

    // 生成完整的访存分析器代码
    static std::string generateCompleteProfiler(const std::vector<std::string> &includes)
    {
        return generateBaseStructures(includes) + generateInitFunction() + generateRecordFunction() +
               generateAnalysisFunction();
    }
};

#endif // MEMORY_PROFILER_H