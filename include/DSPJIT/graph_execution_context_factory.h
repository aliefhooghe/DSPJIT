#ifndef DSPJIT_GRAPH_EXECUTION_CONTEXT_FACTORY_H_
#define DSPJIT_GRAPH_EXECUTION_CONTEXT_FACTORY_H_

#include "llvm_legacy_execution_engine.h"
#include "graph_memory_manager.h"
#include "graph_execution_context.h"

namespace DSPJIT
{
    class graph_execution_context_factory
    {
        public:
            static graph_execution_context build(
                llvm::LLVMContext& llvm_context,
                llvm::CodeGenOpt::Level opt_level = llvm::CodeGenOpt::Level::Default,
                const llvm::TargetOptions& target_options = {},
                const std::size_t instance_count = 1u);
    };
}

#endif