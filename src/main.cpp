#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/CommandLine.h"
#include "../include/CommandLineOptions.h"
#include "../include/FrontendAction.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;

int main(int argc, const char **argv) {
    // 解析命令行参数
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    // 检查是否指定了任何插桩类型
    if (!EnableTimeInst && !EnableMemoryInst) {
        llvm::errs() << "Error: Must specify at least one instrumentation type "
                     << "(-time-inst or -memory-inst)\n";
        return 1;
    }

    // 检查是否同时指定了多个插桩类型
    if (EnableTimeInst && EnableMemoryInst) {
        llvm::errs() << "Error: Cannot enable both time and memory instrumentation simultaneously\n";
        return 1;
    }

    // 打印工具信息和配置
    llvm::outs() << "MT-3000 Source Code Instrumentation Tool\n";
    llvm::outs() << "======================================\n";
    if (EnableTimeInst) {
        llvm::outs() << "Mode: Time Instrumentation\n";
        llvm::outs() << "Settings:\n";
        llvm::outs() << "  - Total Time Threshold: " << TotalTimeThreshold << "%\n";
        llvm::outs() << "  - Parent Time Threshold: " << ParentTimeThreshold << "%\n";
    } else {
        llvm::outs() << "Mode: Memory Access Instrumentation\n";
    }
    llvm::outs() << "\n";

    // 运行工具
    return Tool.run(std::make_unique<InstrumentationFrontendActionFactory>().get());
}