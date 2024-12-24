#ifndef CALL_GRAPH_H
#define CALL_GRAPH_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"

class CallGraphNode {
public:
    explicit CallGraphNode(const std::string& name) : functionName(name) {}

    void addCallee(std::shared_ptr<CallGraphNode> callee) {
        callees.push_back(callee);
    }

    const std::string& getName() const { return functionName; }
    const std::vector<std::shared_ptr<CallGraphNode>>& getCallees() const { return callees; }

private:
    std::string functionName;
    std::vector<std::shared_ptr<CallGraphNode>> callees;
};

class CallGraph {
public:
    void addNode(const std::string& name);
    void addEdge(const std::string& caller, const std::string& callee);
    std::shared_ptr<CallGraphNode> getNode(const std::string& name) const;
    std::vector<std::string> getRootFunctions() const;
    bool isLeafFunction(const std::string& name) const;
    const std::vector<std::string>& getCallers(const std::string& name) const;
    void clear();
    void dump() const;

private:
    std::unordered_map<std::string, std::shared_ptr<CallGraphNode>> nodes;
    std::unordered_map<std::string, std::vector<std::string>> callers;  // 反向边
};

class CallGraphBuilder : public clang::RecursiveASTVisitor<CallGraphBuilder> {
public:
    explicit CallGraphBuilder(CallGraph& g) : graph(g) {}

    bool VisitFunctionDecl(clang::FunctionDecl* func);
    bool VisitCallExpr(clang::CallExpr* call);

    // 添加必要的遍历控制函数
    bool shouldVisitImplicitCode() const { return false; }
    bool shouldVisitTemplateInstantiations() const { return false; }

private:
    CallGraph& graph;
    std::string currentFunction;
};

#endif // CALL_GRAPH_H