#ifndef DSPJIT_GRAPH_EXECUTION_CONTEXT_H_
#define DSPJIT_GRAPH_EXECUTION_CONTEXT_H_

#include <cstdint>
#include <vector>
#include <map>

#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include "graph_state_manager.h"
#include "graph_compiler.h"
#include "lock_free_queue.h"

namespace DSPJIT {

    /**
     * \brief graph_execution_context
     */
    class graph_execution_context {
        

        /* Native compiled function types */
        using native_process_func = void (*)(std::size_t instance_num, const float *inputs, float *outputs);
        using native_initialize_func = void (*)(std::size_t instance_num);

        /* Default function implementation */
        static constexpr auto default_process_func = [](std::size_t, const float*, float*) {};
        static constexpr auto default_initialize_func = [](std::size_t) {};

        /** ack_msg are sent from process thread to compile thread */
        using ack_msg = compile_sequence_t;

        /** compile_done_msg are sent from compile thread to process thread */
        struct compile_done_msg {
            compile_sequence_t seq;
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
            llvm::LLVMContext& llvm_context,
            std::size_t instance_count = 1u,
            const opt_level level = opt_level::Default,
            const llvm::TargetOptions& options = llvm::TargetOptions{});
        
        ~graph_execution_context();

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
         * \brief Create if needed and set a global constant,
         * available for the compile nodes
         */
        void set_global_constant(const std::string& name, float value);

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

        llvm::LLVMContext& _llvm_context;
        std::unique_ptr<llvm::ExecutionEngine> _execution_engine;   ///< execution engine is used for just in time compilation
        std::unique_ptr<llvm::Module> _library{};                   ///< code available for execution from graph node
        compile_sequence_t _current_sequence;                       ///< current compilation sequence number
        graph_state_manager _state_manager;                         ///< manage the state of the graph across recompilations

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
            llvm::Function* process_func,
            llvm::Function* initialize_func);

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

        /* compile done msg process*/
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