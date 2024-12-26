// TimeInstrumentation.h
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

// AST访问器,用于遍历和插入计时代码
class TimeInstrumentationVisitor : public clang::RecursiveASTVisitor<TimeInstrumentationVisitor>
{
public:
    explicit TimeInstrumentationVisitor(clang::Rewriter &R, const CallGraph &CG, clang::ASTContext &Context, std::vector<std::string>& includes)
        : rewriter(R), callGraph(CG), currentFunction(nullptr), ASTCtx(Context), includes(includes)
    {
    }

    // 访问到翻译单元时触发
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl *TU);

    // 访问到函数声明时触发
    bool VisitFunctionDecl(clang::FunctionDecl *func);

    // 访问到return语句时触发
    bool VisitReturnStmt(clang::ReturnStmt *retStmt);

    // 访问到调用函数语句时触发
    bool VisitCallExpr(clang::CallExpr *call);

    // 生成结果处理代码
    std::string generateResultProcessing() const;

    // 重写 TraverseFunctionDecl 以跟踪当前函数
    bool TraverseFunctionDecl(clang::FunctionDecl *func);

private:
    clang::Rewriter &rewriter;
    const CallGraph &callGraph;
    std::unordered_set<std::string> alreadyDeclaredFuncs; // 记录已声明的函数
    clang::FunctionDecl *currentFunction; // 当前正在访问的函数
    clang::ASTContext &ASTCtx;
    std::vector<std::string>& includes;  // 存储头文件列表

    // 在文件开始处插入计时代码
    void insertFileEntryCode(clang::TranslationUnitDecl *TU);

    // 在函数开始处插入计时代码
    void insertFunctionEntryCode(clang::FunctionDecl *func);

    // 在return语句之前插入计时代码
    void insertReturnExitCode(clang::ReturnStmt *retStmt);

    // 在函数调用前后插入计时代码
    void insertCallTimingCode(clang::CallExpr *call, bool isStart);

    // 判断根函数是否需要插桩
    bool shouldRootFuncInstrument(const clang::FunctionDecl *func) const;

    // 判断调用函数是否需要插桩
    bool shouldCalleeFuncInstrument(const clang::FunctionDecl *callee) const;

    // 安全地插入代码的辅助方法
    bool safelyInsertText(clang::SourceLocation loc, const std::string &text, bool insertAfter = false);

    // 找到调用函数最开头
    clang::SourceLocation findStartOfCallStatement(const clang::CallExpr *call) const;
};

// AST消费者类,用于处理整个翻译单元
class TimeInstrumentationConsumer : public clang::ASTConsumer
{
public:
    explicit TimeInstrumentationConsumer(clang::Rewriter &R, std::vector<std::string>& includes) : rewriter(R), includes(includes)
    {
    }
    // 处理整个翻译单元
    void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
    clang::Rewriter &rewriter;
    CallGraph callGraph; // 存储函数调用关系图
    std::vector<std::string>& includes;  // 存储头文件列表
};

#endif // TIME_INSTRUMENTATION_H
