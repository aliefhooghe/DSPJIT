#include <sstream>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Linker/Linker.h>

#include "external_plugin.h"
#include "graph_compiler.h"
#include "ir_helper.h"

#include <iostream>

namespace DSPJIT {

    //  Helper for name prefixing
    static auto ptr_2_string(const void* ptr)
    {
        std::stringstream ss;
        ss << ptr;
        return  ss.str();
    }

    //
    class external_plugin_node : public compile_node_class {

    public:
        explicit external_plugin_node(
            const external_plugin::process_info& info,
            const std::string& process_symbol,
            const std::optional<std::string>& initialize_symbol);

        void initialize_mutable_state(
            llvm::IRBuilder<>& builder,
            llvm::Value *mutable_state, llvm::Value*) const override;

        std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state_ptr, llvm::Value*) const override;
    private:
        llvm::Value *_convert_ptr_arg(llvm::IRBuilder<>& builder, const llvm::Function *func, int arg_index, llvm::Value *ptr) const;

        const std::string _process_symbol;
        const std::optional<std::string> _initialize_symbol;
    };

    external_plugin_node::external_plugin_node(
        const external_plugin::process_info& info,
        const std::string& process_symbol,
        const std::optional<std::string>& initialize_symbol)
    :   compile_node_class{
            info.input_count, info.output_count,
            info.mutable_state_size, info.use_static_memory},
        _process_symbol{process_symbol},
        _initialize_symbol{initialize_symbol}
    {
    }

    std::vector<llvm::Value*> external_plugin_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *mutable_state_ptr, llvm::Value *static_mem) const
    {
        auto& builder = compiler.builder();
        auto module = builder.GetInsertBlock()->getModule();
        auto function = module->getFunction(_process_symbol);

        if (function == nullptr)
            throw std::runtime_error("DSPJIT : external_plugin_node : process symbol not found in module");

        auto func_type = function->getFunctionType();
        const auto input_count = get_input_count();
        const auto output_count = get_output_count();

        //  Allocate outputs
        std::vector<llvm::Value*> outputs_ptr{output_count};
        std::generate(
            outputs_ptr.begin(), outputs_ptr.end(),
            [&builder]() { return builder.CreateAlloca(builder.getFloatTy()); });

        //  Call process func
        std::vector<llvm::Value*> arg_values{};

        // Add static memory if needed
        if (use_static_memory)
            arg_values.push_back(_convert_ptr_arg(builder, function, arg_values.size(), static_mem));

        // Add state pointer if needed
        if (mutable_state_size > 0u)
            arg_values.push_back(_convert_ptr_arg(builder, function, arg_values.size(), mutable_state_ptr));

        //  Add I/O arguments
        for (auto i = 0u; i < input_count; ++i)  arg_values.push_back(inputs[i]);
        for (auto i = 0u; i < output_count; ++i) arg_values.push_back(outputs_ptr[i]);

        //  Create call instruction
        builder.CreateCall(function, arg_values);

        //  Load and return output values
        std::vector<llvm::Value*> output_values{output_count};
        for (auto i = 0u; i < output_count; ++i)
            output_values[i] = builder.CreateLoad(outputs_ptr[i]);

        return output_values;
    }

    void external_plugin_node::initialize_mutable_state(
            llvm::IRBuilder<>& builder,
        llvm::Value *mutable_state, llvm::Value *static_mem) const
    {
        if (mutable_state_size == 0)
            return;

        const auto initialize_symbol = _initialize_symbol.value();
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

    llvm::Value *external_plugin_node::_convert_ptr_arg(llvm::IRBuilder<>& builder, const llvm::Function *func, int arg_index, llvm::Value *ptr) const
    {
        const auto arg_type = func->getFunctionType()->getFunctionParamType(arg_index);
        return builder.CreatePointerCast(ptr, arg_type);
    }

    /*
     *  External Plugin implementation
     */
    external_plugin::external_plugin(std::unique_ptr<llvm::Module>&& module)
    :   _module{std::move(module)}
    {
        const auto symbol_prefix = "plugin__" + ptr_2_string(this) + "__";

        process_info proc_info{};
        std::optional<initialization_info> init_info{};
        bool found_process_function = false;

        // Apply prefix and search for API functions
        for (auto& function : *_module) {
            // Ignore declarations
            if (function.isDeclaration())
                continue;

            // External plugins modules functions symbols which are not declarations (from external libs)
            // are prefixed in order to avoid name collisions
            const auto new_name = symbol_prefix + function.getName();

            if (!found_process_function && function.getName().equals(process_func_symbol)) {
                proc_info = _read_process_func(function);
                found_process_function = true;
                _mangled_process_func_symbol = new_name.str();
                LOG_DEBUG("[DSPJIT][external plugin] Found process function : input_count : %u, output count : %u, mutable_state_size : %llu, use_static_mem : %s\n",
                    proc_info.input_count, proc_info.output_count, proc_info.mutable_state_size, proc_info.use_static_memory ? "true" : "false");
            }
            else if (!_mangled_initialize_func_symbol && function.getName().equals(initialize_func_symbol)) {
                _mangled_initialize_func_symbol = new_name.str();
                init_info = _read_initialize_func(function);
            }

            // Add prefix to function name
            function.setName(new_name);

            // Remove all function attributes as they tend to prevent inlining
            function.setAttributes(llvm::AttributeList{});
        }

        // Check consistency between init and process functions
        if (!found_process_function) {
            throw std::invalid_argument("external plugin : process function was not found int the external module");
        }
        else if (_check_consistency(proc_info, init_info)) {
            _proc_info = proc_info;
        }
        else {
            throw std::invalid_argument("external plugin : initialize and process function are not consistent");
        }
    }

    std::unique_ptr<llvm::Module> external_plugin::create_module()
    {
        return llvm::CloneModule(*_module);
    }

    std::unique_ptr<compile_node_class> external_plugin::create_node() const
    {
        return std::make_unique<external_plugin_node>(
            _proc_info,
            _mangled_process_func_symbol,
            _mangled_initialize_func_symbol);
    }

    external_plugin::process_info external_plugin::_read_process_func(const llvm::Function& function) const
    {
        const auto argument_count = function.getFunctionType()->getFunctionNumParams();

        if (argument_count < 1)
            throw std::invalid_argument("external plugin process function does not have enough arguments");

        std::size_t mutable_state_size = 0u;
        bool use_static_mem = false;
        auto arg_index = 0u;

        // Try read a static mem pointer
        if (_is_static_mem(function.getArg(arg_index))) {
            use_static_mem = true;
            arg_index++;
        }

        // Try read a mutable state pointer
        if (_is_mutable_state(function.getArg(arg_index), mutable_state_size)) {
            arg_index++;
        }

        // Read and check Input/Output parameters
        auto input_count = 0u;
        auto output_count = 0u;

        for (; arg_index < argument_count; ++arg_index) {
            if (_is_input(function.getArg(arg_index)))
                input_count++;
            else
                break;

        }
        for (; arg_index < argument_count; ++arg_index) {
            if (_is_output(function.getArg(arg_index)))
                output_count++;
            else
                break;
        }

        if (arg_index != argument_count)
            throw std::invalid_argument("external plugin : process function does not have a compatible signature");

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
        // static mem is a const ptr
        return arg->getType()->isPointerTy() &&
            (arg->hasAttribute(llvm::Attribute::AttrKind::ReadOnly) ||
                arg->hasAttribute(llvm::Attribute::AttrKind::ReadNone));
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
}