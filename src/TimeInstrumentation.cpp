// TimeInstrumentation.cpp
#include "TimeInstrumentation.h"
#include <sstream>
#include <functional>
#include <string>
#include <clang/Lex/Lexer.h>

// 判断根函数是否需要插桩
bool TimeInstrumentationVisitor::shouldRootFuncInstrument(const clang::FunctionDecl *func) const
{
    // 如果这个函数只是一个声明
    if (!func || !func->hasBody()) return false;
    // 如果这个函数不在函数调用图中，不插桩
    if (!callGraph.getNode(func->getNameAsString())) return false;
    // 如果该函数为函数调用图中的叶子节点，即该函数再没有调用别的函数
    if (callGraph.isLeafFunction(func->getNameAsString())) return false;
    // 跳过系统头文件中的函数
    if (rewriter.getSourceMgr().isInSystemHeader(func->getLocation())) return false;
    // 跳过隐式生成的函数
    if (func->isImplicit()) return false;
    return true;
}

// 判断调用函数是否需要插桩
bool TimeInstrumentationVisitor::shouldCalleeFuncInstrument(const clang::FunctionDecl *callee) const
{
    // 如果这个函数只是一个声明
    if (!callee || !callee->hasBody()) return false;
    // 如果这个函数不在函数调用图中，不插桩
    if (!callGraph.getNode(callee->getNameAsString())) return false;
    // 跳过系统头文件中的函数
    if (rewriter.getSourceMgr().isInSystemHeader(callee->getLocation())) return false;
    // 跳过隐式生成的函数
    if (callee->isImplicit()) return false;
    return true;
}

// 安全地在指定位置插入代码
bool TimeInstrumentationVisitor::safelyInsertText(clang::SourceLocation loc, const std::string &text, bool insertAfter)
{
    if (loc.isInvalid()) return false;

    // 调整插入位置(如果是在某个位置之后插入)
    if (insertAfter) {
        loc = loc.getLocWithOffset(1);
    }

    // 验证插入位置是否在主文件中
    if (!rewriter.getSourceMgr().isWrittenInMainFile(loc)) return false;

    // 执行插入操作
    llvm::outs() << "\tInserting text at " << loc.printToString(rewriter.getSourceMgr()) << "\n";
    return rewriter.InsertText(loc, text, /*InsertAfter=*/false, /*IndentNewLines=*/true);
}

// 在文件开始处插入计时代码
void TimeInstrumentationVisitor::insertFileEntryCode(clang::TranslationUnitDecl *TU)
{
    // TranslationUnitDecl 是一个特殊的 Decl，代表整个翻译单元的根节点。它的 SourceLocation 通常是无效的
    llvm::outs() << "Instrumenting at the start of file:\n";
    const std::string code = TimingCodeGenerator::generateTimeCalcCode(includes);
    clang::SourceManager &SM = TU->getASTContext().getSourceManager();

    // 获取主文件的 FileID
    clang::FileID MainFileID = SM.getMainFileID();
    if (MainFileID.isInvalid()) {
        llvm::outs() << "Invalid main file ID\n";
        return;
    }

    // 获取主文件的开始位置
    clang::SourceLocation loc = SM.getLocForStartOfFile(MainFileID);
    if (loc.isInvalid()) {
        llvm::outs() << "Invalid location\n";
        return;
    }
    safelyInsertText(loc, code);
}

// 在函数开始处插入计时代码
void TimeInstrumentationVisitor::insertFunctionEntryCode(clang::FunctionDecl *func)
{
    llvm::outs() << "Instrumenting " + func->getNameAsString() + " at entry potion:\n";
    const std::string entryCode = TimingCodeGenerator::generateFunctionEntryCode(
        callGraph.getNode(func->getNameAsString()));

    if (!func->getBody()) return;

    clang::SourceLocation beginLoc = func->getBody()->getBeginLoc().getLocWithOffset(1); // '{' 的下一个位置
    safelyInsertText(beginLoc, entryCode);
}

// 在return语句之前插入计时代码
void TimeInstrumentationVisitor::insertReturnExitCode(clang::ReturnStmt *retStmt)
{
    if (!retStmt || !currentFunction) return;

    // 使用 currentFunction 跟踪当前函数
    if (!currentFunction) return;
    llvm::outs() << "Instrumenting " + currentFunction->getNameAsString() + " before return:\n";

    std::string funcName = currentFunction->getNameAsString();
    std::string exitCode = TimingCodeGenerator::generateFunctionExitCode(funcName, callGraph.getNode(funcName));

    clang::SourceLocation retLoc = retStmt->getBeginLoc();
    safelyInsertText(retLoc, exitCode);
}

