#include "../include/MemoryInstrumentation.h"
#include <sstream>
#include <functional>
#include <string>
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/Expr.h"

bool MemoryInstrumentationVisitor::VisitTranslationUnitDecl(clang::TranslationUnitDecl *TU) {
    llvm::outs() << "Inserting memory profiler definitions at file start\n";
    auto InsertLoc = rewriter.getSourceMgr().getLocForStartOfFile(
        rewriter.getSourceMgr().getMainFileID());
    if (InsertLoc.isValid()) {
        rewriter.InsertText(InsertLoc,
            MemoryCodeGenerator::generateCompleteProfiler(includes));
    }
    return true;
}

void MemoryInstrumentationVisitor::insertVarProfiler(const clang::VarDecl *VD) {
    if (!shouldInstrumentVar(VD)) return;

    std::string VarName = VD->getNameAsString();
    if (instrumentedVars.count(VarName)) return;

    // 将变量添加到当前函数的已初始化变量集合中
    if (const auto *FDContext = llvm::dyn_cast<clang::FunctionDecl>(VD->getDeclContext())) {
        std::string FuncName = FDContext->getNameAsString();
        functionInitializedVars[FuncName].insert(VarName);
    }
    
    // 获取函数名
    std::string FuncName = "global";
    if (const auto *FDContext = llvm::dyn_cast<clang::FunctionDecl>(VD->getDeclContext())) {
        FuncName = FDContext->getNameAsString();
        functionVars[FuncName].push_back(VarName);
    }

    // 生成初始化代码,添加换行
    std::stringstream SS;
    clang::QualType type = VD->getType();
    std::string addrExpr = (type->isArrayType() || type->isPointerType()) ? VarName : "&" + VarName;
    
    SS << "\nmem_profile_t __" << VarName << "_prof;\n"
       << "__mem_init(&__" << VarName << "_prof, \""
       << VarName << "\", \"" << FuncName << "\", (void*)"
       << addrExpr << ", sizeof(" << VarName << "[0]));\n";
    
    // 如果是数组类型的变量，直接计算大小，例如，int a[10]; 则 a_prof.var_size = sizeof(a);
    // if (type->isArrayType()) {
    //     SS << "__" << VarName << "_prof.var_size = sizeof(" << VarName << ");\n";
    // }

    // 获取变量声明后的正确位置
    clang::SourceLocation InsertLoc;
    if (VD->hasInit()) {
        InsertLoc = VD->getInit()->getEndLoc();
    } else {
        InsertLoc = VD->getEndLoc();
    }
    InsertLoc = clang::Lexer::findLocationAfterToken(
        InsertLoc,
        clang::tok::semi,
        rewriter.getSourceMgr(),
        rewriter.getLangOpts(),
        false);

    if (isInMainFile(InsertLoc)) {
        rewriter.InsertText(InsertLoc, SS.str(), true, true);
        instrumentedVars.insert(VarName);
    }
}

// 处理函数参数中的数组初始化
void MemoryInstrumentationVisitor::insertFuncParamProfiler(const clang::FunctionDecl *FD) {
    if (!FD || !FD->hasBody()) return;

    // 获取函数体开始位置
    clang::SourceLocation BodyStart = FD->getBody()->getBeginLoc();
    std::string ParamProfilerCode;

    // 处理每个参数
    for (const auto *Param : FD->parameters()) {
        if (shouldInstrumentVar(Param)) {
            std::string ParamName = Param->getNameAsString();
            functionInitializedVars[FD->getNameAsString()].insert(ParamName);
            clang::QualType type = Param->getType();
            std::string addrExpr = (type->isArrayType() || type->isPointerType()) 
                                  ? ParamName : "&" + ParamName;

            ParamProfilerCode += "\n\tmem_profile_t __" + ParamName + "_prof;\n"
                              + "\t__mem_init(&__" + ParamName + "_prof, \""
                              + ParamName + "\", \"" + FD->getNameAsString()
                              + "\", (void*)" + addrExpr
                              + ", sizeof(" + ParamName + "[0]));\n";
            instrumentedVars.insert(ParamName);
            functionVars[FD->getNameAsString()].push_back(ParamName);
        }
    }

    // 在函数体开始处插入参数profiler代码
    if (!ParamProfilerCode.empty() && isInMainFile(BodyStart)) {
        rewriter.InsertText(BodyStart.getLocWithOffset(1),
                           ParamProfilerCode, true, true);
    }
}

