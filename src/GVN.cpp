//==============================================================================
// FILE:
//    GVN.cpp
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libGVN.dylib -passes="function(demo-gvn)" \
//        -disable-output
//
// DESCRIPTION:
//    This pass implements a simplified Global Value Numbering (GVN) algorithm.
//    GVN is an optimization technique that eliminates redundant computations.
//    It works by assigning a unique value number to computations that produce
//    the same value, then replacing redundant computations with previously
//    computed values.
//
// License: MIT
//==============================================================================

#include "GVN.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>
#include <string>
#include <unordered_map>

using namespace llvm;

#define DEBUG_TYPE "demo-gvn"

// Statistics to track the effectiveness of the pass
STATISTIC(NumGVNInstructions, "Number of instructions processed by GVN");
STATISTIC(NumGVNRedundant, "Number of redundant instructions removed by GVN");

namespace {
// ValueNumber uniquely identifies a computed value
using ValueNumber = unsigned;

// Hash structure for Value*
struct ValueHashInfo {
  unsigned hashValue(const Value *Val) const;
  bool isEqual(const Value *LHS, const Value *RHS) const;
  static unsigned getHashValue(const Value *Val);
  static bool compare(const Value *LHS, const Value *RHS);
};

// Value expression table
struct ValueTable {
  DenseMap<Value *, ValueNumber> valueNumbering;
  std::unordered_map<std::string, ValueNumber> expressionNumbering;
  DenseMap<ValueNumber, Value *> numberToValue;
  unsigned nextValueNumber;

  ValueTable() : nextValueNumber(1) {}

  ValueNumber lookupOrAddValue(Value *V);
  std::string getExpressionString(Instruction *I);
  bool areEqual(Value *V1, Value *V2);
  void clear() {
    valueNumbering.clear();
    expressionNumbering.clear();
    numberToValue.clear();
    nextValueNumber = 1;
  }
};
} // anonymous namespace

// Hash a value based on its properties
unsigned ValueHashInfo::hashValue(const Value *Val) const {
  return getHashValue(Val);
}

// Static hash function for Value*
unsigned ValueHashInfo::getHashValue(const Value *Val) {
  // Handle Constants
  if (const Constant *C = dyn_cast<Constant>(Val)) {
    if (C->isNullValue())
      return 0;
    // Hash based on constant's raw data
    if (const ConstantInt *CI = dyn_cast<ConstantInt>(C))
      return CI->getZExtValue();
    // For other constants, use the pointer value as a hash
    return (unsigned)(intptr_t)C;
  }

  // Handle Instructions
  if (const Instruction *I = dyn_cast<Instruction>(Val)) {
    unsigned Hash = I->getOpcode();
    // Combine hash with operand pointers
    for (const auto &Op : I->operands())
      Hash = Hash * 31 + getHashValue(Op);
    return Hash;
  }

  // For other Values like arguments, just use pointer
  return (unsigned)(intptr_t)Val;
}

// Compare two values for equality
bool ValueHashInfo::isEqual(const Value *LHS, const Value *RHS) const {
  return compare(LHS, RHS);
}

// Static compare function for Values
bool ValueHashInfo::compare(const Value *LHS, const Value *RHS) {
  // Different types cannot be equal
  if (LHS->getType() != RHS->getType())
    return false;

  // If they're the same value
  if (LHS == RHS)
    return true;

  // Compare constants
  if (const Constant *LC = dyn_cast<Constant>(LHS)) {
    if (const Constant *RC = dyn_cast<Constant>(RHS)) {
      // For simple constants, we can just compare them directly
      if (LC == RC)
        return true;

      // For constant integers, compare their values
      if (const ConstantInt *LCI = dyn_cast<ConstantInt>(LC)) {
        if (const ConstantInt *RCI = dyn_cast<ConstantInt>(RC))
          return LCI->getValue() == RCI->getValue();
      }

      // For constant FP, compare their values
      if (const ConstantFP *LCF = dyn_cast<ConstantFP>(LC)) {
        if (const ConstantFP *RCF = dyn_cast<ConstantFP>(RC))
          return LCF->isExactlyValue(RCF->getValueAPF());
      }

      // For other constants, we'd need more detailed comparisons
      // This is simplified for demo purposes
    }
    return false;
  }

  // Compare instructions
  if (const Instruction *LI = dyn_cast<Instruction>(LHS)) {
    if (const Instruction *RI = dyn_cast<Instruction>(RHS)) {
      // Different opcodes cannot be equal
      if (LI->getOpcode() != RI->getOpcode())
        return false;

      // Must have the same number of operands
      if (LI->getNumOperands() != RI->getNumOperands())
        return false;

      // For commutative operations, check both orderings
      if (LI->isCommutative()) {
        return (compare(LI->getOperand(0), RI->getOperand(0)) &&
                compare(LI->getOperand(1), RI->getOperand(1))) ||
               (compare(LI->getOperand(0), RI->getOperand(1)) &&
                compare(LI->getOperand(1), RI->getOperand(0)));
      }

      // Compare operands in order
      for (unsigned i = 0; i < LI->getNumOperands(); ++i) {
        if (!compare(LI->getOperand(i), RI->getOperand(i)))
          return false;
      }
      return true;
    }
  }

  // Different value types
  return false;
}

