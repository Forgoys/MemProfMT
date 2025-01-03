#ifndef TIME_PROFILER_H
#define TIME_PROFILER_H

#include <sstream>
#include "CallGraph.h"

// 用于生成计时代码的工具类
class TimingCodeGenerator
{
public:
    constexpr static unsigned NUM_THREADS = 24;
    constexpr static double DEFAULT_TOTAL_TIME_THRESHOLD = 20.0; // 20%
    constexpr static double DEFAULT_PARENT_TIME_THRESHOLD = 40.0; // 40%

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
        for (const std::string &include : includes) {
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
        for (const std::string &name : varNames) {
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
        for (const auto &callee : callGraphNode->getCallees()) {
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
        ss << "\n\tunsigned long __end_time = get_clk();\n"
           << "\tunsigned long __total_time = __end_time - __start_time;\n"
           << "\t__time_" + funcName + "[__tid] += __total_time;\n\n"
           << "\t// 调整父函数时间，减去子函数的时间\n"
           << "\tunsigned long __children_time = 0;\n";

        // 累加所有子函数的时间
        for (const auto &callee : callGraphNode->getCallees()) {
            const std::string calleeName = callee->getName();
            ss << "\t" << combineCalleeTimeVarName(funcName, calleeName) << "[__tid] = "
               << combineCalleeTimeVarTmpName(calleeName) << ";\n"
               << "\t__children_time += " << combineCalleeTimeVarTmpName(calleeName) << ";\n";
        }

        // 调整当前函数的纯执行时间（不包括子函数时间）
        if (!callGraphNode->getCallees().empty()) {
            ss << "\n\t// 调整纯执行时间\n"
               << "\t__time_" + funcName + "[__tid] -= __children_time;\n";
        }

        return ss.str();
    }

    // 函数调用前的计时函数
    static std::string generateCallBeforeCode(const std::string &calleeName)
    {
        return "\tunsigned long __call_start_" + calleeName + " = get_clk();\n";
    }

    // 函数调用后的计时函数
    static std::string generateCallAfterCode(const std::string &calleeName)
    {
        std::stringstream ss;
        ss << "\tunsigned long __call_end_" + calleeName + " = get_clk();\n"
           << "\t" + combineCalleeTimeVarTmpName(calleeName) + " += (__call_end_" + calleeName + " - __call_start_" +
                calleeName + ");\n";
        return ss.str();
    }

    // 生成获取函数总时间的函数
    static std::string generateGetTotalTimeFunc()
    {
        std::stringstream ss;
        ss << "static inline void __combine_thread_times(unsigned long time_array[" << NUM_THREADS << "], "
           << "unsigned long* total_time) {\n"
           << "\t*total_time = 0;\n"
           << "\tfor(int i = 0; i < " << NUM_THREADS << "; i++) {\n"
           << "\t\tif (time_array[i] != 0) {\n"
           << "\t\t\t*total_time = *total_time < time_array[i] ? time_array[i] : *total_time;\n"
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
           << "\tif (get_thread_id() == 0) {\n"
           << "\t\tconst unsigned long start_wait = get_clk();\n"
           << "\t\t// 等待3秒\n"
           << "\t\twhile ((get_clk() - start_wait) < (3UL * CLK_FREQ)) {}\n"
           << "\t\ththread_printf(\"\\nProcessing timing results...\\n\");\n"
           << "\t}\n"
           << "}\n\n";
        return ss.str();
    }

    // 生成结果处理函数的头部代码
    static std::string generateResultsHeader()
    {
        std::stringstream ss;
        ss << "void __print_timing_results() {\n"
           << "\t__wait_for_threads();\n"
           << "\tif (get_thread_id() == 0) {\n"
           << "\t\tunsigned long total_program_time = 0;\n\n";
        return ss.str();
    }

    // 生成时间合并代码
    static std::string generateTimeCombiningCode(const std::string &funcName,
        const std::vector<std::shared_ptr<CallGraphNode>> &callees, const bool isRootFunction)
    {
        std::stringstream ss;
        // 合并函数自身的执行时间
        ss << "\t\tunsigned long total_" << funcName << ";\n"
           << "\t\t__combine_thread_times(__time_" << funcName
           << ", &total_" << funcName << ");\n";

        // 如果是根函数，加入到总程序时间中
        if (isRootFunction) {
            ss << "\t\ttotal_program_time += total_" << funcName << ";\n";
        }

        // 合并所有被调用函数的时间
        for (const auto &callee : callees) {
            ss << "\t\tunsigned long total_" << funcName << "_" << callee->getName() << ";\n"
               << "\t\t__combine_thread_times(__time_" << funcName << "_" << callee->getName()
               << ", &total_" << funcName << "_" << callee->getName() << ");\n";
        }
        ss << "\n";
        return ss.str();
    }

    // 生成报告标题
    static std::string generateReportHeader()
    {
        std::stringstream ss;
        ss << "\t\ththread_printf(\"\\n═══════════════════════════════════════════════\\n\");\n"
           << "\t\ththread_printf(\"              Timing Analysis Report              \\n\");\n"
           << "\t\ththread_printf(\"═══════════════════════════════════════════════\\n\\n\");\n"
           << "\t\ththread_printf(\"Total Program Time: %.2f ms\\n\\n\", "
           << "CYCLES_TO_MS((double)total_program_time));\n";
        return ss.str();
    }

    // 生成单个调用树的统计信息
    // static std::string generateCallTreeStats(const std::string &rootFunc,
    //         const CallGraph &callGraph, std::unordered_set<std::string> &processedFuncs)
    // {
    //     std::stringstream ss;
    //     // 辅助函数：递归生成调用树
    //     std::function<void(const std::string&, int, bool, const std::string&)> generateTree;
    //     generateTree = [&](const std::string &funcName, int level, bool isLast, const std::string &parentName) {
    //         auto node = callGraph.getNode(funcName);
    //         if (!node) return;
    //
    //         // 记录处理过的函数，避免重复处理
    //         if (processedFuncs.count(funcName) > 0) return;
    //         processedFuncs.insert(funcName);
    //
    //         // 根函数直接显示名称
    //         if (level == 0) {
    //             ss << "\t\ththread_printf(\"" << funcName << "\\n\");\n";
    //         } else {
    //             // 非根函数显示树形结构和时间占比
    //             ss << "\t\tfor (int i = 0; i < " << (level-1) << "; i++) "
    //                << "hthread_printf(\"│   \");\n"
    //                << "\t\ththread_printf(\"" << (isLast ? "└── " : "├── ") << "\");\n";
    //
    //             // 使用父函数对应的调用时间变量
    //             const std::string timeVarName = "total_" + parentName + "_" + funcName;
    //             ss << "\t\t{\n"
    //                << "\t\t\tdouble percent = ((double)" << timeVarName << " / (double)total_"
    //                << parentName << ") * 100.0;\n"
    //                << "\t\t\ththread_printf(\"" << funcName << ": %.2f ms (%.1f%% of "
    //                << parentName << ")\\n\", "
    //                << "CYCLES_TO_MS((double)" << timeVarName << "), percent);\n"
    //                << "\t\t}\n";
    //         }
    //
    //         // 递归处理所有被调用的函数
    //         const auto& callees = node->getCallees();
    //         for (size_t i = 0; i < callees.size(); ++i) {
    //             generateTree(callees[i]->getName(), level + 1, i == callees.size() - 1, funcName);
    //         }
    //     };
    //
    //     // 从根函数开始生成调用树
    //     generateTree(rootFunc, 0, true, "");
    //     ss << "\n";
    //     return ss.str();
    // }

    static std::string generateCallTreeStats(const std::string &funcName, const CallGraph &callGraph)
    {
        std::stringstream ss;
        ss << "\t\ththread_printf(\"" << funcName << "\\n\");\n";
        const auto& callees = callGraph.getNode(funcName)->getCallees();

        int count = 0;
        for (const auto &callee : callees) {
            count++;
            std::string calleeName = callee->getName();
            // 非根函数显示树形结构和时间占比
            ss << "\t\ththread_printf(\"│   \");\n"
               << "\t\ththread_printf(\"" << (count == callees.size() ? "└── " : "├── ") << "\");\n";

            // 使用父函数对应的调用时间变量
            const std::string timeVarName = "total_" + funcName + "_" + calleeName;
            ss << "\t\t{\n"
               << "\t\t\tdouble percent = ((double)" << timeVarName << " / (double)total_"
               << funcName << ") * 100.0;\n"
               << "\t\t\ththread_printf(\"" << calleeName << ": %.2f ms (%.1f%% of "
               << funcName << ")\\n\", "
               << "CYCLES_TO_MS((double)" << timeVarName << "), percent);\n"
               << "\t\t}\n";
        }
        ss << "\n";
        return ss.str();
    }

    // 生成热点函数分析
    static std::string generateHotFunctionAnalysis(const CallGraph &callGraph,
                                                 const std::unordered_set<std::string> &instrumentedFuncs)
    {
        std::stringstream ss;
        ss << "\t\ththread_printf(\"\\n═══════════════════════════════════════════════\\n\");\n"
           << "\t\ththread_printf(\"                  Hot Functions                  \\n\");\n"
           << "\t\ththread_printf(\"═══════════════════════════════════════════════\\n\\n\");\n"
           << "\t\ththread_printf(\"Functions that meet the following criteria:\\n\");\n"
           << "\t\ththread_printf(\"1. Root functions: total time >= %.1f%% of program time\\n\", "
           << DEFAULT_TOTAL_TIME_THRESHOLD << ");\n"
           << "\t\ththread_printf(\"2. Called functions: time >= %.1f%% of caller time\\n\\n\", "
           << DEFAULT_PARENT_TIME_THRESHOLD << ");\n";

        // 辅助函数：递归检查热点函数
        std::function<void(const std::string&, int, bool)> checkHotFunction;
        checkHotFunction = [&](const std::string &funcName, int level, bool isLast) {
            if (!instrumentedFuncs.count(funcName)) return;

            auto node = callGraph.getNode(funcName);
            if (!node) return;

            bool isRoot = callGraph.isRootFunction(funcName);
            bool isHot = false;

            ss << "\t\t{\n"
               << "\t\t\tdouble func_time_percent = ((double)total_" << funcName
               << " / (double)total_program_time) * 100.0;\n";

            if (isRoot) {
                // 根函数检查总体时间占比
                ss << "\t\tif (func_time_percent >= " << DEFAULT_TOTAL_TIME_THRESHOLD << ") {\n";
                isHot = true;
            } else {
                // 非根函数检查相对父函数的时间占比
                const auto& callers = callGraph.getCallers(funcName);
                if (!callers.empty()) {
                    ss << "\t\t\tdouble parent_time_percent = 0.0;\n";
                    for (const auto& caller : callers) {
                        ss << "\t\t\tparent_time_percent += ((double)total_" << funcName
                           << " / (double)total_" << caller << ") * 100.0;\n";
                    }
                    ss << "\t\t\tparent_time_percent /= " << callers.size() << ";\n"
                       << "\t\t\tif (parent_time_percent >= " << DEFAULT_PARENT_TIME_THRESHOLD << ") {\n";
                    isHot = true;
                }
            }

            if (isHot) {
                // 生成树形分支和热点函数信息
                if (level == 0) {
                    ss << "\t\t\ththread_printf(\"" << funcName << "\\n\");\n";
                } else {
                    // 缩进处理
                    ss << "\t\t\tfor (int i = 0; i < " << (level-1) << "; i++) "
                       << "hthread_printf(\"│   \");\n"
                       << "\t\t\ththread_printf(\"" << (isLast ? "└── " : "├── ") << "\");\n";
                }

                // 生成热点函数统计信息
                ss << "\t\t\t{\n"
                   << "\t\t\t\tdouble percent = " << (isRoot ?
                        "((double)total_" + funcName + " / (double)total_program_time) * 100.0" :
                        "parent_time_percent") << ";\n"
                   << "\t\t\t\ththread_printf(\"" << funcName << ": %.2f ms (%.1f%% of "
                   << (isRoot ? "total" : "parent") << ")\\n\", "
                   << "CYCLES_TO_MS((double)total_" << funcName << "), percent);\n"
                   << "\t\t\t}\n";

                // 递归检查被调用的函数
                const auto& callees = node->getCallees();
                for (size_t i = 0; i < callees.size(); ++i) {
                    checkHotFunction(callees[i]->getName(), level + 1, i == callees.size() - 1);
                }

                ss << "\t\t\t}\n";
            }

            ss << "\t\t}\n";
        };

        // 从根函数开始分析
        auto rootFuncs = callGraph.getRootFunctions();
        for (size_t i = 0; i < rootFuncs.size(); ++i) {
            checkHotFunction(rootFuncs[i], 0, i == rootFuncs.size() - 1);
        }

        return ss.str();
    }
};

#endif // TIME_PROFILER_H