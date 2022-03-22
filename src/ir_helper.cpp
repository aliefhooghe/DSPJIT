
#include <sstream>
#include <iostream>

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_os_ostream.h>

#include <DSPJIT/log.h>
#include <DSPJIT/ir_helper.h>

namespace DSPJIT {

    void log_module(const llvm::Module& module)
    {
        std::stringstream sstream{};
        llvm::raw_os_ostream stream{ sstream };
        module.print(stream, nullptr);
        stream.flush();
        DSPJIT::log_function("%s", sstream.str().c_str());
    }

    void log_function(const llvm::Function& function)
    {
        std::stringstream sstream{};
        llvm::raw_os_ostream stream{sstream};
        function.print(stream);
        stream.flush();
        DSPJIT::log_function("%s", sstream.str().c_str());
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