// Convert an instruction into a string representation
std::string ValueTable::getExpressionString(Instruction *I) {
  std::ostringstream OS;

  // Include the opcode
  OS << I->getOpcode() << " ";

  // Special handling for PHI nodes to capture their semantics
  if (PHINode *PN = dyn_cast<PHINode>(I)) {
    // For PHI nodes, we need to incorporate both values and their source blocks
    for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
      Value *InVal = PN->getIncomingValue(i);
      BasicBlock *InBB = PN->getIncomingBlock(i);

      // Add value number and a hash of the block pointer
      ValueNumber ValNum = lookupOrAddValue(InVal);
      OS << ValNum << " " << (unsigned)(intptr_t)InBB << " ";
    }
  } else {
    // Normal handling for non-PHI instructions
    // Add value numbers for each operand
    for (const auto &Op : I->operands()) {
      ValueNumber OpNum = lookupOrAddValue(Op);
      OS << OpNum << " ";
    }

    // For Load instructions, include the address space
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
      OS << LI->getPointerAddressSpace() << " ";
    }

    // For compare instructions, include the predicate
    if (CmpInst *CI = dyn_cast<CmpInst>(I)) {
      OS << CI->getPredicate() << " ";
    }
  }

  return OS.str();
}

// Look up a value's number or assign a new one
ValueNumber ValueTable::lookupOrAddValue(Value *V) {
  // Constants always get the same number
  if (Constant *C = dyn_cast<Constant>(V)) {
    if (C->isNullValue())
      return 0; // Null values always get number 0
  }

  // Check if we already have a number for this value
  auto It = valueNumbering.find(V);
  if (It != valueNumbering.end())
    return It->second;

  // Handle instructions specially
  if (Instruction *I = dyn_cast<Instruction>(V)) {
    // Skip instructions we don't handle
    if (!I->isBinaryOp() && !isa<CmpInst>(I) && !isa<LoadInst>(I) && !isa<PHINode>(I))
      goto CreateNewNumber;

    // Create expression string and check if we've seen it before
    std::string Expression = getExpressionString(I);
    auto ExprIt = expressionNumbering.find(Expression);
    if (ExprIt != expressionNumbering.end()) {
      ValueNumber VN = ExprIt->second;
      valueNumbering[V] = VN;
      return VN;
    }

    // New expression, assign a new number
    ValueNumber VN = nextValueNumber++;
    expressionNumbering[Expression] = VN;
    valueNumbering[V] = VN;
    numberToValue[VN] = V;
    return VN;
  }

CreateNewNumber:
  // Assign new number for this value
  ValueNumber VN = nextValueNumber++;
  valueNumbering[V] = VN;
  numberToValue[VN] = V;
  return VN;
}

// Check if two values compute the same result
bool ValueTable::areEqual(Value *V1, Value *V2) {
  return lookupOrAddValue(V1) == lookupOrAddValue(V2);
}

