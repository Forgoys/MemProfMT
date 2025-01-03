#ifndef MEMORY_INSTRUMENTATION_H
#define MEMORY_INSTRUMENTATION_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <unordered_set>
#include <string>

// AST访问器,用于遍历和插入内存访问分析代码
class MemoryInstrumentationVisitor : public clang::RecursiveASTVisitor<MemoryInstrumentationVisitor> {
public:
    explicit MemoryInstrumentationVisitor(clang::Rewriter &R, clang::ASTContext &Context)
        : rewriter(R), context(Context) {}

    // 访问到数组声明时触发
    bool VisitArrayType(clang::ArrayType *arrayType);

    // 访问变量声明时触发
    bool VisitVarDecl(clang::VarDecl *varDecl);

    // 访问数组或指针访问表达式时触发
    bool VisitArraySubscriptExpr(clang::ArraySubscriptExpr *arrayExpr);
    bool VisitMemberExpr(clang::MemberExpr *memberExpr);
    bool VisitUnaryOperator(clang::UnaryOperator *unaryOp);

    // 访问函数声明时触发
    bool VisitFunctionDecl(clang::FunctionDecl *funcDecl);

private:
    clang::Rewriter &rewriter;
    clang::ASTContext &context;
    std::unordered_set<std::string> instrumentedVars;

    // 在变量声明处插入初始化代码
    void insertInitialization(clang::VarDecl *varDecl);

    // 在内存访问处插入记录代码
    void insertAccessRecord(clang::Expr *expr, const std::string &varName);

    // 在函数结尾处插入统计代码
    void insertStatistics(clang::FunctionDecl *funcDecl);

    // 辅助方法：安全插入代码
    bool safelyInsertText(clang::SourceLocation loc, const std::string &text, bool insertAfter = false);

    // 辅助方法：获取表达式的完整源代码
    std::string getExprAsString(const clang::Expr *expr);
};

// AST消费者类,用于处理整个翻译单元
class MemoryInstrumentationConsumer : public clang::ASTConsumer {
public:
    explicit MemoryInstrumentationConsumer(clang::Rewriter &R) : rewriter(R) {}

    void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
    clang::Rewriter &rewriter;
};

#endif // MEMORY_INSTRUMENTATION_H