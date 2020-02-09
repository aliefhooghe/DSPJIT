
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"

#include "llvm/Support/TargetSelect.h"

#include "jit_compiler.h"

namespace jit_test {

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


    std::unique_ptr<llvm::ExecutionEngine> build_execution_engine(
        std::unique_ptr<llvm::Module> && module,
        llvm::JITEventListener *listener,
        const llvm::TargetOptions options)
    {
        static auto llvm_native_was_init = false;
        if (llvm_native_was_init == false) {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm_native_was_init = true;
        }

        auto memory_mgr = std::unique_ptr<llvm::RTDyldMemoryManager>(new llvm::SectionMemoryManager());

        auto execution_engine =
            std::unique_ptr<llvm::ExecutionEngine>(
                llvm::EngineBuilder{std::move(module)}
                .setEngineKind(llvm::EngineKind::JIT)
                .setTargetOptions(options)
                .setMCJITMemoryManager(std::move(memory_mgr))
                //.setUseOrcMCJITReplacement()
                .create());

        if (listener)
            execution_engine->RegisterJITEventListener(listener);

        execution_engine->finalizeObject();

        if (listener)
           execution_engine->UnregisterJITEventListener(listener);

        return execution_engine;
    }

}