#include "CallGraph.h"
#include <iostream>
#include <stack>
#include <queue>

// 将一个新节点添加到图中
void CallGraph::addNode(const std::string &name)
{
    if (nodes.find(name) == nodes.end()) {
        nodes[name] = std::make_shared<CallGraphNode>(name);
    }
}

// 在图中添加一条从caller到callee的边
void CallGraph::addEdge(const std::string &caller, const std::string &callee)
{
    // 添加节点（如果不存在）
    addNode(caller);
    addNode(callee);

    // 添加调用关系
    nodes[caller]->addCallee(nodes[callee]);

    // 记录反向边（使用set自动去重）
    callers[callee].insert(caller);
}

// 获取指定名称的节点
std::shared_ptr<CallGraphNode> CallGraph::getNode(const std::string &name) const
{
    auto it = nodes.find(name);
    return (it != nodes.end()) ? it->second : nullptr;
}

/**
 * 返回调用图中的所有根函数
 * 
 * 根据CallGraph的设计，根函数是指在调用图中没有任何调用者(caller)的函数。
 * 判断一个函数是否为根函数的条件为:
 * 1. 该函数在callers反向边集合中不存在，表示从未被任何函数调用过
 * 2. 或者该函数在callers中存在但其调用者集合为空
 * 
 * 例如在一个C程序中，main()函数通常就是一个根函数，因为它是程序的入口点，
 * 不会被其他函数调用。
 * 
 * @return 包含所有根函数名称的vector
 */
std::vector<std::string> CallGraph::getRootFunctions() const
{
    std::vector<std::string> roots;
    for (const auto &pair: nodes) {
        const std::string &funcName = pair.first;
        if (callers.find(funcName) == callers.end() || callers.at(funcName).empty()) {
            roots.push_back(funcName);
        }
    }
    return roots;
}

std::vector<std::string> CallGraph::getAllFunctionName() const
{
    std::vector<std::string> rst;
    for (const auto &pair: nodes) {
        rst.push_back(pair.first);
    }
    return rst;
}

// 判断是否为叶子节点
bool CallGraph::isLeafFunction(const std::string &name) const
{
    auto node = getNode(name);
    return node && node->getCallees().empty();
}

// 判断是否为根节点
bool CallGraph::isRootFunction(const std::string &name) const
{
    return callers.find(name) == callers.end() || callers.at(name).empty();
}

// 清空图
void CallGraph::clear()
{
    nodes.clear();
    callers.clear();
}

// 打印调用图结构
void CallGraph::dump() const
{
    std::cout << "Call Graph:\n";
    std::cout << "==================\n\n";

    // 递归打印树形结构的辅助函数
    std::function<void(const std::string &, const std::string &, bool)> printTree;
    printTree = [&](const std::string &funcName, const std::string &prefix, bool isLast) {
        // 打印当前节点
        std::cout << prefix;
        std::cout << (isLast ? "└── " : "├── ");
        std::cout << funcName << "\n";

        // 获取当前函数的所有被调用函数
        auto node = nodes.find(funcName);
        if (node == nodes.end() || node->second->getCallees().empty()) {
            return;
        }

        // 遍历所有被调用函数
        const auto &callees = node->second->getCallees();
        for (size_t i = 0; i < callees.size(); ++i) {
            bool lastCallee = (i == callees.size() - 1);
            std::string newPrefix = prefix + (isLast ? "    " : "│   ");
            printTree(callees[i]->getName(), newPrefix, lastCallee);
        }
    };

    // 找出所有根节点
    std::vector<std::string> roots = getRootFunctions();

    // 从每个根节点开始打印树形结构
    for (size_t i = 0; i < roots.size(); ++i) {
        std::cout << roots[i] << "\n";
        const auto &callees = nodes.find(roots[i])->second->getCallees();
        for (size_t j = 0; j < callees.size(); ++j) {
            printTree(callees[j]->getName(), "", j == callees.size() - 1);
        }
        if (i < roots.size() - 1) std::cout << "\n";
    }

    std::cout << "==================\n";
}

// 将调用图结构输出到文件
void CallGraph::dumpToFile(const std::string &filename) const
{
    FILE *file = fopen(filename.c_str(), "w");
    if (!file) {
        std::cerr << "Error: Unable to open file " << filename << " for writing\n";
        return;
    }

    fprintf(file, "digraph CallGraph {\n");
    for (const auto &pair: nodes) {
        const std::string &funcName = pair.first;
        const auto &callees = pair.second->getCallees();

        for (const auto &callee: callees) {
            fprintf(file, "  \"%s\" -> \"%s\";\n", funcName.c_str(), callee->getName().c_str());
        }
    }
    fprintf(file, "}\n");
    fclose(file);
}

// 获取调用某函数的所有函数
std::vector<std::string> CallGraph::getCallers(const std::string &name) const
{
    std::vector<std::string> result;
    auto it = callers.find(name);
    if (it != callers.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

// 访问函数声明节点
bool CallGraphBuilder::VisitFunctionDecl(const clang::FunctionDecl *func)
{
    if (!func || !func->hasBody()) {
        return true;
    }

    // 检查是否在主源文件中
    auto &SM = func->getASTContext().getSourceManager();
    auto fileEntry = SM.getFileEntryForID(SM.getFileID(func->getLocation()));
    if (!fileEntry || fileEntry->tryGetRealPathName() != sourceFile) {
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

// 访问函数调用表达式节点
bool CallGraphBuilder::VisitCallExpr(clang::CallExpr *call)
{
    if (!call || currentFunction.empty()) {
        return true;
    }

    if (const auto *callee = call->getDirectCallee()) {
        // 跳过系统函数
        clang::SourceManager& SM = callee->getASTContext().getSourceManager();
        if (SM.isInSystemHeader(callee->getLocation())) {
            return true;
        }

        // 获取被调用函数的位置信息
        clang::SourceLocation callLoc = callee->getLocation();
        if (!callLoc.isValid()) {
            return true;
        }

        // 确保被调用函数在当前源文件中
        if (SM.getFilename(callLoc) != sourceFile) {
            return true;
        }
        
        std::string calleeName = callee->getNameInfo().getName().getAsString();
        if (!calleeName.empty()) {
            graph.addEdge(currentFunction, calleeName);
        } else {
        }
    }
    return true;
}
