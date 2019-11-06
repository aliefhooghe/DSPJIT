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

#include "node.h"
#include "lock_free_queue.h"

namespace ProcessGraph {

    class graph_execution_context;

    class compile_node_class : public node<compile_node_class> {
    public:

        //  State
        class state {
        public:
            virtual void* get_raw_ptr() { return nullptr; }
        };
        compile_node_class(
                graph_execution_context& context,
                const unsigned int input_count);

        virtual ~compile_node_class();

        virtual std::unique_ptr<state> create_initial_state() const 
        { 
            return std::make_unique<state>();
        };

        virtual llvm::Value *compile(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& input,
                void *state_raw_ptr) const = 0;
    private:
        graph_execution_context& _context;
    };

    class graph_execution_context {

        static constexpr auto default_process_func = []() { return 0.0f; };

    public:
        friend class compile_node_class;

        graph_execution_context();
        ~graph_execution_context();

        /**
         *   Compile Thread API
         **/
        void compile(compile_node_class& output_node);
        
        /**
         *   Process Thread API
         **/
        float process();

    private:

        using raw_func = float (*)();
        using node_state = compile_node_class::state;

        class delete_sequence {
        public:
            delete_sequence(std::unique_ptr<llvm::ExecutionEngine>&& engine)
            : _execution_engine{std::move(engine)}
            {}

            delete_sequence() = default;
            delete_sequence(delete_sequence&&) = default;
            delete_sequence(delete_sequence&) = delete;
        
            void add_deleted_node(std::unique_ptr<node_state> && state) { _node_states.emplace_back(std::move(state)); }

        private:
            std::unique_ptr<llvm::ExecutionEngine> _execution_engine;
            std::vector<std::unique_ptr<node_state>> _node_states;
        };

        /*  ack_msg are sent from process thread to compile thread */
        using compile_sequence_t = uint32_t;
        using ack_msg = compile_sequence_t;

        /* compile_done msg are send from compile thread to process thread */
        using compile_done_msg = std::pair<compile_sequence_t, raw_func>;

        /**
         *   Compile Thread
         **/

        llvm::LLVMContext _llvm_context{};

        /* Compileling helpers */
        llvm::Value *compile_node_helper(
            llvm::IRBuilder<>& builder,
            const compile_node_class& node,
            std::map<const compile_node_class*, llvm::Value*>& values);

        llvm::Value *compile_node_value(
            llvm::IRBuilder<>& builder,
            const compile_node_class& node);

        /* Nodes callbacks */
        void notify_delete_node(compile_node_class*);
        void notify_graph_change() {}     /*  TODO : Any parameters to know what is the modification ? it could be useful for dependencies computations */

        /* ack msg process*/
        void _process_ack_msg(const ack_msg msg);

        std::map<const compile_node_class*, std::unique_ptr<compile_node_class::state>> _state;
        std::map<compile_sequence_t, delete_sequence> _delete_sequences;
        compile_sequence_t _sequence;
        
        /**
         *   Process Thread
         **/

        /* compile done msg process*/
        void _process_compile_done_msg(const compile_done_msg msg);

        raw_func _process_func{};

        /**
         *   Both Threads : Lock free inter thread comunication
         **/
        lock_free_queue<ack_msg> _ack_msg_queue;
        lock_free_queue<compile_done_msg> _compile_done_msg_queue;
    };


    //


    // class input_compile_node : public compile_node_class {

    // public:
    //     input_compile_node(const std::size_t size = 0u) :
    //         compile_node_class(0u),
    //         _size{size}
    //     {}

    //     ~input_compile_node() override {}

    //     llvm::Value *compile(llvm::IRBuilder<>& builder, const std::vector<llvm::Value*>&/*input*/) const override
    //     {
    //         auto* table = builder.GetInsertBlock()->getValueSymbolTable();
    //         return table->lookup("input");
    //     }
    // private:
    //     const std::size_t _size;
    // };

}

#endif //JITTEST_COMPILE_NODE_CLASS_H
