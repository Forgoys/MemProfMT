#include "CallGraph.h"
#include <iostream>
#include <stack>
#include <queue>

void CallGraph::addNode(const std::string& name) {
    if (nodes.find(name) == nodes.end()) {
        nodes[name] = std::make_shared<CallGraphNode>(name);
    }
}

void CallGraph::addEdge(const std::string& caller, const std::string& callee) {
    // 添加节点（如果不存在）
    addNode(caller);
    addNode(callee);

    // 添加调用关系
    nodes[caller]->addCallee(nodes[callee]);

    // 记录反向边
    callers[callee].push_back(caller);
}

std::shared_ptr<CallGraphNode> CallGraph::getNode(const std::string& name) const {
    auto it = nodes.find(name);
    return (it != nodes.end()) ? it->second : nullptr;
}

std::vector<std::string> CallGraph::getRootFunctions() const {
    std::vector<std::string> roots;
    for (const auto& pair : nodes) {
        const std::string& funcName = pair.first;
        if (callers.find(funcName) == callers.end() || callers.at(funcName).empty()) {
            roots.push_back(funcName);
        }
    }
    return roots;
}

bool CallGraph::isLeafFunction(const std::string& name) const {
    auto node = getNode(name);
    return node && node->getCallees().empty();
}

void CallGraph::clear() {
    nodes.clear();
    callers.clear();
}

void CallGraph::dump() const {
    std::cout << "Call Graph Structure:\n";
    for (const auto& pair : nodes) {
        const std::string& funcName = pair.first;
        const auto& callees = pair.second->getCallees();

        std::cout << funcName << " calls:";
        for (const auto& callee : callees) {
            std::cout << " " << callee->getName();
        }
        std::cout << "\n";
    }
}

bool CallGraphBuilder::VisitFunctionDecl(clang::FunctionDecl* func) {
    if (func->hasBody()) {
        currentFunction = func->getNameAsString();
        graph.addNode(currentFunction);
    }
    return true;
}

bool CallGraphBuilder::VisitCallExpr(clang::CallExpr* call) {
    if (!currentFunction.empty()) {
        if (const auto* callee = call->getDirectCallee()) {
            std::string calleeName = callee->getNameAsString();
            graph.addEdge(currentFunction, calleeName);
        }
    }
    return true;
}

const std::vector<std::string>& CallGraph::getCallers(const std::string& name) const {
    static const std::vector<std::string> empty;
    auto it = callers.find(name);
    return (it != callers.end()) ? it->second : empty;
}