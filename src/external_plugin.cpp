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
            unsigned int input_count,
            unsigned int output_count,
            const std::string& process_symbol,
            const std::optional<std::string>& initialize_symbol,
            std::size_t mutable_state_size);

        void initialize_mutable_state(
            llvm::IRBuilder<>& builder,
            llvm::Value *mutable_state) const override;

        std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state_ptr) const override;
    private:
        const std::string _process_symbol;
        const std::optional<std::string> _initialize_symbol;
    };

    external_plugin_node::external_plugin_node(
        unsigned int input_count,
        unsigned int output_count,
        const std::string& process_symbol,
        const std::optional<std::string>& initialize_symbol,
        std::size_t mutable_state_size)
    :   compile_node_class{input_count, output_count, mutable_state_size},
        _process_symbol{process_symbol},
        _initialize_symbol{initialize_symbol}
    {
    }

    std::vector<llvm::Value*> external_plugin_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *mutable_state_ptr) const
    {
        auto& builder = compiler.builder();
        const auto input_count = get_input_count();
        const auto output_count = get_output_count();

        auto module = builder.GetInsertBlock()->getModule();
        auto function = module->getFunction(_process_symbol);

        if (function == nullptr) {
            LOG_ERROR("[external_plugin_node] Can't find symbol '%s', graph_execution_context was not setup\n", _process_symbol.c_str());
            throw std::runtime_error("DSPJIT : external_plugin_node : symbol not found");
        }

        //  Allocate outputs
        std::vector<llvm::Value*> outputs_ptr{output_count};

        for (auto i = 0u; i < output_count; ++i)
            outputs_ptr[i] = builder.CreateAlloca(builder.getFloatTy());

        //  Call process func
        std::vector<llvm::Value*> arg_values{};

        // Add state argument if any
        if (mutable_state_ptr != nullptr) {
            const auto state_arg_type =
                function->getFunctionType()->getFunctionParamType(0);
            const auto state_arg =
                compiler.builder().CreatePointerCast(mutable_state_ptr, state_arg_type);
            arg_values.push_back(state_arg);
        }

        //  Add I/O arguments
        for (auto i = 0u; i < input_count; ++i)
            arg_values.push_back(inputs[i]);
        for (auto i = 0u; i < output_count; ++i)
            arg_values.push_back(outputs_ptr[i]);

        //  Create call instruction
        builder.CreateCall(function, arg_values);

        //  Get and return Output values
        std::vector<llvm::Value*> output_values{output_count};
        for (auto i = 0u; i < output_count; ++i)
            output_values[i] = builder.CreateLoad(outputs_ptr[i]);

        return output_values;
    }

    void external_plugin_node::initialize_mutable_state(
            llvm::IRBuilder<>& builder,
        llvm::Value *mutable_state) const
    {
        if (!_initialize_symbol.has_value() || mutable_state == nullptr)
            return;

        const auto initialize_symbol = _initialize_symbol.value();
        auto module = builder.GetInsertBlock()->getModule();
        auto function = module->getFunction(initialize_symbol);

        if (function == nullptr) {
            LOG_ERROR("[external_plugin_node] Can't find symbol '%s', graph_execution_context was not setup\n", initialize_symbol.c_str());
            throw std::runtime_error("DSPJIT : external_plugin_node : symbol not found");
        }

        //  Call the initialize function on the given state
        const auto state_arg_type = function->getFunctionType()->getFunctionParamType(0);
        const auto state_arg = builder.CreatePointerCast(mutable_state, state_arg_type);
        builder.CreateCall(function, {state_arg});
    }

    /*
     *  External Plugin implementation
     */

    external_plugin::external_plugin(
        llvm::LLVMContext &llvm_context,
        const std::vector<std::filesystem::path>& code_object_paths)
    {
        /*
         *  external plugins modules function symbols which are not declaration (from external libs)
         *  are prefixed in order to avoid name collisions
         */
        const auto symbol_prefix = "plugin__" + ptr_2_string(this) + "__";
        bool process_func_found = false;
        std::vector<std::unique_ptr<llvm::Module>> modules;

        for (const auto& obj_path : code_object_paths) {
            //  Load the module object from file
            const auto obj_path_string = obj_path.string();
            llvm::SMDiagnostic error;

            LOG_DEBUG("[external_plugin] Loading module %s\n", obj_path_string.c_str());
            auto module = llvm::parseIRFile(obj_path_string, error, llvm_context);
            if (!module) {
                LOG_ERROR("[external_plugin] Cannot load object %s\n", obj_path_string.c_str());
                throw std::runtime_error("DSPJIT : Failed to load object");
            }

            /*
             * Rename all functions defined in the module in order to avoid symbol colision
             * Find process and initialize function in the module
             */
            for(auto& function : *module) {
                //  Ingore declaration (there are typically libs functions)
                if (function.isDeclaration())
                    continue;

                const auto new_name = symbol_prefix + function.getName();

                if (function.getName().equals(process_func_symbol)) {
                    if (process_func_found) {
                        LOG_WARNING("[external_plugin] Warning : duplicate process symbol %s\n", process_func_symbol);
                        break;
                    }

                    process_func_found = true;
                    _mangled_process_func_symbol = new_name.str();

                    //  Check that process function match needed interface
                    if (_read_process_func_type(*module, function.getFunctionType())) {
                        LOG_DEBUG("[external_plugin] Found process symbol : %u input(s), %u output(s), state size = %llu\n",
                            _input_count, _output_count, _mutable_state_size);
                    }
                    else {
                        LOG_ERROR("[external_plugin] process function does not match the required interface\n");
                        throw std::runtime_error("DSPJIT::external plugin bad process func interface");
                    }
                }
                else if (function.getName().equals(initialize_func_symbol)) {
                    _mangled_initialize_func_symbol = new_name.str();

                    //  Check that initialize function match needed interface
                    if (_check_initialize_func_type(*module, function.getFunctionType())) {
                        LOG_DEBUG("[external_plugin] Found initialize symbol\n");
                    }
                    else {
                        LOG_ERROR("[external_plugin] process function does not match the required interface\n");
                        throw std::runtime_error("DSPJIT::external plugin bad process func interface");
                    }
                }

                //  Rename the function in order to isolate this plugin's functions in a namespace
                function.setName(new_name);

                //  Remove all function attributes as they can prevent optimization such as inlining
                function.setAttributes(
                    llvm::AttributeList::get(llvm_context, llvm::ArrayRef<llvm::AttributeList>{}));
            }

            modules.emplace_back(std::move(module));
        }

        if (!process_func_found) {
            LOG_ERROR("[external_plugin] Symbol '%s' not found in plugin\n", process_func_symbol);
            throw std::runtime_error("DSPJIT : external_plugin : symbol not found");
        }
        else if (!_mangled_initialize_func_symbol.has_value() && _mutable_state_size > 0u) {
            LOG_WARNING("[external_plugin] Plugin does not provide an initialize function whereas its state have a non zero size (%u bytes)\n",
                _mutable_state_size);
        }

        //  link all modules together
        const auto module_count = modules.size();

        for (auto i = 1u; i < module_count; ++i)
            llvm::Linker::linkModules(*(modules[0]), std::move(modules[i]));

        _module = std::move(modules[0]);
    }

    std::unique_ptr<llvm::Module> external_plugin::module()
    {
        return llvm::CloneModule(*_module);
    }

    std::unique_ptr<compile_node_class> external_plugin::create_node() const
    {
        return std::make_unique<external_plugin_node>(
            _input_count,
            _output_count,
            _mangled_process_func_symbol,
            _mangled_initialize_func_symbol,
            _mutable_state_size);
    }

    bool external_plugin::_read_process_func_type(const llvm::Module& module, const llvm::FunctionType *func_type)
    {
        const auto param_count = func_type->getFunctionNumParams();

        if (param_count <= 1)
            return false;

        //  Todo check first arg type
        auto index = 0u;
        const auto state_type = _read_state_type(func_type->getFunctionParamType(index));

        if (state_type) {
            const auto& data_layout = module.getDataLayout();
            _mutable_state_size = data_layout.getTypeAllocSize(state_type).getFixedSize();
            index++;
        }

        //  [state *,] float ...inputs, float *...outputs
        _input_count = 0u;
        _output_count = 0u;

        auto is_float_ptr = [] (const llvm::Type *t) {
            const auto ptr_type = llvm::dyn_cast<llvm::PointerType>(t);
            return ptr_type && ptr_type->getElementType()->isFloatTy();
        };

        //  Read inputs types : float...
        for (; index < param_count; ++index) {
            const auto param_type = func_type->getFunctionParamType(index);

            if (param_type->isFloatTy()) {
                _input_count++;
            }
            else if (is_float_ptr(param_type)) {
                break;
            }
            else {
                return false;
            }
        }

        //  Read outputs types : float*
        for (; index < param_count; ++index) {
            const auto param_type = func_type->getFunctionParamType(index);

            if(!is_float_ptr(param_type)) {
                return false;
            }

            _output_count++;
        }

        return true;
    }

    bool external_plugin::_check_initialize_func_type(const llvm::Module&, const llvm::FunctionType* func_type)
    {
        //  Check that the state type is relevant (todo check type coherance)
        return (func_type->getFunctionNumParams() == 1u) &&
            (_read_state_type(func_type->getFunctionParamType(0)) != nullptr);
    }

    llvm::Type *external_plugin::_read_state_type(const llvm::Type *param_type)
    {
        if (param_type->isPointerTy()) {
            const auto ptr_type = llvm::dyn_cast<llvm::PointerType>(param_type);
            const auto state_type = ptr_type->getElementType();

            if (state_type->isSized())
                return state_type;
        }

        return nullptr;
    }

}