#include "TimeInstrumentation.h"
#include <sstream>
#include <functional>
#include <string>
#include <cstdio>

namespace {
    constexpr unsigned NUM_THREADS = 24;
    constexpr double DEFAULT_TOTAL_TIME_THRESHOLD = 0.20;    // 20%
    constexpr double DEFAULT_PARENT_TIME_THRESHOLD = 0.40;   // 40%

    // 生成记录时间的全局数组声明
    std::string generateTimeArrayDecl(const std::string& funcName) {
        std::stringstream ss;
        ss << "static unsigned long __time_" << funcName << "[" << NUM_THREADS << "] = {0};\n";
        ss << "static unsigned long __count_" << funcName << "[" << NUM_THREADS << "] = {0};\n";
        ss << "static unsigned long __call_time_" << funcName << "[" << NUM_THREADS << "] = {0};\n";
        return ss.str();
    }

    // 生成获取函数总时间的代码
    std::string generateGetTotalTimeFunc() {
        std::stringstream ss;
        ss << "static inline void __combine_thread_times(unsigned long time_array[" << NUM_THREADS << "], "
           << "unsigned long call_array[" << NUM_THREADS << "], "
           << "unsigned long count_array[" << NUM_THREADS << "], "
           << "unsigned long* total_time, unsigned long* total_calls, double* avg_time) {\n"
           << "    *total_time = 0;\n"
           << "    *total_calls = 0;\n"
           << "    for(int i = 0; i < " << NUM_THREADS << "; i++) {\n"
           << "        *total_time += time_array[i];\n"
           << "        *total_calls += count_array[i];\n"
           << "    }\n"
           << "    if (*total_calls > 0) {\n"
           << "        *avg_time = (double)*total_time / *total_calls;\n"
           << "    } else {\n"
           << "        *avg_time = 0.0;\n"
           << "    }\n"
           << "}\n\n";
        return ss.str();
    }

    // 生成等待同步的代码
    std::string generateSynchronizationCode() {
        std::stringstream ss;
        ss << "static inline void __wait_for_threads() {\n"
           << "    if (get_thread_id() == 0) {\n"
           << "        const unsigned long start_wait = get_clk();\n"
           << "        // 等待3秒\n"
           << "        while ((get_clk() - start_wait) < (3UL * " << CLK_FREQ << ")) {}\n"
           << "        hthread_printf(\"\\nProcessing timing results...\\n\");\n"
           << "    }\n"
           << "}\n\n";
        return ss.str();
    }
}

bool TimeInstrumentationVisitor::shouldInstrument(const clang::FunctionDecl* func) const {
    if (!func->hasBody()) return false;
    return true;  // 暂时默认所有函数都需要插桩
}

void TimeInstrumentationVisitor::insertTimingCode(clang::FunctionDecl* func, bool isStart) {
    std::string funcName = func->getNameAsString();
    std::stringstream code;

    if (isStart) {
        code << "{\n"
             << "    const unsigned tid = get_thread_id();\n"
             << "    const unsigned long start_time = get_clk();\n";
    } else {
        code << "{\n"
             << "    const unsigned tid = get_thread_id();\n"
             << "    const unsigned long end_time = get_clk();\n"
             << "    const unsigned long elapsed = end_time - start_time;\n"
             << "    __time_" << funcName << "[tid] += elapsed;\n"
             << "    __call_time_" << funcName << "[tid] = elapsed;\n"
             << "    __count_" << funcName << "[tid]++;\n"
             << "}\n";
    }

    if (isStart) {
        auto loc = func->getBody()->getBeginLoc();
        loc = loc.getLocWithOffset(1);
        rewriter.InsertText(loc, code.str(), true, true);
    } else {
        auto loc = func->getBody()->getEndLoc();
        loc = loc.getLocWithOffset(-1);
        rewriter.InsertText(loc, code.str(), true, true);
    }
}

