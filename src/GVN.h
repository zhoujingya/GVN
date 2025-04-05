//==============================================================================
// FILE:
//    GVN.h
//
// DESCRIPTION:
// Demo implementation for global value numbering
//
// License: MIT
//==============================================================================

#ifndef LLVM_TUTOR_CONVERT_FCMP_EQ_H
#define LLVM_TUTOR_CONVERT_FCMP_EQ_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

// Forward declarations
namespace llvm {

class Function;

} // namespace llvm

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------

struct GVN : llvm::PassInfoMixin<GVN> {
  // This is one of the standard run() member functions expected by
  // PassInfoMixin. When the pass is executed by the new PM, this is the
  // function that will be called.
  llvm::PreservedAnalyses run(llvm::Function &Func,
                              llvm::FunctionAnalysisManager &FAM);

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};

#endif // !LLVM_TUTOR_CONVERT_FCMP_EQ_H
