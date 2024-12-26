#ifndef FRONTENDACTION_H
#define FRONTENDACTION_H

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/Tooling.h"
#include "TimeInstrumentation.h"
#include <memory>

using namespace clang;

class IncludeTracker : public PPCallbacks {
public:
    explicit IncludeTracker(SourceManager &SM, std::vector<std::string>& includes)
        : SM(SM), includes(includes) {}

    void InclusionDirective(SourceLocation HashLoc,
                           const Token &IncludeTok,
                           StringRef FileName,
                           bool IsAngled,
                           CharSourceRange FilenameRange,
                           OptionalFileEntryRef File,
                           StringRef SearchPath,
                           StringRef RelativePath,
                           const Module *SuggestedModule,
                           bool ModuleImported,
                           SrcMgr::CharacteristicKind FileType) override {
        // 只处理主文件中的包含指令
        if (SM.isInMainFile(HashLoc)) {
            includes.push_back(FileName.str());
        }
    }

private:
    SourceManager &SM;
    std::vector<std::string>& includes;  // 直接引用外部的 includes
};

class InstrumentationFrontendAction : public clang::ASTFrontendAction
{
public:
    InstrumentationFrontendAction() = default;

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance &CI, llvm::StringRef file) override;

    bool BeginSourceFileAction(clang::CompilerInstance &CI) override;

    void EndSourceFileAction() override;

    const std::vector<std::string> &getIncludes() const { return includes; }

private:
    clang::Rewriter rewriter;
    std::unique_ptr<IncludeTracker> includeTracker;
    std::vector<std::string> includes; // 存储头文件列表
};

class InstrumentationFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
public:
    std::unique_ptr<clang::FrontendAction> create() override
    {
        return std::make_unique<InstrumentationFrontendAction>();
    }
};

#endif //FRONTENDACTION_H
