
#include <DSPJIT/graph_execution_context_factory.h>

namespace DSPJIT
{
    graph_execution_context graph_execution_context_factory::build(
        llvm::LLVMContext& llvm_context,
        llvm::CodeGenOpt::Level opt_level,
        const llvm::TargetOptions& target_options,
        const std::size_t instance_count)
    {
        auto execution_engine =
            std::make_unique<llvm_legacy_execution_engine>(
                llvm_context,
                opt_level,
                target_options);

        auto memory_manager =
            std::make_unique<graph_memory_manager>(
                llvm_context,
                instance_count,
                0u);

        return graph_execution_context{
            std::move(execution_engine),
            std::move(memory_manager)
        };
    }
}