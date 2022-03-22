#ifndef DSPJIT_IR_HELPER_H_
#define DSPJIT_IR_HELPER_H_

#include <llvm/IR/Module.h>

namespace DSPJIT {

    void log_module(const llvm::Module& module);
    void log_function(const llvm::Function& function);
    bool check_module(const llvm::Module& module, std::string& error_string);

}

#endif