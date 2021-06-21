#ifndef EXTERNAL_PLUGIN_H_
#define EXTERNAL_PLUGIN_H_

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

#include "compile_node_class.h"

namespace DSPJIT {

    /**
     *  \class external_plugin
     *  \brief Wrap the defined module function API in a compile_node_class
     *
     *  required API :
     *      node_process([const chunk_type *static_chunk,] [state_type* mutable_state,] [float ...inputs,] [float* ...outputs])
     *      node_initialize([const chunk_type *static_chunk,] state_type* mutable_state) (if mutable state is used in process)
     **/
    class external_plugin {

        static constexpr auto process_func_symbol = "node_process";
        static constexpr auto initialize_func_symbol = "node_initialize";

    public:
        struct process_info
        {
            unsigned int input_count{0u};
            unsigned int output_count{0u};
            std::size_t mutable_state_size{0u};
            bool use_static_memory{false};
        };

        struct initialization_info
        {
            std::size_t mutable_state_size{0u};
            bool use_static_memory{false};
        };

        explicit external_plugin(std::unique_ptr<llvm::Module>&& module);

        external_plugin(const external_plugin&) = delete;
        external_plugin(external_plugin &&) = default;

        /**
         * \brief Return a module containing all code needed by the
         * compiles nodes generated from this plugin
         */
        std::unique_ptr<llvm::Module> create_module();

        /**
         * \brief Create a node instance
         **/
        std::unique_ptr<compile_node_class> create_node() const;


        const auto& get_process_info() const noexcept { return _proc_info; }
    private:
        process_info _read_process_func(const llvm::Function&) const;
        initialization_info _read_initialize_func(const llvm::Function& function) const;

        bool _is_mutable_state(const llvm::Argument *arg, std::size_t& state_size) const;
        bool _is_static_mem(const llvm::Argument *arg) const;
        bool _is_input(const llvm::Argument *arg) const;
        bool _is_output(const llvm::Argument *arg) const;
        bool _check_consistency(
            const process_info& proc_info,
            const std::optional<initialization_info>& init_info) const;

        process_info _proc_info{};                                      //< Information retrieved from functions signatures
        std::string _mangled_process_func_symbol;                       //< Process function symbol in the module
        std::optional<std::string> _mangled_initialize_func_symbol{};   //< Initialize function symbol (if any)
        std::unique_ptr<llvm::Module> _module;                          //< Code module
    };
}

#endif