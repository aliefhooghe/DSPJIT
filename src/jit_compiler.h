#ifndef JITTEST_JIT_COMPILER_H
#define JITTEST_JIT_COMPILER_H

#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include <memory>

namespace jit_test {

    std::unique_ptr<llvm::ExecutionEngine> build_execution_engine(std::unique_ptr<llvm::Module> && module);

}

#endif