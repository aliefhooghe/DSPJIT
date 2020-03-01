
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"

#include "llvm/Support/TargetSelect.h"

#include "ir_optimization.h"

namespace DSPJIT {

    void run_optimization(llvm::Module& m, llvm::Function& func)
    {
        llvm::legacy::FunctionPassManager pm{&m};
        llvm::PassManagerBuilder pm_builder{};

        pm_builder.OptLevel = 3;
        pm_builder.populateFunctionPassManager(pm);

        pm.doInitialization();
        pm.run(func);
        pm.doFinalization();
    }

}