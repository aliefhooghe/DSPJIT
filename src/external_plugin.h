#ifndef EXTERNAL_PLUGIN_H_
#define EXTERNAL_PLUGIN_H_

#include <string>
#include <vector>

#include "compile_node_class.h"

namespace DSPJIT {

    /**
     *  \class
     *  \brief
     *
     *  node_process(void *state, float ...inputs, float* ...outputs)
     *
     **/
    class external_plugin {
        static constexpr auto process_func_symbol = "node_process";
    public:
        explicit external_plugin(
            llvm::LLVMContext &llvm_context,
            const std::vector<std::string>& code_object_paths,
            const std::size_t mutable_state_size = 0u);

        external_plugin(const external_plugin&) = delete;
        external_plugin(external_plugin &&) = default;

        std::unique_ptr<llvm::Module> module();

        /**
         *
         **/
        std::unique_ptr<compile_node_class> create_node() const;

    private:
        bool _read_process_func_type(const llvm::FunctionType*);

        const std::size_t _mutable_state_size;
        unsigned int _input_count;
        unsigned int _output_count;
        std::string _mangled_process_func_symbol;
        std::unique_ptr<llvm::Module> _module{};
    };

}

#endif