#include "MemoryInstrumentation.h"
#include <sstream>
#include "clang/Lex/Lexer.h"
#include "llvm/Support/Casting.h"

bool MemoryInstrumentationVisitor::safelyInsertText(clang::SourceLocation loc,
                                                   const std::string &text,
                                                   bool insertAfter) {
    if (loc.isInvalid()) return false;

    if (insertAfter) {
        loc = loc.getLocWithOffset(1);
    }

    if (!rewriter.getSourceMgr().isWrittenInMainFile(loc)) return false;

    return rewriter.InsertText(loc, text, false, true);
}

std::string MemoryInstrumentationVisitor::getExprAsString(const clang::Expr *expr) {
    clang::SourceManager &SM = rewriter.getSourceMgr();
    clang::SourceLocation startLoc = expr->getBeginLoc();
    clang::SourceLocation endLoc = expr->getEndLoc();

    if (startLoc.isInvalid() || endLoc.isInvalid()) {
        return "";
    }

    clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(startLoc, endLoc);
    return clang::Lexer::getSourceText(range, SM, rewriter.getLangOpts()).str();
}

void MemoryInstrumentationVisitor::insertInitialization(clang::VarDecl *varDecl) {
    std::string varName = varDecl->getNameAsString();
    if (instrumentedVars.count(varName) > 0) return;

    std::stringstream ss;
    ss << "initMemoryRegion(\"" << varName << "\", \""
       << varDecl->getDeclContext()->getDeclKindName() << "\", "
       << "&" << varName << ", sizeof(" << varName << "));\n";

    clang::SourceLocation loc = varDecl->getEndLoc();
    safelyInsertText(loc, ss.str(), true);
    instrumentedVars.insert(varName);
}

void MemoryInstrumentationVisitor::insertAccessRecord(clang::Expr *expr,
                                                     const std::string &varName) {
    std::stringstream ss;
    ss << "recordMemoryAccess(\"" << varName << "\", "
       << "&(" << getExprAsString(expr) << "));\n";

    safelyInsertText(expr->getBeginLoc(), ss.str());
}

void MemoryInstrumentationVisitor::insertStatistics(clang::FunctionDecl *funcDecl) {
    std::stringstream ss;
    for (const auto &varName : instrumentedVars) {
        ss << "finalizeMemoryRegion(\"" << varName << "\");\n";
    }

    // 修改这一行
    if (auto *body = llvm::dyn_cast<clang::CompoundStmt>(funcDecl->getBody())) {
        clang::SourceLocation endLoc = body->getRBracLoc();
        safelyInsertText(endLoc, ss.str());
    }
}

bool MemoryInstrumentationVisitor::VisitArrayType(clang::ArrayType* arrayType) {
    // 数组类型访问不需要特殊处理，因为我们在变量声明时处理
    return true;
}

bool MemoryInstrumentationVisitor::VisitVarDecl(clang::VarDecl *varDecl) {
    if (!varDecl->hasGlobalStorage() && !varDecl->isStaticLocal()) {
        insertInitialization(varDecl);
    }
    return true;
}

bool MemoryInstrumentationVisitor::VisitArraySubscriptExpr(clang::ArraySubscriptExpr *arrayExpr) {
    if (const clang::Expr *base = arrayExpr->getBase()) {
        if (const clang::DeclRefExpr *declRef =
            clang::dyn_cast<clang::DeclRefExpr>(base->IgnoreParenCasts())) {
            std::string varName = declRef->getDecl()->getNameAsString();
            if (instrumentedVars.count(varName) > 0) {
                insertAccessRecord(arrayExpr, varName);
            }
        }
    }
    return true;
}

bool MemoryInstrumentationVisitor::VisitMemberExpr(clang::MemberExpr *memberExpr) {
    if (const clang::Expr *base = memberExpr->getBase()) {
        if (const clang::DeclRefExpr *declRef =
            clang::dyn_cast<clang::DeclRefExpr>(base->IgnoreParenCasts())) {
            std::string varName = declRef->getDecl()->getNameAsString();
            if (instrumentedVars.count(varName) > 0) {
                insertAccessRecord(memberExpr, varName);
            }
        }
    }
    return true;
}

bool MemoryInstrumentationVisitor::VisitUnaryOperator(clang::UnaryOperator *unaryOp) {
    if (unaryOp->getOpcode() == clang::UO_Deref) {
        if (const clang::Expr *subExpr = unaryOp->getSubExpr()) {
            if (const auto *declRef =
                clang::dyn_cast<clang::DeclRefExpr>(subExpr->IgnoreParenCasts())) {
                std::string varName = declRef->getDecl()->getNameAsString();
                if (instrumentedVars.count(varName) > 0) {
                    insertAccessRecord(unaryOp, varName);
                }
            }
        }
    }
    return true;
}

bool MemoryInstrumentationVisitor::VisitFunctionDecl(clang::FunctionDecl *funcDecl) {
    if (funcDecl->hasBody()) {
        insertStatistics(funcDecl);
    }
    return true;
}

void MemoryInstrumentationConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
    MemoryInstrumentationVisitor visitor(rewriter, Context);
    visitor.TraverseDecl(Context.getTranslationUnitDecl());
}