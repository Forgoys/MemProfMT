#include <memory>
#include "clang/AST/ASTConsumer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "../include/FrontendAction.h"
#include "../include/CommandLineOptions.h"
#include "../include/TimeInstrumentation.h"

std::unique_ptr<clang::ASTConsumer> InstrumentationFrontendAction::CreateASTConsumer(
        clang::CompilerInstance &CI, llvm::StringRef file) {
    rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    if (EnableTimeInst) {
        return std::make_unique<TimeInstrumentationConsumer>(rewriter, includes);
    }
    llvm::errs() << "Error: Memory instrumentation not implemented yet.\n";
    return nullptr;
}

bool InstrumentationFrontendAction::BeginSourceFileAction(clang::CompilerInstance &CI) {
    // 直接向预处理器添加回调，收集头文件信息
    CI.getPreprocessor().addPPCallbacks(std::make_unique<IncludeTracker>(CI.getSourceManager(), includes));
    return true;
}

void InstrumentationFrontendAction::EndSourceFileAction() {
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