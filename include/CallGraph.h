#ifndef CALL_GRAPH_H
#define CALL_GRAPH_H

#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"

// 用于表示调用图中的每个节点（即每个函数）
class CallGraphNode {
public:
    explicit CallGraphNode(const std::string& name) : functionName(name) {}

    // 将一个被调用的函数（callee）添加到当前函数的 callees 列表中
    void addCallee(const std::shared_ptr<CallGraphNode>& callee) {
        // 避免重复添加相同的callee
        for (const auto& existing : callees) {
            if (existing->getName() == callee->getName()) {
                return;
            }
        }
        callees.push_back(callee);
        std::stringstream ss;
        ss << "__time_" << functionName << "_" << callee->getName();  // main -> sub:__time_main_sub
        calleesVarNameMap[callee] = ss.str();
    }

    const std::string& getName() const { return functionName; }
    const std::vector<std::shared_ptr<CallGraphNode>>& getCallees() const { return callees; }
    /* 返回该函数所需要用到的所有时间记录变量的名字，包括自己的
        规则：__time_ + 本函数名 + _调用函数名
        e.g. main -> sub:__time_main_sub
    */
    const std::vector<std::string> getTimeVarName() const
    {
        std::vector<std::string> rst;
        rst.push_back("__time_" + functionName);
        for (const auto& i : calleesVarNameMap) {
            // llvm::outs() << i.second;
            rst.push_back(i.second);
        }
        return rst;
    }

private:
    std::string functionName;
    // 调用的函数的列表
    std::vector<std::shared_ptr<CallGraphNode>> callees;
    std::unordered_map<std::shared_ptr<CallGraphNode>, std::string> calleesVarNameMap;
};

// 用于管理整个调用图
class CallGraph {
public:
    // 如果某个函数节点尚未存在，则创建它
    void addNode(const std::string& name);

    /*
    添加一个从 caller 到 callee 的调用关系（即有向边）。
    •   首先确保两个函数节点都存在。
    •   在调用图中，caller 调用 callee，并将 callee 加入 caller 的 callees 列表中。
    •   另外，caller 被添加到 callee 的反向边（callers）列表中，反映出 callee 被哪些函数调用。
    */
    void addEdge(const std::string& caller, const std::string& callee);

    std::shared_ptr<CallGraphNode> getNode(const std::string &name) const;

    // 返回调用图中的根节点，即没有其他函数调用它们的函数。这些函数没有出现在任何其他函数的反向边中
    std::vector<std::string> getRootFunctions() const;

    std::vector<std::string> getAllFunctionName() const;

    // 判断给定函数是否为叶子函数，即没有被其他函数调用
    bool isLeafFunction(const std::string& name) const;
    // 判断给定函数是否为根函数
    bool isRootFunction(const std::string& name) const;
    // 获取调用某函数的所有函数
    std::vector<std::string> getCallers(const std::string& name) const;
    void clear();

    // 输出调用图的结构，列出每个函数以及它调用的函数
    void dump() const;
    // 输出调用图到文件
    void dumpToFile(const std::string& filename) const;

private:
    // 节点集合
    std::unordered_map<std::string, std::shared_ptr<CallGraphNode>> nodes;
    // 反向边集合 callers（记录每个函数的调用者），使用set避免重复
    std::unordered_map<std::string, std::unordered_set<std::string>> callers;
};

/*
用于遍历 Clang 解析的 AST 并构建调用图
•   CallGraphBuilder 作为 Clang 的 AST 遍历器，遍历源代码中的每个函数声明和函数调用。
•   每当遇到一个函数声明时，调用 CallGraph::addNode 添加该函数节点。
•   每当遇到一个函数调用时，调用 CallGraph::addEdge 记录函数之间的调用关系。
*/
class CallGraphBuilder : public clang::RecursiveASTVisitor<CallGraphBuilder> {
public:
    explicit CallGraphBuilder(CallGraph& g, const std::string& sourceFile) : graph(g), currentFunction(""), sourceFile(sourceFile) {}

    // 处理函数声明节点。获取当前正在处理的函数名称，并在调用图中为其创建一个节点
    bool VisitFunctionDecl(const clang::FunctionDecl* func);

    /*
    处理函数调用表达式
    如果当前函数（currentFunction）存在，则记录它调用了一个直接的函数 callee，并在调用图中添加一条从当前函数到被调用函数的边
    */
    bool VisitCallExpr(clang::CallExpr* call);

    // 添加必要的遍历控制函数
    bool shouldVisitImplicitCode() const { return false; }
    bool shouldVisitTemplateInstantiations() const { return false; }

private:
    CallGraph& graph;
    std::string currentFunction;
    std::string sourceFile;
};

#endif // CALL_GRAPH_H