// 在函数调用前后插入计时代码
void TimeInstrumentationVisitor::insertCallTimingCode(clang::CallExpr *call, bool isStart)
{
    llvm::outs() << "Instrumenting call" << (isStart ? "start: " : "end: ") << call->getDirectCallee()->
            getNameAsString() << ":\n";
    if (const auto *callee = call->getDirectCallee()) {
        std::string calleeName = callee->getNameAsString();
        clang::SourceLocation loc;
        std::string code;

        if (isStart) {
            // 在函数调用前插入代码
            loc = findStartOfCallStatement(call);
            code = TimingCodeGenerator::generateCallBeforeCode(calleeName);
        } else {
            // 在函数调用后插入代码
            loc = clang::Lexer::getLocForEndOfToken(
                call->getEndLoc(), 0, rewriter.getSourceMgr(), rewriter.getLangOpts());
            code = TimingCodeGenerator::generateCallAfterCode(calleeName);
        }

        safelyInsertText(loc, code, /*insertAfter=*/!isStart);
    }
}

// 重写 TraverseFunctionDecl 以跟踪当前函数
bool TimeInstrumentationVisitor::TraverseFunctionDecl(clang::FunctionDecl *func)
{
    if (!shouldRootFuncInstrument(func)) {
        return clang::RecursiveASTVisitor<TimeInstrumentationVisitor>::TraverseFunctionDecl(func);
    }
    llvm::outs() << "\n\n==========Traversing function: " << func->getNameAsString() << "===================\n\n";

    std::string funcName = func->getNameAsString();
    if (!alreadyDeclaredFuncs.count(funcName)) {
        // 在函数前添加计时数组声明
        std::string code = TimingCodeGenerator::generateArrayDecls(callGraph.getNode(funcName));
        llvm::outs() << "Instrumenting before " + func->getNameAsString() + ":\n";
        safelyInsertText(func->getBeginLoc(), code, false);
        alreadyDeclaredFuncs.insert(funcName);
    }

    // 跟踪当前函数
    currentFunction = func;

    // 在函数入口插入计时代码
    insertFunctionEntryCode(func);

    // 调用基类的默认遍历方法，继续递归遍历函数体，并触发自定义遍历类中重写的访问函数，进行插桩等逻辑
    bool result = clang::RecursiveASTVisitor<TimeInstrumentationVisitor>::TraverseFunctionDecl(func);

    // 重置当前函数
    currentFunction = nullptr;

    llvm::outs() << "\n==========Traversing function: " << func->getNameAsString() << "===================\n\n";
    return result;
}

clang::SourceLocation TimeInstrumentationVisitor::findStartOfCallStatement(const clang::CallExpr *call) const
{
    // 首先获取调用表达式的初始位置
    clang::SourceLocation loc = call->getBeginLoc();
    if (!loc.isValid()) {
        return loc;
    }

    // 获取源码管理器
    const clang::SourceManager &SM = rewriter.getSourceMgr();

    // 获取位置的详细信息
    std::pair<clang::FileID, unsigned> locInfo = SM.getDecomposedLoc(loc);

    // 文件大小检查
    if (locInfo.second >= SM.getFileIDSize(locInfo.first)) {
        return loc;
    }

    // 获取文件的文本缓冲区
    bool invalidFlag = false;
    const char *bufferStart = SM.getBufferData(locInfo.first, &invalidFlag).data();
    if (invalidFlag || !bufferStart) {
        return loc;
    }

    // 获取当前token的开始位置
    const char *tokenStart = bufferStart + locInfo.second;
    const char *originalStart = tokenStart;

    // 向前查找语句的真正开始位置
    while (tokenStart > bufferStart) {
        // 如果遇到这些字符，说明找到了上一个语句的结束
        if (*tokenStart == ';' || *tokenStart == '\n' || *tokenStart == '{' || *tokenStart == '}') {
            tokenStart++;
            // 跳过空白字符
            while (tokenStart < originalStart &&
                   (*tokenStart == ' ' || *tokenStart == '\t' || *tokenStart == '\n')) {
                tokenStart++;
            }
            break;
        }
        tokenStart--;
    }

    // 返回新的源码位置
    return SM.getLocForStartOfFile(locInfo.first).getLocWithOffset(tokenStart - bufferStart);
}

