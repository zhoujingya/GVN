//==============================================================================
// FILE:
//    GVN.cpp
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libGVN.dylib -passes="demo-gvn" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//==============================================================================
#include "GVN.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//------------------------------------------------------------------------------
// HelloWorld implementation
//------------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.

llvm::PreservedAnalyses GVN::run(llvm::Function &Func,
                                 llvm::FunctionAnalysisManager &FAM) {
  llvm::outs() << "Hello world\n";
  return PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getGVNPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "GVN", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "demo-gvn") {
                    FPM.addPass(GVN());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getGVNPluginInfo();
}
