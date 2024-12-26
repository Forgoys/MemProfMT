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

cl::opt<bool> EnableTimeInst(
    "time-inst",
    cl::desc("Enable time instrumentation"),
    cl::init(false),
    cl::cat(ToolCategory));

cl::opt<bool> EnableMemoryInst(
    "memory-inst",
    cl::desc("Enable memory access instrumentation"),
    cl::init(false),
    cl::cat(ToolCategory));

cl::opt<double> TotalTimeThreshold(
    "total-time-threshold",
    cl::desc("Threshold for total execution time percentage (default: 20%)"),
    cl::init(20.0),
    cl::cat(ToolCategory));

cl::opt<double> ParentTimeThreshold(
    "parent-time-threshold",
    cl::desc("Threshold for parent function time percentage (default: 40%)"),
    cl::init(40.0),
    cl::cat(ToolCategory));

cl::opt<std::string> OutputFilename(
    "o",
    cl::desc("Specify output filename"),
    cl::value_desc("filename"),
    cl::cat(ToolCategory));