bool MemoryInstrumentationVisitor::insertAccessProfiler(const clang::Expr *E) const
{
    if (!E) return true;

    std::string VarName;
    std::string AccessExpr;
    clang::SourceLocation InsertLoc;

    if (auto ASE = llvm::dyn_cast<clang::ArraySubscriptExpr>(E)) {
        // 数组访问
        // if (auto *Base = ASE->getBase()) {
        //     if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
        //             Base->IgnoreImpCasts())) {
        //         VarName = DRE->getNameInfo().getAsString();
        //         AccessExpr = getSourceText(ASE);
        //         InsertLoc = ASE->getBeginLoc();
        //     }
        // }
        return handleArraySubscriptExpr(ASE);
    } else if (auto *UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
        // 指针解引用
        // if (UO->getOpcode() == clang::UO_Deref) {
        //     if (auto *SubExpr = UO->getSubExpr()) {
        //         if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
        //                 SubExpr->IgnoreImpCasts())) {
        //             VarName = DRE->getNameInfo().getAsString();
        //             AccessExpr = getSourceText(UO);
        //             InsertLoc = UO->getBeginLoc();
        //         }
        //     }
        // }
        return handleUnaryOperator(UO);
    } else if (auto *ME = llvm::dyn_cast<clang::MemberExpr>(E)) {
        // 结构体成员访问
        if (auto *Base = ME->getBase()) {
            if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
                    Base->IgnoreImpCasts())) {
                VarName = DRE->getNameInfo().getAsString();
                AccessExpr = getSourceText(ME);
                InsertLoc = ME->getBeginLoc();
            }
        }
    }

    if (VarName.empty() || !instrumentedVars.count(VarName) ||
        !isInMainFile(InsertLoc)) return true;

    std::string RecordCode = "__mem_record(&__" + VarName + "_prof, (void*)&(" +
                            AccessExpr + "));\n";
    rewriter.InsertText(InsertLoc, RecordCode, true, true);
    return true;
}

void MemoryInstrumentationVisitor::insertAnalysisCode(clang::ReturnStmt* RS) {
    if (!RS || !shouldInstrumentFunction()) return;
    
    clang::SourceLocation Loc = RS->getBeginLoc();
    if (!isInMainFile(Loc)) return;
    
    // 插入当前函数已初始化变量的分析代码
    std::string analysisCode = generateAnalysisCode(currentFunctionName);
    rewriter.InsertText(Loc, analysisCode, true, true);
}

std::string MemoryInstrumentationVisitor::generateAnalysisCode(const std::string& functionName) {
    std::stringstream analysisCode;
    
    // 只分析在该函数中已初始化的变量
    auto& initializedVars = functionInitializedVars[functionName];
    for (const auto& var : initializedVars) {
        analysisCode << "__mem_analyze(&__" << var << "_prof);\n";
        analysisCode << "__mem_print_analysis(&__" << var << "_prof);\n";
    }
    
    return analysisCode.str();
}

std::string MemoryInstrumentationVisitor::getSourceText(const clang::Stmt *stmt) const
{
    clang::SourceManager &SM = rewriter.getSourceMgr();
    clang::SourceLocation Start = stmt->getBeginLoc();
    clang::SourceLocation End = stmt->getEndLoc();

    if (Start.isInvalid() || End.isInvalid()) {
        return "";
    }

    clang::CharSourceRange Range = clang::CharSourceRange::getTokenRange(Start, End);
    return std::string(clang::Lexer::getSourceText(Range, SM,
                                                   rewriter.getLangOpts()));
}

