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

    // 记录反向边（使用set自动去重）
    callers[callee].insert(caller);
}

std::shared_ptr<CallGraphNode> CallGraph::getNode(const std::string &name) const {
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

bool CallGraph::isRootFunction(const std::string& name) const {
    return callers.find(name) == callers.end() || callers.at(name).empty();
}

void CallGraph::clear() {
    nodes.clear();
    callers.clear();
}

void CallGraph::dump() const {
    std::cout << "Call Graph Structure:\n";
    std::cout << "==================\n";
    for (const auto& pair : nodes) {
        const std::string& funcName = pair.first;
        const auto& callees = pair.second->getCallees();
        const auto& funcCallers = callers.find(funcName);

        std::cout << "\nFunction: " << funcName << "\n";
        std::cout << "  Calls:";
        for (const auto& callee : callees) {
            std::cout << " " << callee->getName();
        }
        std::cout << "\n  Called by:";
        if (funcCallers != callers.end()) {
            for (const auto& caller : funcCallers->second) {
                std::cout << " " << caller;
            }
        }
        std::cout << "\n";
    }
    std::cout << "==================\n";
}

void CallGraph::dumpToFile(const std::string& filename) const {
    FILE* file = fopen(filename.c_str(), "w");
    if (!file) {
        std::cerr << "Error: Unable to open file " << filename << " for writing\n";
        return;
    }

    fprintf(file, "digraph CallGraph {\n");
    for (const auto& pair : nodes) {
        const std::string& funcName = pair.first;
        const auto& callees = pair.second->getCallees();

        for (const auto& callee : callees) {
            fprintf(file, "  \"%s\" -> \"%s\";\n", funcName.c_str(), callee->getName().c_str());
        }
    }
    fprintf(file, "}\n");
    fclose(file);
}

std::vector<std::string> CallGraph::getCallers(const std::string& name) const {
    std::vector<std::string> result;
    auto it = callers.find(name);
    if (it != callers.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

bool CallGraphBuilder::VisitFunctionDecl(const clang::FunctionDecl* func) {
    if (!func || !func->hasBody()) {
        return true;
    }

    // 检查是否在主源文件中
    if (func->getASTContext().getSourceManager().getFilename(func->getLocation()) != sourceFile) {
        return true;
    }

    // 获取函数名称
    std::string name = func->getNameInfo().getName().getAsString();
    if (name.empty()) {
        return true;
    }

    currentFunction = name;
    graph.addNode(currentFunction);
    return true;
}

bool CallGraphBuilder::VisitCallExpr(clang::CallExpr* call) {
    if (!call || currentFunction.empty()) {
        return true;
    }

    if (const auto* callee = call->getDirectCallee()) {
        std::string calleeName = callee->getNameInfo().getName().getAsString();
        if (!calleeName.empty()) {
            graph.addEdge(currentFunction, calleeName);
        }
    }
    return true;
}