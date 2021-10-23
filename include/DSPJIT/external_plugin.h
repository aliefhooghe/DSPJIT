#ifndef EXTERNAL_PLUGIN_H_
#define EXTERNAL_PLUGIN_H_

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <DSPJIT/compile_node_class.h>

namespace DSPJIT {

    /**
     *  \class external_plugin
     *  \brief Wrap the defined module function API in a compile_node_class
     *
     *  Required API:
     *
     *  Either Dependant API:
     *      node_process([const chunk_type *static_chunk,] [state_type* mutable_state,] [float ...inputs,] [float* ...outputs])
     *
     *  Or Non dependant API:
     *      node_push([const chunk_type *static_chunk,] [state_type* mutable_state,] [float ...inputs,]);
     *      node_pull([const chunk_type *static_chunk,] [state_type* mutable_state,] [float* ...outputs])
     *
     *  In both cases, if mutable state is used in process:
     *      node_initialize([const chunk_type *static_chunk,] state_type* mutable_state)
     **/
    class external_plugin {
        friend class external_plugin_node;
    public:
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

        /**
         * \brief Return information about the external nodes
         */
        const auto& get_process_info() const noexcept { return _proc_info; }

    private:

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

        struct dependant_process_symbol
        {
            std::string process_symbol;
        };

        struct nondependant_process_symbols
        {
            std::string push_symbol;
            std::string pull_symbol;
        };

        struct external_plugin_symbols
        {
            std::optional<std::string> initialize_symbol;
            std::variant<
                dependant_process_symbol,
                nondependant_process_symbols> compute_symbols;

            bool is_dependant_process() const noexcept
            {
                return std::holds_alternative<dependant_process_symbol>(compute_symbols);
            }
        };

        enum class compute_type : unsigned int
        {
            PROCESS = 0,
            PUSH,
            PULL,

            COUNT
        };

        static constexpr auto compute_type_count = static_cast<unsigned int>(compute_type::COUNT);

        static constexpr const char* _compute_functions_symbols[] =
        {
            "node_process",
            "node_push",
            "node_pull"
        };

        static constexpr auto _initialize_symbol = "node_initialize";

        process_info _read_compute_func(const llvm::Function&, compute_type) const;
        initialization_info _read_initialize_func(const llvm::Function& function) const;

        unsigned int _try_read_state_and_static_chunk_args(
            const llvm::Function& function,
            bool& use_static_mem,
            std::size_t& mutable_state_size) const;

        bool _is_mutable_state(const llvm::Argument *arg, std::size_t& state_size) const;
        bool _is_static_mem(const llvm::Argument *arg) const;
        bool _is_input(const llvm::Argument *arg) const;
        bool _is_output(const llvm::Argument *arg) const;

        bool _check_consistency(
            const process_info& proc_info,
            const std::optional<initialization_info>& init_info) const;

        bool _check_consistency(
            const process_info& push_proc_info,
            const process_info& pull_proc_info) const;

        void _log_compute_function(const char *name, const process_info&);

        process_info _proc_info{};                                      //< Information retrieved from functions signatures
        external_plugin_symbols _symbols{};                             //< magled API symbols found in module
        std::unique_ptr<llvm::Module> _module;                          //< Code module
    };
}

#endif