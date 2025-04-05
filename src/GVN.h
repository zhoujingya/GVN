//==============================================================================
// FILE:
//    GVN.h
//
// DESCRIPTION:
//    Declares the Global Value Numbering pass for the new LLVM Pass Manager.
//    This pass eliminates redundant computations by identifying expressions
//    that compute identical values.
//
// License: MIT
//==============================================================================

#ifndef GVN_H
#define GVN_H

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

//------------------------------------------------------------------------------
// GVN Pass
//------------------------------------------------------------------------------
// This class implements the Global Value Numbering optimization pass
class GVN : public llvm::PassInfoMixin<GVN> {
public:
  // Main entry point - run GVN on a function
  llvm::PreservedAnalyses run(llvm::Function &F,
                             llvm::FunctionAnalysisManager &FAM);
};

#endif // GVN_H