bool MemoryInstrumentationVisitor::shouldInstrumentVar(const clang::VarDecl *VD) const {
    if (!VD) return false;

    // 新增主文件检查
    if (!isInMainFile(VD->getLocation())) {
        return false;
    }

    // 跳过系统头文件中的变量
    if (rewriter.getSourceMgr().isInSystemHeader(VD->getLocation())) {
        return false;
    }

    // 跳过常量
    if (VD->getType().isConstQualified()) {
        return false;
    }

    clang::QualType Type = VD->getType();

    // 分析变量类型
    bool isValidType = Type->isArrayType() ||     // 数组类型
                      Type->isPointerType() ||    // 指针类型
                      Type->isStructureType();    // 结构体类型

    // 对于结构体类型，检查是否包含数组或指针成员
    if (Type->isStructureType()) {
        if (const auto *RT = Type->getAs<clang::RecordType>()) {
            const auto *RD = RT->getDecl();
            for (const auto *Field : RD->fields()) {
                clang::QualType FieldType = Field->getType();
                if (FieldType->isArrayType() || FieldType->isPointerType()) {
                    return true;
                }
            }
        }
        return false;  // 如果结构体不包含数组或指针成员，不进行插桩
    }

    return isValidType;
}

bool MemoryInstrumentationVisitor::shouldInstrumentFunction() const
{
    return targetFunctions.empty() || // 如果没有指定目标函数，则对所有函数进行插桩
           targetFunctions.find(currentFunctionName) != targetFunctions.end(); // 否则只对指定函数插桩
}

bool MemoryInstrumentationVisitor::isInMainFile(clang::SourceLocation Loc) const {
    return !Loc.isInvalid() &&
           !rewriter.getSourceMgr().isInSystemHeader(Loc) &&
           rewriter.getSourceMgr().isInMainFile(Loc);
}

bool MemoryInstrumentationVisitor::TraverseFunctionDecl(clang::FunctionDecl *FD) {
    if (!FD || !FD->hasBody() || !shouldInstrumentFunction()) {
        return clang::RecursiveASTVisitor<MemoryInstrumentationVisitor>::TraverseFunctionDecl(FD);
    }

    // Store current function name for context
    std::string prevFunction = currentFunctionName;
    currentFunctionName = FD->getNameAsString();

    // Clear previous function's variables
    functionVars[currentFunctionName].clear();

    // Traverse the function normally
    bool result = clang::RecursiveASTVisitor<MemoryInstrumentationVisitor>::TraverseFunctionDecl(FD);

    // Insert analysis code at the end of the function
    // insertAnalysisCode(FD);

    // Restore previous function name
    currentFunctionName = prevFunction;

    return result;
}

bool MemoryInstrumentationVisitor::VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (!FD) return true;
    currentFunctionName = FD->getNameAsString();

    // 处理函数参数,在函数体开始处插入
    if (FD->hasBody() && shouldInstrumentFunction()) {
        insertFuncParamProfiler(FD);
    }
    return true;
}

bool MemoryInstrumentationVisitor::VisitVarDecl(clang::VarDecl *VD) {
    if (shouldInstrumentFunction()) {
        insertVarProfiler(VD);
    }
    return true;
}

bool MemoryInstrumentationVisitor::VisitArraySubscriptExpr(clang::ArraySubscriptExpr *ASE) const {
    return handleArraySubscriptExpr(ASE);
}

bool MemoryInstrumentationVisitor::VisitUnaryOperator(clang::UnaryOperator *UO) {
    return handleUnaryOperator(UO);
}

bool MemoryInstrumentationVisitor::VisitReturnStmt(clang::ReturnStmt* RS) {
    if (shouldInstrumentFunction()) {
        insertAnalysisCode(RS);
    }
    return true;
}

