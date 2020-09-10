#ifndef JITTEST_COMPILE_NODE_CLASS_H
#define JITTEST_COMPILE_NODE_CLASS_H

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include <vector>
#include <map>
#include <set>
#include <functional>
#include <initializer_list>

#include "node.h"
#include "lock_free_queue.h"
#include "object_dumper.h"
#include "log.h"

namespace DSPJIT {

    class graph_execution_context;
    class composite_compile_node;

    //
    class compile_node_class : public node<compile_node_class> {
        friend class graph_execution_context;
    public:
        compile_node_class(
                const unsigned int input_count,
                const unsigned int output_count,
                std::size_t mutable_state_size_bytes = 0u);
        compile_node_class(const compile_node_class&) = delete;
        compile_node_class(compile_node_class&&) = delete;
        virtual ~compile_node_class() = default;

    protected:
        virtual void initialize_mutable_state(
                llvm::IRBuilder<>& builder,
                llvm::Value *mutable_state) const
        {}

        virtual std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
        { return {}; }

        const std::size_t mutable_state_size;
    };

    class graph_execution_context {

        friend class composite_compile_node;

        using native_process_func = void (*)(std::size_t instance_num, const float *inputs, float *outputs);
        using native_initialize_func = void (*)(std::size_t instance_num);

        static constexpr auto default_process_func = [](std::size_t, const float*, float*) {};
        static constexpr auto default_initialize_func = [](std::size_t) {};

    public:
        friend class compile_node_class;
        using opt_level = llvm::CodeGenOpt::Level;
        using node_ref_list = std::initializer_list<std::reference_wrapper<compile_node_class>>;

        graph_execution_context(
            llvm::LLVMContext& llvm_context,
            std::size_t instance_num = 1u,
            const opt_level level = opt_level::Default,
            const llvm::TargetOptions& options = llvm::TargetOptions{});
        ~graph_execution_context();

        /**
         *   Compile Thread API
         **/
        void register_JITEventListener(llvm::JITEventListener*);
        void add_module(std::unique_ptr<llvm::Module>&&);

        void compile(
            node_ref_list input_nodes,
            node_ref_list output_nodes);

        /**
         *   Process Thread API
         **/
        bool update_program() noexcept;
        void process(std::size_t instance_num, const float * inputs, float *outputs) noexcept;
        void process(const float *inputs, float *outputs) noexcept {   process(0u, inputs, outputs);   }
        void initialize_state(std::size_t instance_num = 0u) noexcept;

    private:
        /**
         *  \class mutable_node_state
         *  \brief store compiled program data across re-compilation, for every instances number
         *  \details Each mutable_node_state instance is associated with a compile_node_class_instance
         *      but their lifetime are not linked, as a compile_node_class instance can be removed during the program execution.
         *      The state is removed only when we are sure that the process thread is not anymore using this state.
         */
        struct mutable_node_state {
            explicit mutable_node_state(
                    std::size_t state_size,
                    std::size_t instance_count,
                    std::size_t output_count)
            :   cycle_state(instance_count * output_count, 0.f),
                data(state_size * instance_count, 0u),
                size{state_size}
            {}

            mutable_node_state(const mutable_node_state&) = delete;
            mutable_node_state(mutable_node_state&&) = default;

            std::vector<float> cycle_state;
            std::vector<uint8_t> data{};
            std::size_t size;
        };

        class delete_sequence {
        public:
            explicit delete_sequence(llvm::ExecutionEngine& e, llvm::Module *m = nullptr) noexcept
            : _engine{&e},  _module{m}
            {}

            ~delete_sequence()
            {
                if (_engine && _module) {
                    LOG_DEBUG("[graph_execution_context][compile thread] ~delete_sequence : delete module and %u node stats\n",
                        static_cast<unsigned int>(_node_states.size()));
                    //  llvm execution transfert module's ownership,so we must delete it
                    _engine->removeModule(_module);
                    delete _module;
                }
            }
            delete_sequence(const delete_sequence&) = delete;

            delete_sequence(delete_sequence&& o) noexcept
            : _engine{o._engine}, _module{o._module}, _node_states{std::move(o._node_states)}
            {
                o._engine = nullptr;
                o._module = nullptr;
            }

            void add_deleted_node(mutable_node_state && state) { _node_states.emplace_back(std::move(state)); }

