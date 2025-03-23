// CommandLineOptions.cpp
#include "../include/CommandLineOptions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/CommandLine.h"
#include <memory>
#include <string>

using namespace llvm;

cl::OptionCategory ToolCategory("MT-3000 Instrumentation Tool Options");

cl::opt<std::string> OutputFilename(
    "o",
    cl::desc("Specify output filename"),
    cl::value_desc("filename"),
    cl::cat(ToolCategory));

cl::list<std::string> TargetFunctions(
    "target-funcs",
    cl::desc("Specify target functions to instrument"),
    cl::value_desc("function_name"),
    cl::CommaSeparated,
    cl::cat(ToolCategory));