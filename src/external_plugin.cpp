#include <sstream>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include "external_plugin.h"
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
            const std::string& symbol,
            std::size_t mutable_state_size);

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override;
    private:
        const std::string _symbol;
    };

    external_plugin_node::external_plugin_node(
        unsigned int input_count,
        unsigned int output_count,
        const std::string& symbol,
        std::size_t mutable_state_size)
    :   compile_node_class{input_count, output_count, mutable_state_size},
        _symbol{symbol}
    {
    }

    std::vector<llvm::Value*> external_plugin_node::emit_outputs(
        llvm::IRBuilder<>& builder,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *mutable_state_ptr) const
    {
        const auto input_count = get_input_count();
        const auto output_count = get_output_count();

        auto module = builder.GetInsertBlock()->getModule();
        auto function = module->getFunction(_symbol);

        if (function == nullptr) {
            LOG_ERROR("[external_plugin_node] Can't find symbol '%s', graph_execution_context was not setup\n", _symbol.c_str())
            throw std::runtime_error("DSPJIT : external_plugin_node : symbol not found");
        }

        //  Allocate outputs
        std::vector<llvm::Value*> outputs_ptr{output_count};

        for (auto i = 0u; i < output_count; ++i)
            outputs_ptr[i] = builder.CreateAlloca(builder.getFloatTy());

        //  Call process func
        std::vector<llvm::Value*> arg_values{};

        arg_values.push_back(mutable_state_ptr != nullptr ?
            builder.CreateIntToPtr(mutable_state_ptr, builder.getInt8PtrTy()) :
            llvm::ConstantPointerNull::get(builder.getInt8PtrTy()));

        //  Get arguments
        for (auto i = 0u; i < input_count; ++i)
            arg_values.push_back(inputs[i]);
        for (auto i = 0u; i < output_count; ++i)
            arg_values.push_back(outputs_ptr[i]);

        //  Create call instruction
        //auto call_inst =
        builder.CreateCall(function, arg_values);
        // llvm::InlineFunctionInfo info;

        // if (!llvm::InlineFunction(call_inst, info))
        //     LOG_WARNING("[external_plugin_node] Unable to inline function");


        //  Get and return Output values
        std::vector<llvm::Value*> output_values{output_count};
        for (auto i = 0u; i < output_count; ++i)
            output_values[i] = builder.CreateLoad(outputs_ptr[i]);

        return output_values;
    }

    //

    external_plugin::external_plugin(
        llvm::LLVMContext &llvm_context,
        const std::vector<std::string>& code_object_paths,
        const std::size_t mutable_state_size)
    :   _mutable_state_size{mutable_state_size}
    {
        const auto symbol_prefix = "plugin__" + ptr_2_string(this) + "__";
        bool process_func_found = false;

        for (const auto& obj_path : code_object_paths) {
            llvm::SMDiagnostic error;

            auto module = llvm::parseIRFile(obj_path, error, llvm_context);
            if (!module) {
                LOG_ERROR("[external_plugin] Cannot load object %s\n", obj_path.c_str());
                throw std::runtime_error("DSPJIT : Failed to load object");
            }

            LOG_INFO("[external_plugin] Loaded module %s\n", obj_path.c_str());
            //  Add prefix for all global names in module
            for(auto& function : *module) {

                if (!function.isDeclaration()) {
                    const auto new_name = symbol_prefix + function.getName();

                    if (function.getName().equals(process_func_symbol)) {
                        if (process_func_found) {
                            LOG_WARNING("[external_plugin] Warning : duplicate symbol %s\n", process_func_symbol);
                            break;
                        }

                        process_func_found = true;
                        _mangled_process_func_symbol = new_name.str();

                        //  Check that function match an interface
                        const auto process_func_type = function.getFunctionType();

                        if (!_read_process_func_type(process_func_type)) {
                            LOG_ERROR("[external_plugin] process function arguments does not match the required interface\n");
                            throw std::runtime_error("");
                        }

                        LOG_DEBUG("[external_plugin] Found process symbol : %u input(s), %u output(s)\n", _input_count, _output_count);
                    }

                    function.setName(new_name);
                }
            }

            _modules.emplace_back(std::move(module));
        }

        if (!process_func_found) {
            LOG_ERROR("[external_plugin] Symbol '%s' not found in plugin\n", process_func_symbol);
            throw std::runtime_error("DSPJIT : external_plugin : symbol not found");
        }
    }

    void external_plugin::prepare_context(graph_execution_context& context)
    {
        //  Add every needed module to the executinon context
        for (const auto& module : _modules)
            context.add_module(llvm::CloneModule(*module));
    }

    std::unique_ptr<compile_node_class> external_plugin::create_node() const
    {
        return std::make_unique<external_plugin_node>(
            _input_count,
            _output_count,
            _mangled_process_func_symbol,
            _mutable_state_size);
    }

    bool external_plugin::_read_process_func_type(const llvm::FunctionType *func_type)
    {
        const auto param_count = func_type->getFunctionNumParams();

        if (param_count <= 1)
            return false;

        //  Todo check first arg type

        /*  */

        //  void *, float ..., float *...
        _input_count = 0u;
        _output_count = 0u;
        auto i = 1;

        auto is_float_ptr = [] (const llvm::Type *t) {
            const auto ptr_type = llvm::dyn_cast<llvm::PointerType>(t);
            return ptr_type && ptr_type->getElementType()->isFloatTy();
        };

        //  Read inputs types : float...
        for (; i < param_count; ++i) {
            const auto param_type = func_type->getFunctionParamType(i);

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
        for (; i < param_count; ++i) {
            const auto param_type = func_type->getFunctionParamType(i);

            if(!is_float_ptr(param_type)) {
                return false;
            }

            _output_count++;
        }

        return true;
    }

}