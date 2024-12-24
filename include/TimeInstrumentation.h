#ifndef TIME_INSTRUMENTATION_H
#define TIME_INSTRUMENTATION_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "CallGraph.h"
#include "../runtime/TimeProfiler.h"
#include <unordered_set>

class TimeInstrumentationVisitor : public clang::RecursiveASTVisitor<TimeInstrumentationVisitor> {
public:
    explicit TimeInstrumentationVisitor(clang::Rewriter& R, const CallGraph& CG)
        : rewriter(R), callGraph(CG) {}

    // 必要的AST访问函数
    bool shouldVisitImplicitCode() const { return false; }
    bool shouldVisitTemplateInstantiations() const { return false; }
    bool shouldTraversePostOrder() const { return true; }

    // 访问函数声明
    bool VisitFunctionDecl(clang::FunctionDecl* func);

    // 访问函数调用
    bool VisitCallExpr(clang::CallExpr* call);

    // 结果生成
    std::string generateResultProcessing() const;

private:
    clang::Rewriter& rewriter;
    const CallGraph& callGraph;
    std::unordered_set<std::string> alreadyDeclaredFuncs;  // 跟踪已声明的函数

    // 辅助函数
    void insertTimingCode(clang::FunctionDecl* func, bool isStart);
    void insertCallTimingCode(clang::CallExpr* call, bool isStart);
    bool shouldInstrument(const clang::FunctionDecl* func) const;
};

class TimeInstrumentationConsumer : public clang::ASTConsumer {
public:
    explicit TimeInstrumentationConsumer(clang::Rewriter& R) : rewriter(R) {}

    void HandleTranslationUnit(clang::ASTContext& Context) override;

private:
    clang::Rewriter& rewriter;
    CallGraph callGraph;
};

#endif // TIME_INSTRUMENTATION_H