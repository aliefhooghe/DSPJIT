
#include <llvm/Support/raw_os_ostream.h>
#include <iostream>

#include "ir_helper.h"

namespace ir_helper {

    void print_module(const llvm::Module& module)
    {
        llvm::raw_os_ostream stream{std::cout};

        for (const auto& func : module)
            func.print(stream);

        std::cout << std::endl;
    }

    void print_function(const llvm::Function& function)
    {
        llvm::raw_os_ostream stream{std::cout};
        function.print(stream);
    }

}