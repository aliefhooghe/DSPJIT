#ifndef IR_HELPER_H_
#define IR_HELPER_H_

#include <llvm/IR/Module.h>

namespace ir_helper {

    void print_module(const llvm::Module& module);
    void print_function(const llvm::Function& function);

}

#endif