void TimeInstrumentationVisitor::insertCallTimingCode(clang::CallExpr* call, bool isStart) {
    if (const auto* callee = call->getDirectCallee()) {
        if (!shouldInstrument(callee)) return;

        std::string calleeName = callee->getNameAsString();
        std::stringstream code;

        if (isStart) {
            code << "{\n"
                 << "    const unsigned tid = get_thread_id();\n"
                 << "    const unsigned long call_start = get_clk();\n";
        } else {
            code << "{\n"
                 << "    const unsigned tid = get_thread_id();\n"
                 << "    const unsigned long call_end = get_clk();\n"
                 << "    __time_" << calleeName << "[tid] += call_end - call_start;\n"
                 << "}\n";
        }

        auto loc = isStart ? call->getBeginLoc() : call->getEndLoc();
        rewriter.InsertText(loc, code.str(), true, true);
    }
}

bool TimeInstrumentationVisitor::VisitFunctionDecl(clang::FunctionDecl* func) {
    if (!shouldInstrument(func)) return true;

    std::string funcName = func->getNameAsString();
    if (!alreadyDeclaredFuncs.count(funcName)) {
        rewriter.InsertText(func->getBeginLoc(), generateTimeArrayDecl(funcName), true, true);
        alreadyDeclaredFuncs.insert(funcName);
    }

    insertTimingCode(func, true);  // 函数开始
    insertTimingCode(func, false); // 函数结束

    return true;
}

bool TimeInstrumentationVisitor::VisitCallExpr(clang::CallExpr* call) {
    if (const auto* callee = call->getDirectCallee()) {
        if (!shouldInstrument(callee)) return true;

        insertCallTimingCode(call, true);
        insertCallTimingCode(call, false);
    }
    return true;
}

