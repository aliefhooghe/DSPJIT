#ifndef EXTERNAL_PLUGIN_H_
#define EXTERNAL_PLUGIN_H_

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

#include "compile_node_class.h"

namespace DSPJIT {

    /**
     *  \class
     *  \brief
     *
     *  required  :  node_process(void *state, float ...inputs, float* ...outputs)
     *  optional  :  node_initialize(void *state)
     *
     *
     **/
    class external_plugin {

        static constexpr auto process_func_symbol = "node_process";
        static constexpr auto initialize_func_symbol = "node_initialize";

    public:
        explicit external_plugin(
            llvm::LLVMContext &llvm_context,
            const std::vector<std::filesystem::path>& code_object_paths);

        external_plugin(const external_plugin&) = delete;
        external_plugin(external_plugin &&) = default;

        std::unique_ptr<llvm::Module> module();

        /**
         *
         **/
        std::unique_ptr<compile_node_class> create_node() const;

        /*
         *
         */
        const auto get_input_count() const noexcept { return _input_count; }
        const auto get_output_count() const noexcept { return _output_count; }

    private:
        bool _read_process_func_type(const llvm::Module& module, const llvm::FunctionType*);
        bool _check_initialize_func_type(const llvm::Module& module, const llvm::FunctionType*);

        llvm::Type *_read_state_type(const llvm::Type *param_type);

        std::size_t _mutable_state_size{0u};
        unsigned int _input_count;
        unsigned int _output_count;
        std::string _mangled_process_func_symbol;
        std::optional<std::string> _mangled_initialize_func_symbol{};
        std::unique_ptr<llvm::Module> _module{};
    };

}

#endif