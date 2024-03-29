#ifndef DSPJIT_GRAPH_EXECUTION_CONTEXT_H_
#define DSPJIT_GRAPH_EXECUTION_CONTEXT_H_

#include <cstdint>
#include <vector>
#include <map>

#include <DSPJIT/abstract_execution_engine.h>
#include <DSPJIT/abstract_graph_memory_manager.h>
#include <DSPJIT/lock_free_queue.h>

namespace DSPJIT {

    /**
     * \brief graph_execution_context
     */
    class graph_execution_context {

        /* Native compiled function types */
        using native_process_func = void (*)(std::size_t instance_num, const float *inputs, float *outputs);
        using native_initialize_func = void (*)(std::size_t instance_num);

        /* Default functions implementation */
        static constexpr auto default_process_func = [](std::size_t, const float*, float*) {};
        static constexpr auto default_initialize_func = [](std::size_t) {};

        /** ack_msg are sent from process thread to compile thread */
        using ack_msg = abstract_graph_memory_manager::compile_sequence_t;

        /** compile_done_msg are sent from compile thread to process thread */
        struct compile_done_msg {
            abstract_graph_memory_manager::compile_sequence_t seq;
            native_process_func process_func;
            native_initialize_func initialize_func;
        };

    public:
        using opt_level = llvm::CodeGenOpt::Level;
        using node_ref_list = std::initializer_list<std::reference_wrapper<compile_node_class>>;

        /**
         * \brief initialize a new graph execution context
         * \param llvm_context llvm context used for JIT compilation
         * \param instance_count the number of graph state instance that must be managed
         * \param level Native code generation optimization level
         * \param options Native code generation options
         */
        graph_execution_context(
            std::unique_ptr<abstract_execution_engine>&&,
            std::unique_ptr<abstract_graph_memory_manager>&&);

        graph_execution_context(const graph_execution_context&) = delete;
        graph_execution_context(graph_execution_context&&) = delete;

        ~graph_execution_context() noexcept = default;

        /*********************************************
         *   Compile Thread API
         *********************************************/

        /**
         * \brief add a code module, whose functions will be available for nodes
         */
        void add_library_module(std::unique_ptr<llvm::Module>&&);

        /**
         * \brief Compile the current graph into executable code
         * \param input_nodes the nodes which represents the graph inputs
         * \param output_nodes the nodes which represents the graph outputs
         */
        void compile(
            node_ref_list input_nodes,
            node_ref_list output_nodes);

        /**
         * \brief Enable/disable printing of IR code
         * \param enable Print IR if true
         */
        void enable_ir_dump(bool enable = true);

        /**
         * \brief Create if needed and set a global constant,
         * available for the compile nodes
         */
        void set_global_constant(const std::string& name, float value);

        /**
         * \brief Register a memory chunk available as static memory for the given node
         * \note This chunk is not automatically deallocated when the node is not anymore in
         * the compiled circuit.
         */
        void register_static_memory_chunk(const compile_node_class& node, std::vector<uint8_t>&& data);

        /**
         * \brief Free the static memory chunk registered for the given node
         */
        void free_static_memory_chunk(const compile_node_class& node);

        /**
         * \brief Return the number of instance this context can run
         */
        std::size_t get_instance_count() const noexcept { return _instance_count; }

        /*********************************************
         *   Process Thread API
         *********************************************/

        /**
         * \brief Update current process program to the latests available compiled program
         */
        bool update_program() noexcept;

        /**
         * \brief Run the current process program using the graph state indexed by instance_num
         * \param instance_num state instance to be used
         * \param inputs input values
         * \param outputs output values
         */
        void process(std::size_t instance_num, const float * inputs, float *outputs) noexcept;

        /**
         * \brief Run the current process program using the default graph state
         * \param inputs input values
         * \param outputs output values
         */
        void process(const float *inputs, float *outputs) noexcept {   process(0u, inputs, outputs);   }

        /**
         * \brief Initialize the graph state indexed by instance_num
         * \param instance_num state instance to be used
         */
        void initialize_state(std::size_t instance_num = 0u) noexcept;

    private:

        /*********************************************
         *   Used by Compile Thread
         *********************************************/

        using initialize_functions = abstract_graph_memory_manager::initialize_functions;

        llvm::LLVMContext& _llvm_context;
        const std::size_t _instance_count;                           ///< Number of state instances ready for execution
        std::unique_ptr<llvm::Module> _library{};                    ///< code available for execution from graph node

        std::unique_ptr<abstract_execution_engine> _execution_engine{};
        std::unique_ptr<abstract_graph_memory_manager> _state_manager{};

        abstract_graph_memory_manager::compile_sequence_t _current_sequence;  ///< current compilation sequence number

        // debug:
        bool _ir_dump{false};                                       ///< print IR on logs if enabled

        /**
         * \brief Compile the process function
         * \param input_nodes the nodes which represents the graph inputs
         * \param output_nodes the nodes which represents the graph outputs
         * \return the IR process function
         */
        llvm::Function *_compile_process_function(
            node_ref_list input_nodes,
            node_ref_list output_nodes,
            llvm::Module& graph_module);

        /**
         *  \brief Load all nodes from the graph input array and associate them to the inputs nodes
         *  \param builder instruction builder
         *  \param input_nodes input nodes
         *  \param input_array input value array (process function argument)
         */
        void _load_graph_input_values(
            graph_compiler& compiler,
            node_ref_list input_nodes,
            llvm::Argument *input_array);

        /**
         *  \brief compute all output nodes dependencies and store the result to the graph output array
         *  \param context compilation context
         *  \param output_nodes output nodes
         *  \param output_array output value array (process function argument)
         *  \param instance_num the instance number value (process function parameter)
         *  \param value the value memoize map
         */
        void _compile_and_store_graph_output_values(
            graph_compiler& compiler,
            node_ref_list output_nodes,
            llvm::Argument *output_array,
            llvm::Value *instance_num);

        /**
         * \brief the last compilation step : native code jit generation
         * \param graph_module the new module in which the graph functions have been compiled
         * \param process_func the compiled IR process function
         * \param initialize_func the compiled IR initialize function
         */
        void _emit_native_code(
            std::unique_ptr<llvm::Module>&& graph_module,
            llvm::Function* process_funcs,
            initialize_functions initialize_func);

        /**
         *  \brief Process an acknowledgment message
         *  \param msg the message
         *  \details the msg indicate to the compile thread that the process thread is up to date with a given sequence number.
         *      The compile thread can then decide to remove data that are not anymore used
         */
        void _process_ack_msg(const ack_msg msg);

        /*********************************************
         *   Used by Process Thread
         *********************************************/

        /**
         *  \brief Process a compile-done message
         *  \param msg the message
         *  \details the msg indicate to the process thread that a new process function is ready to be used
         */
        void _process_compile_done_msg(const compile_done_msg msg);

        native_process_func _process_func{default_process_func};
        native_initialize_func _initialize_func{default_initialize_func};


        /*********************************************
         *   Shared by both thread
         *********************************************/

        lock_free_queue<ack_msg> _ack_msg_queue;
        lock_free_queue<compile_done_msg> _compile_done_msg_queue;
    };
}

#endif /* DSPJIT_GRAPH_EXECUTION_CONTEXT_H_ */