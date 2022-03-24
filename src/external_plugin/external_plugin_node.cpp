
#include <DSPJIT/graph_compiler.h>

#include "external_plugin_node.h"

namespace DSPJIT {
    external_plugin_node::external_plugin_node(
            const external_plugin::process_info& info,
            const external_plugin_symbols& symbols)
    :   compile_node_class{
            info.input_count, info.output_count,
            info.mutable_state_size, info.use_static_memory, symbols.is_dependant_process()},
        _symbols{symbols}
    {
        if (info.mutable_state_size != 0u && !symbols.initialize_symbol.has_value())
            throw std::runtime_error("external_plugin_node::external_plugin_node: no initialize function was provided whereas mutable_state_size > 0");
    }

    void external_plugin_node::initialize_mutable_state(
            llvm::IRBuilder<>& builder,
        llvm::Value *mutable_state, llvm::Value *static_mem) const
    {
        if (mutable_state_size == 0)
            return;

        const auto initialize_symbol = _symbols.initialize_symbol.value();
        auto module = builder.GetInsertBlock()->getModule();
        auto function = module->getFunction(initialize_symbol);

        if (function == nullptr)
            throw std::runtime_error("DSPJIT : external_plugin_node : initialization function symbol not found");

        std::vector<llvm::Value*> arg_values{};

        //  Create the static memory chunk argument if needed
        if (use_static_memory)
            arg_values.push_back(_convert_ptr_arg(builder, function, 0, static_mem));

        //  Create the mutable state argument
        arg_values.push_back(_convert_ptr_arg(builder, function, arg_values.size(), mutable_state));

        builder.CreateCall(function, arg_values);
    }

    std::vector<llvm::Value*> external_plugin_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *mutable_state_ptr, llvm::Value *static_mem) const
    {
        if (dependant_process) {
            auto& builder = compiler.builder();
            auto module = builder.GetInsertBlock()->getModule();
            const auto& process_symbol =
                std::get<external_plugin::dependant_process_symbol>(_symbols.compute_symbols).process_symbol;

            const auto result = _call_compute_function(
                process_symbol, compute_type::PROCESS, compiler, inputs, mutable_state_ptr, static_mem);

            return result.value();
        }
        else {
            throw std::runtime_error("external_plugin_node::emit_outputs: Bad compute api was called for non dependant node");
        }
    }

    std::vector<llvm::Value*> external_plugin_node::pull_output(
        graph_compiler& compiler,
        llvm::Value *mutable_state,
        llvm::Value *static_memory) const
    {
        if (!dependant_process) {
            auto& builder = compiler.builder();
            auto module = builder.GetInsertBlock()->getModule();
            const auto& pull_symbol =
                std::get<external_plugin::nondependant_process_symbols>(_symbols.compute_symbols).pull_symbol;

            const auto result = _call_compute_function(pull_symbol, compute_type::PULL, compiler, {}, mutable_state, static_memory);
            return result.value();
        }
        else {
            throw std::runtime_error("external_plugin_node::pull_outputs: Bad compute api was called for dependant node");
        }
    }

    void external_plugin_node::push_input(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *mutable_state,
        llvm::Value *static_memory) const
    {
        if (!dependant_process) {
            auto& builder = compiler.builder();
            auto module = builder.GetInsertBlock()->getModule();
            const auto& push_symbol =
                std::get<external_plugin::nondependant_process_symbols>(_symbols.compute_symbols).push_symbol;

            _call_compute_function(push_symbol, compute_type::PUSH, compiler, inputs, mutable_state, static_memory);
        }
        else {
            throw std::runtime_error("external_plugin_node::push_outputs: Bad compute api was called for dependant node");
        }
    }

    std::optional<std::vector<llvm::Value*>>
        external_plugin_node::_call_compute_function(
            const std::string& symbol,
            compute_type type, graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state_ptr, llvm::Value *static_mem) const
    {
        auto& builder = compiler.builder();
        auto module = builder.GetInsertBlock()->getModule();
        auto function = module->getFunction(symbol);

        if (function == nullptr)
            throw std::runtime_error("DSPJIT : external_plugin_node : process symbol not found in module");

        auto func_type = function->getFunctionType();
        const auto input_count = get_input_count();
        const auto output_count = get_output_count();

        std::vector<llvm::Value*> outputs_ptr{};

        //  Allocate outputs
        if (type != compute_type::PUSH) {
            outputs_ptr.resize(output_count);
            std::generate(
                outputs_ptr.begin(), outputs_ptr.end(),
                [&builder]() { return builder.CreateAlloca(builder.getFloatTy()); });
        }

        //  Call process func
        std::vector<llvm::Value*> arg_values{};

        // Add static memory if needed
        if (use_static_memory)
            arg_values.push_back(_convert_ptr_arg(builder, function, arg_values.size(), static_mem));

        // Add state pointer if needed
        if (mutable_state_size > 0u)
            arg_values.push_back(_convert_ptr_arg(builder, function, arg_values.size(), mutable_state_ptr));

        //  Add I/O arguments
        if (type != compute_type::PULL)
        {
            for (auto i = 0u; i < input_count; ++i)  arg_values.push_back(inputs[i]);
        }

        if (type != compute_type::PUSH)
        {
            for (auto i = 0u; i < output_count; ++i) arg_values.push_back(outputs_ptr[i]);
        }

        //  Create call instruction
        builder.CreateCall(function, arg_values);

        //  Load and return output values
        if (type != compute_type::PUSH) {
            std::vector<llvm::Value*> output_values{output_count};
            for (auto i = 0u; i < output_count; ++i)
                output_values[i] = builder.CreateLoad(builder.getFloatTy(), outputs_ptr[i]);
            return output_values;
        }
        else {
            return std::nullopt;
        }
    }

    llvm::Value *external_plugin_node::_convert_ptr_arg(llvm::IRBuilder<>& builder, const llvm::Function *func, int arg_index, llvm::Value *ptr) const
    {
        const auto arg_type = func->getFunctionType()->getFunctionParamType(arg_index);
        return builder.CreatePointerCast(ptr, arg_type);
    }

}