

#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include "abstract_execution_engine.h"

namespace DSPJIT
{

    class llvm_legacy_execution_engine : public abstract_execution_engine
    {
    public:
        llvm_legacy_execution_engine(
            std::unique_ptr<llvm::ExecutionEngine>&& execution_engine);

        llvm_legacy_execution_engine(
            llvm::LLVMContext& llvm_context,
            llvm::CodeGenOpt::Level opt_level,
            const llvm::TargetOptions& target_options);

        void add_module(std::unique_ptr<llvm::Module>&&) override;
        void delete_module(llvm::Module*) override;
        void emit_native_code() override;
        void* get_function_pointer(llvm::Function*) override;

    private:
        std::unique_ptr<llvm::ExecutionEngine> _execution_engine;
    };

} // namespace DSPJIT
