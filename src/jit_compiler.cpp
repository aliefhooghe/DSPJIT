
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/Support/TargetSelect.h"

#include "jit_compiler.h"

namespace jit_test {

    std::unique_ptr<llvm::ExecutionEngine> build_execution_engine(
        std::unique_ptr<llvm::Module> && module, const llvm::TargetOptions options)
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
                .create());

        //execution_engine->RegisterJITEventListener()

        execution_engine->finalizeObject();

        return execution_engine;
    }

}