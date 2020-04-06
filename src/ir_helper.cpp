
#include <llvm/Support/raw_os_ostream.h>
#include <iostream>

#include "ir_helper.h"

namespace ir_helper {

    void print_module(const llvm::Module& module)
    {
        llvm::raw_os_ostream stream{std::cout};
        module.print(stream, nullptr);
    }

}