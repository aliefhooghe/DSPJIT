#ifndef EXTERNAL_PLUGIN_NODE_H_
#define EXTERNAL_PLUGIN_NODE_H_

#include <string>
#include <vector>

#include <DSPJIT/external_plugin.h>

namespace DSPJIT {

    class external_plugin_node : public compile_node_class {

        using external_plugin_symbols = external_plugin::external_plugin_symbols;

    public:
        explicit external_plugin_node(
            const external_plugin::process_info& info,
            const external_plugin_symbols& symbols);

        void initialize_mutable_state(
            llvm::IRBuilder<>& builder,
            llvm::Value *mutable_state, llvm::Value*) const override;

        std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state_ptr, llvm::Value*) const override;

        std::vector<llvm::Value*> pull_output(
            graph_compiler& compiler,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const override;

        void push_input(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const override;

    private:
        using compute_type = external_plugin::compute_type;

        std::optional<std::vector<llvm::Value*>>
            _call_compute_function(
                const std::string& symbol,
                compute_type type,
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr, llvm::Value *static_mem) const;

        llvm::Value *_convert_ptr_arg(llvm::IRBuilder<>& builder, const llvm::Function *func, int arg_index, llvm::Value *ptr) const;

        const external_plugin_symbols _symbols;
    };

}

#endif