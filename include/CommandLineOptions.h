// CommandLineOptions.h
#ifndef COMMANDLINEOPTIONS_H
#define COMMANDLINEOPTIONS_H

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/CommandLine.h"
#include "TimeInstrumentation.h"
#include <memory>
#include <string>

using namespace clang::tooling;
using namespace llvm;
using namespace clang;

// 命令行选项
extern cl::OptionCategory ToolCategory;

extern cl::opt<bool> EnableTimeInst;
extern cl::opt<bool> EnableMemoryInst;
extern cl::opt<double> TotalTimeThreshold;
extern cl::opt<double> ParentTimeThreshold;
extern cl::opt<std::string> OutputFilename;
extern cl::list<std::string> TargetFunctions;

#endif //COMMANDLINEOPTIONS_H