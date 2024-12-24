#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/CommandLine.h"
#include "TimeInstrumentation.h"
#include <memory>
#include <string>

using namespace clang::tooling;
using namespace llvm;
using namespace clang;

// 命令行选项
static cl::OptionCategory ToolCategory("MT-3000 Instrumentation Tool Options");

static cl::opt<bool> EnableTimeInst(
    "time-inst",
    cl::desc("Enable time instrumentation"),
    cl::init(false),
    cl::cat(ToolCategory));

static cl::opt<bool> EnableMemoryInst(
    "memory-inst",
    cl::desc("Enable memory access instrumentation"),
    cl::init(false),
    cl::cat(ToolCategory));

static cl::opt<double> TotalTimeThreshold(
    "total-time-threshold",
    cl::desc("Threshold for total execution time percentage (default: 20%)"),
    cl::init(20.0),
    cl::cat(ToolCategory));

static cl::opt<double> ParentTimeThreshold(
    "parent-time-threshold",
    cl::desc("Threshold for parent function time percentage (default: 40%)"),
    cl::init(40.0),
    cl::cat(ToolCategory));

static cl::opt<std::string> OutputFilename(
    "o",
    cl::desc("Specify output filename"),
    cl::value_desc("filename"),
    cl::cat(ToolCategory));

class InstrumentationFrontendAction : public clang::ASTFrontendAction {
public:
    InstrumentationFrontendAction() {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance &CI, llvm::StringRef file) override {
        rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        if (EnableTimeInst) {
            return std::make_unique<TimeInstrumentationConsumer>(rewriter);
        }
        // TODO: 添加访存插桩的支持
        llvm::errs() << "Error: Memory instrumentation not implemented yet.\n";
        return nullptr;
    }

    void EndSourceFileAction() override {
        const auto &ID = rewriter.getSourceMgr().getMainFileID();
        std::string outputName;

        if (!OutputFilename.empty()) {
            outputName = OutputFilename;
        } else {
            auto inputPath = llvm::sys::path::filename(
                rewriter.getSourceMgr().getFilename(rewriter.getSourceMgr().getLocForStartOfFile(ID)));
            outputName = "instrumented_" + inputPath.str();
        }

        std::error_code EC;
        llvm::raw_fd_ostream outFile(outputName, EC, llvm::sys::fs::OF_None);

        if (EC) {
            llvm::errs() << "Error: Could not create output file " << outputName
                        << ": " << EC.message() << "\n";
            return;
        }

        // 获取重写后的代码
        const clang::RewriteBuffer *RewriteBuf = rewriter.getRewriteBufferFor(ID);
        if (RewriteBuf) {
            outFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
            llvm::outs() << "Successfully generated instrumented file: " << outputName << "\n";
        } else {
            llvm::errs() << "Error: No rewrite buffer for main file\n";
        }
    }

private:
    clang::Rewriter rewriter;
};

class InstrumentationFrontendActionFactory : public clang::tooling::FrontendActionFactory {
public:
    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<InstrumentationFrontendAction>();
    }
};

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