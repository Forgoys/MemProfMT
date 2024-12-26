#ifndef TIME_PROFILER_H
#define TIME_PROFILER_H

// 时间单位换算宏（时钟频率为4150MHz）
// #define CLK_FREQ 4150000000UL
// #define CYCLES_TO_NS(cycles) ((cycles) * 1000000000UL / CLK_FREQ)
// #define CYCLES_TO_US(cycles) ((cycles) * 1000000UL / CLK_FREQ)
// #define CYCLES_TO_MS(cycles) ((cycles) * 1000UL / CLK_FREQ)
#include <sstream>
#include "CallGraph.h"

// 用于生成计时代码的工具类
class TimingCodeGenerator
{
public:
    constexpr static unsigned NUM_THREADS = 24;
    constexpr static double DEFAULT_TOTAL_TIME_THRESHOLD = 20.0; // 20%
    constexpr static double DEFAULT_PARENT_TIME_THRESHOLD = 40.0; // 40%
    // constexpr unsigned long CLK_FREQ = 4150000000UL;      // 已在宏定义中定义，移除

    // 组合根函数的全局记录变量
    static std::string combineRootTimeVarName(const std::string &funcName)
    {
        return "__time_" + funcName;
    }

    // 组合调用函数的全局记录变量
    static std::string combineCalleeTimeVarName(const std::string &funcName, const std::string &calleeName)
    {
        return "__time_" + funcName + "_" + calleeName;
    }

    // 组合调用函数的临时记录变量
    static std::string combineCalleeTimeVarTmpName(const std::string &calleeName)
    {
        return "__time_" + calleeName + "_tmp";
    }

    // 时间计算的宏代码
    static std::string generateTimeCalcCode(const std::vector<std::string> &includes)
    {
        std::stringstream ss;
        bool hasLimitHeaderFile = false;
        for (const std::string &include: includes) {
            if (include == "limits.h") {
                hasLimitHeaderFile = true;
                break;
            }
        }
        if (!hasLimitHeaderFile) {
            ss << "#include <limits.h>\n";
        }
        // 时间单位换算宏（时钟频率为4150MHz）
        ss << "#define CLK_FREQ 4150000000UL\n"
                << "#define CYCLES_TO_NS(cycles) ((cycles) * 1000000000UL / CLK_FREQ)\n"
                << "#define CYCLES_TO_US(cycles) ((cycles) * 1000000UL / CLK_FREQ)\n"
                << "#define CYCLES_TO_MS(cycles) ((cycles) * 1000UL / CLK_FREQ)\n";
        return ss.str();
    }

    // 计时数组声明代码
    static std::string generateArrayDecls(const std::shared_ptr<CallGraphNode> &callGraphNode)
    {
        std::vector<std::string> varNames = callGraphNode->getTimeVarName();
        std::stringstream ss;
        for (const std::string &name: varNames) {
            ss << "static unsigned long " << name << "[" << NUM_THREADS << "] = {0};\n";
        }
        return ss.str();
    }

    // 函数入口处的计时代码
    static std::string generateFunctionEntryCode(const std::shared_ptr<CallGraphNode> &callGraphNode)
    {
        std::stringstream ss;
        ss << "\n\tint __tid = get_thread_id();\n";
        // 累计时间的临时变量
        for (const auto &callee: callGraphNode->getCallees()) {
            ss << "\tunsigned long " << combineCalleeTimeVarTmpName(callee->getName()) << " = 0;\n";
        }
        ss << "\tunsigned long __start_time = get_clk();\n";
        return ss.str();
    }

    // 函数出口处的计时代码
    static std::string generateFunctionExitCode(const std::string &funcName,
                                                const std::shared_ptr<CallGraphNode> &callGraphNode)
    {
        std::stringstream ss;
        ss << "\n";
        // 根函数部分：
        ss << "\tunsigned long __end_time = get_clk();\n"
                << "\t__time_" + funcName + "[__tid] += __end_time - __start_time;\n";

        // 调用函数部分：把累计变量里的值记录回全局变量里
        for (const auto &callee: callGraphNode->getCallees()) {
            const std::string calleeName = callee->getName();
            ss << "\t" << combineCalleeTimeVarName(funcName, calleeName) << "[__tid] = " <<
                    combineCalleeTimeVarTmpName(calleeName) << ";\n";
        }

        return ss.str();
    }

    // 函数调用前的计时函数
    static std::string generateCallBeforeCode(const std::string &calleeName)
    {
        return "unsigned long __call_start_" + calleeName + " = get_clk();\n";
    }

    // 函数调用后的计时函数
    static std::string generateCallAfterCode(const std::string &calleeName)
    {
        std::stringstream ss;
        ss << "\nunsigned long __call_end_" + calleeName + " = get_clk();\n"
                << combineCalleeTimeVarTmpName(calleeName) + " += (__call_end_" + calleeName + " - __call_start_" +
                calleeName + ");\n";
        return ss.str();
    }

