#ifndef JITTEST_JIT_COMPILER_H
#define JITTEST_JIT_COMPILER_H

#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITEventListener.h>

#include <memory>

namespace jit_test {

    void run_optimization(llvm::Module& m, llvm::Function& func);

    std::unique_ptr<llvm::ExecutionEngine> build_execution_engine(
        std::unique_ptr<llvm::Module> && module,
        llvm::JITEventListener *listener = nullptr,
        const llvm::TargetOptions options = {});

}

#endif