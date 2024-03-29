
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/IPO.h>

#include "ir_optimization.h"

namespace DSPJIT {

    void run_optimization(llvm::Module& m)
    {
        llvm::legacy::PassManager pm{};
        pm.add(llvm::createFunctionInliningPass());
        pm.add(llvm::createEarlyCSEPass());
        pm.add(llvm::createReassociatePass());
        pm.add(llvm::createIPSCCPPass());
        pm.add(llvm::createDeadCodeEliminationPass());
        pm.add(llvm::createPromoteMemoryToRegisterPass());
        pm.add(llvm::createAggressiveDCEPass());
        pm.add(llvm::createGlobalDCEPass());
        pm.run(m);
    }
}
