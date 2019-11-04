
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/Support/TargetSelect.h"

#include "jit_compiler.h"

namespace jit_test {

    std::unique_ptr<llvm::ExecutionEngine> build_execution_engine(std::unique_ptr<llvm::Module> && module)
    {
        static auto llvm_native_was_init = false;
        if (llvm_native_was_init == false) {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm_native_was_init = true;
        }

        auto module_ptr = module.get();
        llvm::EngineBuilder factory(std::move(module));
        auto memory_mgr = std::unique_ptr<llvm::RTDyldMemoryManager>(new llvm::SectionMemoryManager());

        factory.setEngineKind(llvm::EngineKind::JIT);
        factory.setTargetOptions(llvm::TargetOptions{});
        factory.setMCJITMemoryManager(std::move(memory_mgr));

        auto raw_ptr = factory.create();
        auto execution_engine = std::unique_ptr<llvm::ExecutionEngine>(raw_ptr);

        execution_engine->finalizeObject();
        return execution_engine;
    }

}