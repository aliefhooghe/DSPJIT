#ifndef JITTEST_IR_OPTIMIZATION_H
#define JITTEST_IR_OPTIMIZATION_H

#include <llvm/IR/Module.h>

namespace DSPJIT {

    void run_optimization(llvm::Module& m);

}

#endif