        private:
            llvm::ExecutionEngine* _engine;
            llvm::Module *_module{nullptr};
            std::vector<mutable_node_state> _node_states;
        };

        /*  ack_msg are sent from process thread to compile thread */
        using compile_sequence_t = uint32_t;
        using ack_msg = compile_sequence_t;

        /* compile_done msg are send from compile thread to process thread */
        struct compile_done_msg {
            compile_sequence_t seq;
            native_process_func process_func;
            native_initialize_func initialize_func;
        };

        /*
         *   Compile Thread
         *********************************************/

        using value_memoize_map = std::map<const compile_node_class*, std::vector<llvm::Value*>>;
        using state_map = std::map<const compile_node_class*, mutable_node_state>;
        using delete_sequence_map = std::map<compile_sequence_t, delete_sequence>;

        llvm::LLVMContext& _llvm_context;
        std::unique_ptr<llvm::ExecutionEngine> _execution_engine;
        std::vector<std::unique_ptr<llvm::Module>> _modules{};

        std::set<const compile_node_class*> _last_compiled_nodes{};    //  node thaat have been used ar last compilation
        state_map _state;
        delete_sequence_map _delete_sequences;
        compile_sequence_t _sequence;
        std::size_t _instance_count;

        // Graph Compiling Helpers
        llvm::Function *_compile_process_function(
            node_ref_list input_nodes,
            node_ref_list output_nodes,
            value_memoize_map& node_values,
            llvm::Module& graph_module);

        llvm::Function *_compile_initialize_function(llvm::Module& graph_module);

        /**
         *  \brief Link every needed dependency modules into the graph module
         *  \param graph_module the graph module, i.e. the module in which the graph
         *      is being compiled
         */
        void _link_dependency_modules(llvm::Module& graph_module);

        /**
         *  \brief Load all nodes from the graph input array and associate them to the inputs nodes
         *  \param builder instruction builder
         *  \param input_nodes input nodes
         *  \param input_array input value array (process function argument)
         *  \param values the value memoize map
         */
        void _load_graph_input_values(
            llvm::IRBuilder<>& builder,
            node_ref_list input_nodes,
            llvm::Argument *input_array,
            value_memoize_map& values);

        /**
         *  \brief compute all output nodes dependencies and store the result to the graph output array
         *  \param builder instruction builder
         *  \param output_nodes output nodes
         *  \param output_array output value array (process function argument)
         *  \param instance_num the instance number value (process function parameter)
         *  \param value the value memoize map
         */
        void _compile_and_store_graph_output_values(
            llvm::IRBuilder<>& builder,
            node_ref_list output_nodes,
            llvm::Argument *output_array,
            llvm::Value *instance_num,
            value_memoize_map& values);

        /**
         *  \brief collect all state where not used to emit a value during the last compilation
         */
        void _collect_unused_states();


        llvm::Value* compile_node_value(
            llvm::IRBuilder<>& builder,
            const compile_node_class* node,
            llvm::Value *instance_num_value,
            value_memoize_map& values,
            unsigned int output_id);

        void compile_node(
            llvm::IRBuilder<>& builder,
            const compile_node_class* node,
            llvm::Value *instance_num_value,
            value_memoize_map& values,
            std::vector<llvm::Value*>& output);

        llvm::Value *get_cycle_state_ptr(
            llvm::IRBuilder<>& builder,
            state_map::iterator state_it,
            llvm::Value *instance_num_value,
            unsigned output_id, unsigned int output_count);

        llvm::Value *get_mutable_state_ptr(
            llvm::IRBuilder<>& builder,
            state_map::iterator state_it,
            llvm::Value *instance_num_value);

        //
        state_map::iterator _get_node_mutable_state(const compile_node_class*);

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

        /*
         *   Process Thread
         *********************************************/

        native_process_func _process_func{default_process_func};
        native_initialize_func _initialize_func{default_initialize_func};

        /* compile done msg process*/
        void _process_compile_done_msg(const compile_done_msg msg);

        /*
         *   Both Threads : Lock free inter thread comunication queues
         */
        lock_free_queue<ack_msg> _ack_msg_queue;
        lock_free_queue<compile_done_msg> _compile_done_msg_queue;
    };

}

#endif //JITTEST_COMPILE_NODE_CLASS_H