bool MemoryInstrumentationVisitor::handleArraySubscriptExpr(const clang::ArraySubscriptExpr *ASE) const {
    if (!shouldInstrumentFunction()) return true;

    const clang::Expr *Base = ASE->getBase()->IgnoreImplicit();
    if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(Base)) {
        std::string ArrayName = DRE->getNameInfo().getAsString();
        if (!instrumentedVars.count(ArrayName)) return true;

        clang::SourceLocation EndLoc = ASE->getEndLoc();
        if (EndLoc.isInvalid()) return true;
        const clang::SourceManager &SM = ctx.getSourceManager();
        unsigned currentLine = SM.getExpansionLineNumber(EndLoc);
        clang::SourceLocation newLineStart = SM.translateLineCol(SM.getFileID(EndLoc), currentLine + 1, 1);

        unsigned indent = getIndentation(ASE->getBeginLoc());
        std::string indentStr(indent, ' ');

        std::string stmtText = getSourceText(ASE);
        std::string RecordCode = indentStr + "__mem_record(&__" + ArrayName +
                                "_prof, (void*)&(" + stmtText + "));\n";

        rewriter.InsertText(newLineStart, RecordCode,true,false);
    }
    return true;
}

bool MemoryInstrumentationVisitor::handleUnaryOperator(const clang::UnaryOperator *UO) const {
    if (!shouldInstrumentFunction() || UO->getOpcode() != clang::UO_Deref)
        return true;

    if (auto *Base = UO->getSubExpr()) {
        const clang::DeclRefExpr *DRE = nullptr;
        const clang::Expr *E = Base->IgnoreParenImpCasts();

        // 递归查找DeclRefExpr
        while (E) {
            if ((DRE = llvm::dyn_cast<clang::DeclRefExpr>(E))) {
                break;
            }
            if (const auto *BO = llvm::dyn_cast<clang::BinaryOperator>(E)) {
                E = BO->getLHS()->IgnoreParenImpCasts();
            } else {
                break;
            }
        }

        if (DRE && instrumentedVars.count(DRE->getNameInfo().getAsString())) {
            std::string PtrName = DRE->getNameInfo().getAsString();
            std::string stmtText = getSourceText(Base);
            std::string RecordCode = "__mem_record(&__" + PtrName +
                                   "_prof, (void*)(" + stmtText + "));\n";
            rewriter.InsertText(UO->getBeginLoc(), RecordCode, false, true);
        }
    }
    return true;
}

unsigned MemoryInstrumentationVisitor::getIndentation(clang::SourceLocation Loc) const{
    if (Loc.isInvalid()) return 0;

    const clang::SourceManager &SM = rewriter.getSourceMgr();
    unsigned lineNo = SM.getExpansionLineNumber(Loc);
    unsigned columnNo = SM.getExpansionColumnNumber(Loc);

    std::string line = getLine(Loc);
    unsigned indent = 0;
    while (indent < line.length() && std::isspace(line[indent])) indent++;

    return indent;
}

std::string MemoryInstrumentationVisitor::getLine(clang::SourceLocation Loc) const {
    const clang::SourceManager &SM = rewriter.getSourceMgr();
    clang::FileID FID = SM.getFileID(Loc);

    // 直接获取 StringRef，无需调用 getBuffer()
    llvm::StringRef content = SM.getBufferData(FID);

    size_t offset = SM.getFileOffset(Loc);
    size_t lineStart = content.rfind('\n', offset) + 1;
    size_t lineEnd = content.find('\n', lineStart);

    // 处理边界条件
    if (lineStart == llvm::StringRef::npos) lineStart = 0;
    if (lineEnd == llvm::StringRef::npos) lineEnd = content.size();

    return content.substr(lineStart, lineEnd - lineStart).str();
}

bool MemoryInstrumentationVisitor::VisitMemberExpr(clang::MemberExpr *ME) const
{
    if (shouldInstrumentFunction()) {
        return insertAccessProfiler(ME);
    }
    return true;
}

void MemoryInstrumentationConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
    MemoryInstrumentationVisitor Visitor(rewriter, Context, includes, targetFunctions);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    
    // Debug output for initialized variables
    llvm::outs() << "\nInstrumented Variables:\n";
    const auto& initializedVars = Visitor.getInitializedVars();
    
    for (const auto& funcPair : initializedVars) {
        if (!funcPair.first.empty() && !funcPair.second.empty()) {
            llvm::outs() << funcPair.first << "\n";
            for (const auto& var : funcPair.second) {
                if (!var.empty()) {
                    llvm::outs() << "  - " << var << "\n";
                }
            }
        }
    }
    llvm::outs() << "\n";
}