//------------------------------------------------------------------------------
// GVN Implementation
//------------------------------------------------------------------------------
PreservedAnalyses GVN::run(Function &F, FunctionAnalysisManager &FAM) {
  llvm::outs() << "Running GVN on function: " << F.getName() << "\n";
  bool Changed = false;

  // Get dominator tree for the function
  DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);

  // Our value table for this function
  ValueTable VT;

  // Set to track instructions to remove
  SmallPtrSet<Instruction *, 32> toRemove;

  // Map from instructions to their replacements
  DenseMap<Instruction *, Value *> replacements;

  // First pass: look for trivial PHI nodes where all incoming values are the same
  // This helps identify cases where PHIs can be immediately replaced
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (PHINode *PN = dyn_cast<PHINode>(&I)) {
        // Skip PHIs with no incoming values
        if (PN->getNumIncomingValues() == 0)
          continue;

        // Check if all incoming values are the same
        Value *FirstVal = PN->getIncomingValue(0);
        bool AllSame = true;

        for (unsigned i = 1; i < PN->getNumIncomingValues(); ++i) {
          if (PN->getIncomingValue(i) != FirstVal) {
            AllSame = false;
            break;
          }
        }

        if (AllSame) {
          llvm::outs() << "Found trivial PHI node: " << *PN
                      << "\n  All values are: " << *FirstVal << "\n";
          toRemove.insert(PN);
          replacements[PN] = FirstVal;
          ++NumGVNRedundant;
          Changed = true;
        }
      }
    }
  }

  // Process blocks in dominator tree pre-order to ensure
  // we process definitions before uses
  for (auto *Node : post_order(DT.getRootNode())) {
    BasicBlock *BB = Node->getBlock();

    // Process each instruction in the block
    for (auto I = BB->begin(); I != BB->end(); ++I) {
      Instruction *Inst = &*I;

      // Skip non-eligible instructions
      if (Inst->isTerminator() || Inst->mayHaveSideEffects() ||
          Inst->isEHPad())
        continue;

      // Special handling for PHI nodes
      if (PHINode *PN = dyn_cast<PHINode>(Inst)) {
        llvm::outs() << "Processing PHI node: " << *PN << "\n";
        // Check if all incoming values are the same
        Value *CommonValue = nullptr;
        bool AllSame = true;

        // Skip PHIs with no incoming values
        if (PN->getNumIncomingValues() == 0)
          continue;

        // Get the first incoming value as reference
        CommonValue = PN->getIncomingValue(0);

        // Check if all other incoming values are the same
        for (unsigned i = 1; i < PN->getNumIncomingValues(); ++i) {
          Value *InVal = PN->getIncomingValue(i);
          if (InVal != CommonValue && !VT.areEqual(InVal, CommonValue)) {
            AllSame = false;
            break;
          }
        }

        // If all incoming values are the same, we can replace the PHI
        if (AllSame) {
          llvm::outs() << "PHI node has all same values, can be replaced with: "
                      << *CommonValue << "\n";
          toRemove.insert(PN);
          replacements[PN] = CommonValue;
          ++NumGVNRedundant;
          Changed = true;
          continue;
        }

        // Continue with normal value numbering for PHI nodes
      }

      // Count instructions processed
      ++NumGVNInstructions;

      // For each instruction, look up its value number
      ValueNumber VN = VT.lookupOrAddValue(Inst);

      // If we found a redundant instruction
      if (VT.numberToValue.count(VN) && VT.numberToValue[VN] != Inst) {
        Value *Earlier = VT.numberToValue[VN];
        if (Earlier != Inst && isa<Instruction>(Earlier)) {
          // Use dominator tree to ensure the replacement dominates the uses
          Instruction *EarlierInst = cast<Instruction>(Earlier);
          if (DT.dominates(EarlierInst, Inst)) {
            llvm::outs() << "Found redundant instruction: " << *Inst
                        << "\n  Can be replaced with: " << *Earlier << "\n";
            // Mark instruction for later removal
            toRemove.insert(Inst);
            replacements[Inst] = Earlier;
            ++NumGVNRedundant;
            Changed = true;
          }
        }
      }
    }
  }

  // Replace redundant instructions with their equivalents
  for (Instruction *I : toRemove) {
    // Before removing, replace uses of the instruction
    if (replacements.count(I)) {
      llvm::outs() << "Replacing: " << *I
                  << "\n  With: " << *replacements[I] << "\n";
      I->replaceAllUsesWith(replacements[I]);
      I->eraseFromParent();
    }
  }

  // Print statistics
  if (Changed) {
    outs() << "GVN pass: processed " << NumGVNInstructions
           << " instructions, removed " << NumGVNRedundant
           << " redundant computations.\n";
  } else {
    outs() << "GVN pass: no changes made.\n";
  }

  // If the function was changed, invalidate analyses
  if (Changed)
    return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}

//------------------------------------------------------------------------------
// New PM Registration
//------------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getGVNPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "GVN", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // Register analysis dependencies
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  // Register the DominatorTree analysis pass
                  FAM.registerPass([&] { return DominatorTreeAnalysis(); });
                });

            // Register for function pass manager
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "demo-gvn") {
                    FPM.addPass(GVN());
                    return true;
                  }
                  return false;
                });

            // Also register as a module pass adapter
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "demo-gvn") {
                    MPM.addPass(createModuleToFunctionPassAdaptor(GVN()));
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize GVN when added to the pass pipeline on the
// command line, i.e. via '-passes=demo-gvn'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getGVNPluginInfo();
}
