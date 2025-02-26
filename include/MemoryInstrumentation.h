#ifndef MEMORY_INSTRUMENTATION_H
#define MEMORY_INSTRUMENTATION_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include "../runtime/MemoryProfiler.h"
#include "CallGraph.h"
#include <unordered_set>
#include <clang/AST/ParentMap.h>

class MemoryInstrumentationVisitor; // 前向声明

// AST访问器，用于遍历和插入内存访问监控代码
class MemoryInstrumentationVisitor
        : public clang::RecursiveASTVisitor<MemoryInstrumentationVisitor>
{
public:
    friend class clang::RecursiveASTVisitor<MemoryInstrumentationVisitor>;

    explicit MemoryInstrumentationVisitor(clang::Rewriter &R,
                                          clang::ASTContext &Context,
                                          std::vector<std::string> &includes,
                                          const std::vector<std::string> targetFuncs)
        : rewriter(R), ctx(Context), includes(includes),
          targetFunctions(),
          currentFunctionName("")
    {
        for (const auto& func : targetFuncs) {
            if (!func.empty()) {
                targetFunctions.insert(func);
            }
        }
    }

    // 控制遍历行为
    bool shouldVisitTemplateInstantiations() const { return false; }
    bool shouldVisitImplicitCode() const { return false; }

    // 访问翻译单元，插入内存分析器的定义
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl *TU);

    // 访问函数声明，记录当前函数名
    bool VisitFunctionDecl(clang::FunctionDecl *FD);

    // 访问变量声明，对数组和指针类型变量进行初始化
    bool VisitVarDecl(clang::VarDecl *VD);

    // 访问数组下标表达式，记录数组访问
    bool VisitArraySubscriptExpr(clang::ArraySubscriptExpr *ASE) const;

    // 访问一元运算符，处理指针解引用
    bool VisitUnaryOperator(clang::UnaryOperator *UO);

    // 访问结构体成员表达式，记录成员访问
    bool VisitMemberExpr(clang::MemberExpr *ME) const;

    // 遍历函数声明，保存当前函数上下文，清空当前函数的变量列表，
    // 遍历完成后恢复之前的函数上下文
    bool TraverseFunctionDecl(clang::FunctionDecl *FD);

    // 访问返回语句，插入内存分析代码
    bool VisitReturnStmt(clang::ReturnStmt* RS);

    bool VisitCompoundStmt(clang::CompoundStmt *CS);

    // 获取所有函数中已初始化的变量
    const std::unordered_map<std::string, std::unordered_set<std::string>>& getInitializedVars() const {
        return functionInitializedVars;
    }

private:
    clang::Rewriter &rewriter;
    clang::ASTContext &ctx;
    std::vector<std::string> &includes;
    std::unordered_set<std::string> instrumentedVars;
    std::unordered_set<std::string> targetFunctions; // 目标函数集合
    std::string currentFunctionName; // 当前正在访问的函数名
    clang::FunctionDecl* currentFunctionDecl = nullptr;
    std::unordered_map<std::string, std::vector<std::string>> functionVars; // Track variables per function
    std::unordered_map<std::string, std::unordered_set<std::string>> functionInitializedVars; // Track initialized variables per function

    // 获取表达式的源代码
    std::string getSourceText(const clang::Stmt *stmt) const;

    // 判断变量是否需要进行内存访问分析
    bool shouldInstrumentVar(const clang::VarDecl *VD) const;

    // 判断当前函数是否需要插桩
    bool shouldInstrumentFunction() const;

    // 为变量插入内存分析器的初始化代码
    void insertVarProfiler(const clang::VarDecl *VD);

    // 处理函数参数中的数组初始化, 遍历函数的所有参数，并调用 insertVarProfiler 处理数组类型的参数
    void insertFuncParamProfiler(const clang::FunctionDecl *FD);

    // 插入内存访问记录代码
    bool insertAccessProfiler(const clang::Expr *E) const;

    // 检查代码位置是否在主文件中
    bool isInMainFile(clang::SourceLocation Loc) const;

    void insertAnalysisCode(clang::ReturnStmt* RS);

    // 访问数组下标表达式，记录数组访问
    bool handleArraySubscriptExpr(const clang::ArraySubscriptExpr *ASE) const;

    // 访问一元运算符，处理指针解引用
    bool handleUnaryOperator(const clang::UnaryOperator *UO) const;

    unsigned getIndentation(clang::SourceLocation Loc) const;

    std::string getLine(clang::SourceLocation Loc) const;

    std::string generateAnalysisCode(const std::string& functionName);
};

// AST消费者类，用于处理整个翻译单元
class MemoryInstrumentationConsumer : public clang::ASTConsumer {
private:
    const std::vector<std::string> targetFunctions;
    clang::Rewriter &rewriter;
    std::vector<std::string> &includes;

public:
    explicit MemoryInstrumentationConsumer(clang::Rewriter &R,
                                         std::vector<std::string> &includes,
                                         const std::vector<std::string> &targetFuncs)
        : targetFunctions(targetFuncs),
          rewriter(R), includes(includes) {}
    
    void HandleTranslationUnit(clang::ASTContext &Context) override;
};

#endif // MEMORY_INSTRUMENTATION_H
