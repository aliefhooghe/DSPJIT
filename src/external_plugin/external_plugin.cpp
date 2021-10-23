
#include <sstream>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Linker/Linker.h>

#include <DSPJIT/external_plugin.h>
#include <DSPJIT/graph_compiler.h>
#include <DSPJIT/ir_helper.h>
#include <DSPJIT/log.h>

#include "external_plugin_node.h"

namespace DSPJIT {

    //  Helper for name prefixing
    static auto ptr_2_string(const void* ptr)
    {
        std::stringstream ss;
        ss << ptr;
        return  ss.str();
    }

    /*
     *  External Plugin implementation
     */
    external_plugin::external_plugin(std::unique_ptr<llvm::Module>&& module)
    :   _module{std::move(module)}
    {
        const auto symbol_prefix = "plugin__" + ptr_2_string(this) + "__";
        const auto rename_function = [symbol_prefix] (const auto& name) { return symbol_prefix + name; };

        // Information to be retrieved
        std::array<std::optional<process_info>, compute_type_count> found_compute_funcs{};
        std::optional<initialization_info> found_initialization_func{};

        // Apply prefix and search for API functions
        for (auto& function : *_module) {
            // Ignore declarations
            if (function.isDeclaration())
                continue;

            // External plugins modules functions symbols which are not declarations (from external libs)
            // are prefixed in order to avoid name collisions
            const auto function_name = function.getName();
            const auto new_name = rename_function(function_name);

            // Check if function is a compute function
            for (auto type = 0u; type < compute_type_count; ++type) {
                if (!found_compute_funcs[type].has_value() && function_name.equals(_compute_functions_symbols[type])) {
                    found_compute_funcs[type] = _read_compute_func(function, static_cast<compute_type>(type));
                    _log_compute_function(_compute_functions_symbols[type], found_compute_funcs[type].value());
                    break;
                }
            }

            // Check if function is a initialize function
            if (!found_initialization_func.has_value() && function_name.equals(_initialize_symbol)) {
                found_initialization_func = _read_initialize_func(function);
            }

            // Add prefix to function name
            function.setName(new_name);

            // Remove all function attributes as they tend to prevent inlining
            function.setAttributes(llvm::AttributeList{});
        }

        // Check consistency
        const auto& process_func = found_compute_funcs[static_cast<unsigned int>(compute_type::PROCESS)];
        const auto& push_func = found_compute_funcs[static_cast<unsigned int>(compute_type::PUSH)];
        const auto& pull_func = found_compute_funcs[static_cast<unsigned int>(compute_type::PULL)];

        // Api consistency
        if (!(process_func.has_value() ^ (push_func.has_value() && pull_func.has_value())))
            throw std::runtime_error("external_plugin::external_plugin: One and only one compute api must be provided");

        // Mutable state/static chunk usage consistency
        if (process_func.has_value()) {
            // Dependant process
            const auto& process_info = process_func.value();

            if (!_check_consistency(process_info, found_initialization_func))
                throw std::invalid_argument("external plugin : initialize and process functions are not consistent");

            _proc_info = process_info;
            _symbols.initialize_symbol = rename_function(_initialize_symbol);
            _symbols.compute_symbols =
                dependant_process_symbol
                {
                    rename_function(_compute_functions_symbols[static_cast<unsigned int>(compute_type::PROCESS)])
                };
        }
        else {
            // Non dependant process
            const auto& push_info = push_func.value();
            const auto& pull_info = pull_func.value();

            if (!_check_consistency(push_info, pull_info))
                throw std::invalid_argument("external plugin : push and pull functions are not consistent");
            else if (!_check_consistency(push_info, found_initialization_func))
                throw std::invalid_argument("external plugin : initialize and push/pull functions are not consistent");

            _proc_info =
            {
                push_info.input_count,
                pull_info.output_count,
                push_info.mutable_state_size, // could be pull_info
                push_info.use_static_memory    // here too
            };
            _symbols.initialize_symbol = rename_function(_initialize_symbol);
            _symbols.compute_symbols =
                nondependant_process_symbols
                {
                    rename_function(_compute_functions_symbols[static_cast<unsigned int>(compute_type::PUSH)]),
                    rename_function(_compute_functions_symbols[static_cast<unsigned int>(compute_type::PULL)])
                };
        }
    }

    std::unique_ptr<llvm::Module> external_plugin::create_module()
    {
        return llvm::CloneModule(*_module);
    }

    std::unique_ptr<compile_node_class> external_plugin::create_node() const
    {
        return std::make_unique<external_plugin_node>(_proc_info, _symbols);
    }

