#ifndef JITTEST_IR_OPTIMIZATION_H
#define JITTEST_IR_OPTIMIZATION_H

#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITEventListener.h>

#include <memory>

namespace DSPJIT {

    void run_optimization(llvm::Module& m, llvm::Function& func);

}

#endif