    // 生成获取函数总时间的函数
    static std::string generateGetTotalTimeFunc()
    {
        std::stringstream ss;
        ss << "static inline void __combine_thread_times(unsigned long time_array[" << NUM_THREADS << "], "
                << "unsigned long* total_time) {\n"
                << "\t*total_time = ULONG_MAX;\n"
                << "\tfor(int i = 0; i < " << NUM_THREADS << "; i++) {\n"
                << "\t\tif (time_array[i] != 0) {\n"
                << "\t\t\t*total_time = *total_time < time_array[i] ? *total_time : time_array[i];\n"
                << "\t\t}\n"
                << "\t}\n"
                << "}\n\n";
        return ss.str();
    }

    // 生成等待同步的函数
    static std::string generateSynchronizationCode()
    {
        std::stringstream ss;
        ss << "static inline void __wait_for_threads() {\n"
                << "    if (get_thread_id() == 0) {\n"
                << "        const unsigned long start_wait = get_clk();\n"
                << "        // 等待3秒\n"
                << "        while ((get_clk() - start_wait) < (3UL * 4150000000UL)) {}\n"
                << "        hthread_printf(\"\\nProcessing timing results...\\n\");\n"
                << "    }\n"
                << "}\n\n";
        return ss.str();
    }

    // 生成结果处理函数的头部代码
    static std::string generateResultsHeader()
    {
        std::stringstream ss;
        ss << "void __print_timing_results() {\n"
                << "    __wait_for_threads();\n"
                << "    if (get_thread_id() == 0) {\n"
                << "        unsigned long total_program_time = 0;\n\n";
        return ss.str();
    }

    // 生成时间合并代码
    static std::string generateTimeCombiningCode(const std::string &funcName,
                                                 const std::vector<std::shared_ptr<CallGraphNode> > &callees,
                                                 bool isRootFunction)
    {
        std::stringstream ss;
        // 合并函数自身的执行时间
        ss << "        unsigned long total_" << funcName << ";\n"
                << "        __combine_thread_times(__time_" << funcName
                << ", &total_" << funcName << ");\n";

        // 只有根函数的时间才计入总时间
        if (isRootFunction) {
            ss << "        total_program_time += total_" << funcName << ";\n";
        }

        // 合并该函数调用其他函数的时间
        for (const auto &callee: callees) {
            ss << "        unsigned long total_" << funcName << "_" << callee->getName() << ";\n"
                    << "        __combine_thread_times(__time_" << funcName << "_" << callee->getName()
                    << ", &total_" << funcName << "_" << callee->getName() << ");\n";
        }
        ss << "\n";
        return ss.str();
    }

    // 生成报告标题
    static std::string generateReportHeader()
    {
        std::stringstream ss;
        ss << "\n        hthread_printf(\"\\n═══════════════════════════════════════════════\\n\");\n"
                << "        hthread_printf(\"              Timing Analysis Report              \\n\");\n"
                << "        hthread_printf(\"═══════════════════════════════════════════════\\n\\n\");\n"
                << "        hthread_printf(\"Total Program Time: %.2f ms\\n\\n\", "
                << "CYCLES_TO_MS((double)total_program_time));\n";
        return ss.str();
    }

    // 生成单个函数的统计信息
    static std::string generateFunctionStats(const std::string &funcName, int level)
    {
        std::stringstream ss;
        // 打印缩进和函数名
        ss << "        {\n"
                << "            for(int i = 0; i < " << level << "; i++) hthread_printf(\"│  \");\n"
                << "            hthread_printf(\"" << funcName << ":\\n\");\n"
                << "            for(int i = 0; i < " << level << "; i++) hthread_printf(\"│  \");\n"
                << "            hthread_printf(\"  Total: %.2f ms (%.1f%% of total)\\n\", "
                << "CYCLES_TO_MS((double)total_" << funcName << "), "
                << "total_program_time > 0 ? "
                << "((double)total_" << funcName << " / ((double)total_program_time) * 100.0) : 0.0);\n"
                << "        }\n";
        return ss.str();
    }

    // 生成热点函数分析的头部
    static std::string generateHotFunctionsHeader()
    {
        std::stringstream ss;
        ss << "        hthread_printf(\"\\n═══════════════════════════════════════════════\\n\");\n"
                << "        hthread_printf(\"                  Hot Functions                  \\n\");\n"
                << "        hthread_printf(\"═══════════════════════════════════════════════\\n\\n\");\n";
        return ss.str();
    }

    // 生成热点函数检查代码
    static std::string generateHotFunctionCheck(const std::string &funcName,
                                                const std::vector<std::string> &callers)
    {
        std::stringstream ss;
        ss << "        {\n"
                << "            double percent_total = (double)total_" << funcName
                << " / (double)total_program_time * 100.0;\n"
                << "            double percent_parent = 0.0;\n";

        if (!callers.empty()) {
            for (const auto &caller: callers) {
                ss << "            percent_parent += ((double)total_" << funcName
                        << ") / ((double)total_" << caller << ") * 100.0;\n";
            }
            ss << "            percent_parent /= " << callers.size() << ";\n";
        }

        ss << "            if (percent_total >= " << DEFAULT_TOTAL_TIME_THRESHOLD
                << " && percent_parent >= " << DEFAULT_PARENT_TIME_THRESHOLD << ") {\n"
                << "                hthread_printf(\"%s: %.1f%% of total, %.1f%% of parent\\n\", \""
                << funcName << "\", percent_total, percent_parent);\n"
                << "            }\n"
                << "        }\n";
        return ss.str();
    }
};

#endif // TIME_PROFILER_H
