#ifndef DSPJIT_ABSTRACT_EXECUTION_ENGINE_H_
#define DSPJIT_ABSTRACT_EXECUTION_ENGINE_H_

#include <memory>

#include <llvm/IR/Module.h>

namespace DSPJIT
{
    /**
     * \brief This class is responsible of native code generation and management
     */
    class abstract_execution_engine
    {
    public:
        virtual ~abstract_execution_engine() noexcept = default;

        virtual void add_module(std::unique_ptr<llvm::Module>&& module) = 0;
        virtual void delete_module(llvm::Module* module) = 0;

        /**
         * \brief Ensure that all previously loaded modules are compiled to native code
         * and are ready for execution.
         */
        virtual void emit_native_code() = 0;

        /**
         * \brief Get a callable pointer to the compiled native code function
         * \param function the function to look for
         */
        virtual void* get_function_pointer(llvm::Function* function) =0;
    };
}

#endif /* DSPJIT_ABSTRACT_EXECUTION_ENGINE_H_ */