std::string TimeInstrumentationVisitor::generateResultProcessing() const {
    std::stringstream ss;

    // 生成辅助函数
    ss << generateGetTotalTimeFunc();
    ss << generateSynchronizationCode();

    // 生成结果处理函数
    ss << "void __print_timing_results() {\n"
       << "    __wait_for_threads();\n"
       << "    if (get_thread_id() == 0) {\n"
       << "        unsigned long total_program_time = 0;\n\n";

    // 为每个函数生成时间统计变量
    for (const auto& funcName : alreadyDeclaredFuncs) {
        ss << "        unsigned long total_" << funcName << ", calls_" << funcName << ";\n"
           << "        double avg_" << funcName << ";\n"
           << "        __combine_thread_times(__time_" << funcName << ", __call_time_" << funcName
           << ", __count_" << funcName << ", &total_" << funcName << ", &calls_" << funcName
           << ", &avg_" << funcName << ");\n\n";

        if (callGraph.getRootFunctions().size() == 1 &&
            callGraph.getRootFunctions()[0] == funcName) {
            ss << "        total_program_time = total_" << funcName << ";\n";
        }
    }

    // 输出统计信息头部
    ss << "\n        hthread_printf(\"\\n═══════════════════════════════════════════════\\n\");\n"
       << "        hthread_printf(\"              Timing Analysis Report              \\n\");\n"
       << "        hthread_printf(\"═══════════════════════════════════════════════\\n\\n\");\n"
       << "        hthread_printf(\"Total Program Time: %.2f ms\\n\\n\", "
       << "CYCLES_TO_MS((double)total_program_time));\n";

    // 层次化输出函数调用关系和时间统计
    std::function<void(const std::string&, int, unsigned long)> printFuncStats;
    printFuncStats = [&](const std::string& funcName, int level, unsigned long parentTime) {
        ss << "        {\n"
           << "            for(int i = 0; i < " << level << "; i++) "
           << "hthread_printf(level == " << level << " ? \"├─ \" : \"│  \");\n"
           << "            const double time_ms = CYCLES_TO_MS((double)total_" << funcName << ");\n"
           << "            const double percent_total = total_program_time > 0 ? "
           << "((double)total_" << funcName << " / total_program_time) * 100.0 : 0.0;\n"
           << "            const double percent_parent = parentTime > 0 ? "
           << "((double)total_" << funcName << " / parentTime) * 100.0 : 0.0;\n"
           << "            const double avg_time_us = CYCLES_TO_US(avg_" << funcName << ");\n"
           << "            hthread_printf(\"" << funcName << ":\\n\");\n"
           << "            for(int i = 0; i < " << level << "; i++) hthread_printf(\"│  \");\n"
           << "            hthread_printf(\"  Total: %.2f ms (%.1f%% of total\", "
           << "time_ms, percent_total);\n"
           << "            if(" << level << " > 0) hthread_printf(\", %.1f%% of parent\", "
           << "percent_parent);\n"
           << "            hthread_printf(\"\\n\");\n"
           << "            for(int i = 0; i < " << level << "; i++) hthread_printf(\"│  \");\n"
           << "            hthread_printf(\"  Calls: %lu (avg: %.2f µs/call)\\n\", "
           << "calls_" << funcName << ", avg_time_us);\n"
           << "        }\n\n";

        // 递归处理被调用函数
        auto node = callGraph.getNode(funcName);
        if (node) {
            auto callees = node->getCallees();
            std::sort(callees.begin(), callees.end(),
                     [](const auto& a, const auto& b) {
                         return a->getName() < b->getName();
                     });

            for (const auto& callee : callees) {
                if (alreadyDeclaredFuncs.count(callee->getName())) {
                    ss << "            " << funcName << "_total = total_" << funcName << ";\n";
                    printFuncStats(callee->getName(), level + 1, parentTime);
                }
            }
        }
    };

    // 从根函数开始打印
    for (const auto& rootFunc : callGraph.getRootFunctions()) {
        if (alreadyDeclaredFuncs.count(rootFunc)) {
            printFuncStats(rootFunc, 0, 0);
        }
    }

    // 输出热点函数部分
    ss << "\n        hthread_printf(\"\\n═══════════════════════════════════════════════\\n\");\n"
       << "        hthread_printf(\"                  Hot Functions                  \\n\");\n"
       << "        hthread_printf(\"═══════════════════════════════════════════════\\n\\n\");\n";

    std::vector<std::pair<std::string, std::pair<double, double>>> hotFuncs;
    for (const auto& funcName : alreadyDeclaredFuncs) {
        if (funcName == callGraph.getRootFunctions()[0]) continue;

        ss << "        {\n"
           << "            double percent_total = ((double)total_" << funcName << " / total_program_time) * 100.0;\n";

        for (const auto& caller : callGraph.getCallers(funcName)) {
            if (alreadyDeclaredFuncs.count(caller)) {
                ss << "            double percent_parent = ((double)total_" << funcName
                   << " / total_" << caller << ") * 100.0;\n"
                   << "            if (percent_total >= " << DEFAULT_TOTAL_TIME_THRESHOLD * 100.0
                   << " && percent_parent >= " << DEFAULT_PARENT_TIME_THRESHOLD * 100.0 << ") {\n"
                   << "                hotFuncs.emplace_back(\"" << funcName << "\", "
                   << "std::make_pair(percent_total, percent_parent));\n"
                   << "                break;\n"
                   << "            }\n";
            }
        }
        ss << "        }\n";
    }

    ss << "    }\n"  // 结束tid==0的判断
       << "}\n";     // 结束函数

    return ss.str();
}

void TimeInstrumentationConsumer::HandleTranslationUnit(clang::ASTContext& Context) {
    // 构建调用图
    CallGraphBuilder graphBuilder(callGraph);
    graphBuilder.TraverseDecl(Context.getTranslationUnitDecl());

    // 创建访问器并遍历AST
    TimeInstrumentationVisitor visitor(rewriter, callGraph);
    visitor.TraverseDecl(Context.getTranslationUnitDecl());

    // 在文件末尾添加时间统计处理代码
    auto loc = rewriter.getSourceMgr().getLocForEndOfFile(rewriter.getSourceMgr().getMainFileID());
    rewriter.InsertText(loc, visitor.generateResultProcessing(), true, true);
}