bool TimeInstrumentationVisitor::VisitTranslationUnitDecl(clang::TranslationUnitDecl *TU)
{
    insertFileEntryCode(TU);
    return true;
}

// 访问函数声明节点,插入计时代码
bool TimeInstrumentationVisitor::VisitFunctionDecl(clang::FunctionDecl *func)
{
    // 已在 TraverseFunctionDecl 中处理
    return true;
}

// 访问return语句,在其之前插入计时代码
bool TimeInstrumentationVisitor::VisitReturnStmt(clang::ReturnStmt *retStmt)
{
    insertReturnExitCode(retStmt);
    return true;
}

// 访问函数调用表达式,插入计时代码
bool TimeInstrumentationVisitor::VisitCallExpr(clang::CallExpr *call)
{
    if (const auto *callee = call->getDirectCallee()) {
        if (!shouldCalleeFuncInstrument(callee)) return true;

        // 在函数调用前后添加计时代码
        insertCallTimingCode(call, true); // 前
        insertCallTimingCode(call, false); // 后
    }
    return true;
}

// 生成结果处理代码
std::string TimeInstrumentationVisitor::generateResultProcessing() const
{
    std::stringstream ss;

    // 1. 生成辅助函数
    ss << TimingCodeGenerator::generateGetTotalTimeFunc();
    ss << TimingCodeGenerator::generateSynchronizationCode();

    // 2. 生成结果处理函数头部
    ss << TimingCodeGenerator::generateResultsHeader();

    // 3. 合并各函数时间
    auto rootFunctions = callGraph.getRootFunctions();
    for (const auto &funcName: alreadyDeclaredFuncs) {
        auto node = callGraph.getNode(funcName);
        if (!node) continue;

        bool isRoot = callGraph.isRootFunction(funcName);
        ss << TimingCodeGenerator::generateTimeCombiningCode(funcName, node->getCallees(), isRoot);
    }

    // 4. 生成报告头部
    ss << TimingCodeGenerator::generateReportHeader();

    // 5. 递归生成函数统计信息
    std::function<void(const std::string &, int)> printFuncStats;
    printFuncStats = [&](const std::string &funcName, int level) {
        auto node = callGraph.getNode(funcName);
        if (!node) return;

        ss << TimingCodeGenerator::generateFunctionStats(funcName, level);

        for (const auto &callee: node->getCallees()) {
            if (alreadyDeclaredFuncs.count(callee->getName())) {
                printFuncStats(callee->getName(), level + 1);
            }
        }
    };

    for (const auto &rootFunc: rootFunctions) {
        if (alreadyDeclaredFuncs.count(rootFunc)) {
            printFuncStats(rootFunc, 0);
        }
    }

    // 6. 生成热点函数分析
    ss << TimingCodeGenerator::generateHotFunctionsHeader();

    for (const auto &funcName: alreadyDeclaredFuncs) {
        ss << TimingCodeGenerator::generateHotFunctionCheck(
            funcName, callGraph.getCallers(funcName));
    }

    // 7. 结束函数
    ss << "    }\n"
            << "}\n";

    return ss.str();
}

// 处理整个翻译单元
void TimeInstrumentationConsumer::HandleTranslationUnit(clang::ASTContext &Context)
{
    // 获取 SourceManager
    clang::SourceManager &SM = Context.getSourceManager();

    // 获取文件入口（FileEntry）
    const clang::FileEntry *FE = SM.getFileEntryForID(SM.getMainFileID());

    // 首先构建调用图
    CallGraphBuilder graphBuilder(callGraph, FE->tryGetRealPathName().str());
    graphBuilder.TraverseDecl(Context.getTranslationUnitDecl());
    callGraph.dump();

    // 执行插桩操作
    TimeInstrumentationVisitor visitor(rewriter, callGraph, Context, includes);
    visitor.TraverseDecl(Context.getTranslationUnitDecl());

    // 在文件末尾添加时间结果处理代码
    auto endLoc = rewriter.getSourceMgr().getLocForEndOfFile(rewriter.getSourceMgr().getMainFileID());
    if (endLoc.isValid()) {
        rewriter.InsertText(endLoc, visitor.generateResultProcessing(), true, true);
    }
}
