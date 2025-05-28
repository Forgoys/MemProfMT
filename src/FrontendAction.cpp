#include <memory>
#include "clang/AST/ASTConsumer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "../include/FrontendAction.h"

#include <MemoryInstrumentation.h>

#include "../include/CommandLineOptions.h"
#include "../include/MemoryInstrumentation.h"

std::unique_ptr<clang::ASTConsumer> InstrumentationFrontendAction::CreateASTConsumer(
        clang::CompilerInstance &CI, llvm::StringRef file) {
    rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    // 获取目标函数列表
    std::vector<std::string> targetFuncs(TargetFunctions.begin(), TargetFunctions.end());

    // 如果指定了目标函数，打印相关信息
    if (!targetFuncs.empty()) {
        llvm::outs() << "Target functions for instrumentation:\n";
        for (const auto& func : targetFuncs) {
            llvm::outs() << "  - " << func << "\n";
        }
    }
    
    return std::make_unique<MemoryInstrumentationConsumer>(rewriter, includes, targetFuncs);
}

bool InstrumentationFrontendAction::BeginSourceFileAction(clang::CompilerInstance &CI) {
    // 直接向预处理器添加回调，收集头文件信息
    CI.getPreprocessor().addPPCallbacks(std::make_unique<IncludeTracker>(CI.getSourceManager(), includes));
    return true;
}

void InstrumentationFrontendAction::EndSourceFileAction() {
    // 获取主文件ID
    const auto &ID = rewriter.getSourceMgr().getMainFileID();
    std::string outputName;

    if (!OutputFilename.empty()) {
        // 如果用户通过-o选项指定了输出文件名，直接使用
        outputName = OutputFilename;
    } else {
        // 获取输入文件的完整路径
        llvm::StringRef inputFullPath = rewriter.getSourceMgr().getFilename(
            rewriter.getSourceMgr().getLocForStartOfFile(ID));
        
        // 分别获取目录路径和文件名
        llvm::SmallString<128> directory(llvm::sys::path::parent_path(inputFullPath));
        llvm::StringRef filename = llvm::sys::path::filename(inputFullPath);
        
        std::string prefix =  "mem_prof_";
        
        // 组合目录路径、前缀和文件名，构造完整的输出路径
        llvm::SmallString<128> outputPath(directory);
        llvm::sys::path::append(outputPath, prefix + filename.str());
        
        outputName = outputPath.str().str();
    }

    // 创建输出文件
    std::error_code EC;
    llvm::raw_fd_ostream outFile(outputName, EC, llvm::sys::fs::OF_None);

    // 检查文件创建是否成功
    if (EC) {
        llvm::errs() << "Error: Could not create output file " << outputName 
                     << ": " << EC.message() << "\n";
        return;
    }

    // 获取重写后的代码缓冲区
    const llvm::RewriteBuffer *RewriteBuf = rewriter.getRewriteBufferFor(ID);
    if (RewriteBuf) {
        // 将重写后的代码写入文件
        outFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
        llvm::outs() << "Successfully generated instrumented file: " << outputName << "\n";
    } else {
        llvm::errs() << "Error: No rewrite buffer for main file\n";
    }
}