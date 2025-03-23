#include "../include/CommandLineOptions.h"
#include "../include/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;

int main(int argc, const char **argv)
{
    // 解析命令行参数
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    // 打印工具信息和配置
    llvm::outs() << "MT-3000 Source Code Instrumentation Tool\n";
    llvm::outs() << "======================================\n";
    llvm::outs() << "Mode: Memory Access Instrumentation\n";
    if (!TargetFunctions.empty()) {
        llvm::outs() << "Target Functions:\n";
        for (const auto &func : TargetFunctions) {
            llvm::outs() << "  - " << func << "\n";
        }
    } else {
        llvm::outs() << "Target: All Functions\n";
    }
    // }
    llvm::outs() << "======================================\n";

    // 运行工具
    return Tool.run(std::make_unique<InstrumentationFrontendActionFactory>().get());
}