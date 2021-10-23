
#include <sstream>
#include <iostream>

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_os_ostream.h>

#include <DSPJIT/log.h>
#include <DSPJIT/ir_helper.h>

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

    bool check_module(const llvm::Module& module, std::string& error_string)
    {
        std::stringstream sstream{};
        llvm::raw_os_ostream stream{sstream};

        bool broken_debug_info = false;

        if (!llvm::verifyModule(module, &stream, &broken_debug_info)) {
            if (broken_debug_info)
                LOG_WARNING("[ir helper] [check_module] Found broken debug info\n");
            return false;
        }
        else {
            stream.flush();
            error_string = sstream.str();
            return true;
        }
    }

}