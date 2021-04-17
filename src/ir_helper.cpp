

#include <llvm/Support/raw_os_ostream.h>
#include <sstream>
#include <iostream>

#include "ir_helper.h"

#ifdef WIN32
#include <Windows.h>
#endif

namespace ir_helper {

    void print_module(const llvm::Module& module)
    {
#ifdef WIN32
        std::stringstream sstream{};
        llvm::raw_os_ostream stream{sstream};
        module.print(stream, nullptr);
        OutputDebugString(sstream.str().c_str());
#else
        llvm::raw_os_ostream stream{std::cout};
        module.print(stream, nullptr);
#endif
    }

    void print_function(const llvm::Function& function)
    {
#ifdef WIN32
        std::stringstream sstream{};
        llvm::raw_os_ostream stream{sstream};
        function.print(stream);
        OutputDebugString(sstream.str().c_str());
#else
        llvm::raw_os_ostream stream{std::cout};
        function.print(stream);
#endif
    }

}