    external_plugin::process_info external_plugin::_read_compute_func(const llvm::Function& function, compute_type type) const
    {
        const auto argument_count = function.getFunctionType()->getFunctionNumParams();

        if (argument_count < 1)
            throw std::invalid_argument("external plugin: compute function does not have enough arguments");

        std::size_t mutable_state_size = 0u;
        bool use_static_mem = false;
        auto arg_index = _try_read_state_and_static_chunk_args(
            function, use_static_mem, mutable_state_size);

        // Read and check Input/Output parameters
        auto input_count = 0u;
        auto output_count = 0u;

        if (type != compute_type::PULL) {
            for (; arg_index < argument_count; ++arg_index) {
                if (_is_input(function.getArg(arg_index)))
                    input_count++;
                else
                    break;
            }
        }
        if (type != compute_type::PUSH) {
            for (; arg_index < argument_count; ++arg_index) {
                if (_is_output(function.getArg(arg_index)))
                    output_count++;
                else
                    break;
            }
        }

        if (arg_index != argument_count)
            throw std::invalid_argument("external plugin: compute function does not have a compatible signature");

        return {
            input_count,
            output_count,
            mutable_state_size,
            use_static_mem
        };
    }

    external_plugin::initialization_info external_plugin::_read_initialize_func(const llvm::Function& function) const
    {
        std::size_t mutable_state_size = 0u;
        const auto argument_count = function.getFunctionType()->getFunctionNumParams();

        if (argument_count == 1u && _is_mutable_state(function.getArg(0u), mutable_state_size))
        {
            return {
                mutable_state_size,
                false
            };
        }
        else if (argument_count == 2u && _is_static_mem(function.getArg(0u)) &&
            _is_mutable_state(function.getArg(1u), mutable_state_size))
        {
            return {
                mutable_state_size,
                true
            };
        }
        else {
            throw std::invalid_argument("external plugin : invalid initialize function signature");
        }
    }

    unsigned int external_plugin::_try_read_state_and_static_chunk_args(
        const llvm::Function& function,
        bool& use_static_mem,
        std::size_t& mutable_state_size) const
    {
        auto arg_index = 0u;
        mutable_state_size = 0u;
        use_static_mem = false;

        if (_is_static_mem(function.getArg(0u))) {

            if (_is_mutable_state(function.getArg(1u), mutable_state_size)) {
                use_static_mem = true;
                arg_index = 2u;
            }
            else if (_is_mutable_state(function.getArg(0u), mutable_state_size)) {
                arg_index = 1u;
            }
            else {
                throw std::invalid_argument("external plugin : process function provide use a static memory chunk without a valid mutable state");
            }
        }

        return arg_index;
    }

    bool external_plugin::_is_mutable_state(const llvm::Argument *arg, std::size_t& state_size) const
    {
        const auto ptr_type = llvm::dyn_cast<llvm::PointerType>(arg->getType());

        if (ptr_type != nullptr) {
            const auto state_type = ptr_type->getElementType();

            // Mutable state can not be a float
            if (state_type->isSized() && !state_type->isFloatTy()) {
                const auto& data_layout = _module->getDataLayout();
                state_size = data_layout.getTypeAllocSize(state_type).getFixedSize();
                return true;
            }
        }

        return false;
    }

    bool external_plugin::_is_static_mem(const llvm::Argument *arg) const
    {
        const auto ptr_type = llvm::dyn_cast<llvm::PointerType>(arg->getType());

        if (ptr_type != nullptr) {
            const auto state_type = ptr_type->getElementType();
            return !state_type->isFloatTy();
        }
        else {
            return false;
        }
    }

    bool external_plugin::_is_input(const llvm::Argument *arg) const
    {
        return arg->getType()->isFloatTy();
    }

    bool external_plugin::_is_output(const llvm::Argument *arg) const
    {
        const auto ptr_type = llvm::dyn_cast<llvm::PointerType>(arg->getType());
        return ptr_type && ptr_type->getElementType()->isFloatTy();
    }

    bool external_plugin::_check_consistency(
        const process_info& proc_info,
        const std::optional<initialization_info>& init_info) const
    {
        // if an initialization function was provided
        if (init_info.has_value()) {
            const auto& init = init_info.value();
            return (proc_info.use_static_memory == init.use_static_memory &&
                    proc_info.mutable_state_size == init.mutable_state_size &&
                    proc_info.mutable_state_size != 0u);
        }
        else {
            return (proc_info.mutable_state_size == 0u);
        }
    }

    bool external_plugin::_check_consistency(
        const process_info& push_proc_info,
        const process_info& pull_proc_info) const
    {
        return (
            push_proc_info.mutable_state_size == pull_proc_info.mutable_state_size &&
            push_proc_info.use_static_memory == pull_proc_info.use_static_memory);
    }

    void external_plugin::_log_compute_function(const char *name, const process_info& proc_info)
    {
        LOG_DEBUG("[DSPJIT][external plugin] Found '%s' function : input_count : %u, output count : %u, mutable_state_size : %llu, use_static_mem : %s\n",
            name, proc_info.input_count, proc_info.output_count, proc_info.mutable_state_size, proc_info.use_static_memory ? "true" : "false");
    }

}