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
#include "object_dumper.h"

namespace ProcessGraph {

    class graph_execution_context;

    //
    class compile_node_class : public node<compile_node_class> {
    public:

        compile_node_class(
                graph_execution_context& context,
                const unsigned int input_count,
                std::size_t mutable_state_size_bytes = 0u);

        virtual ~compile_node_class();

        virtual void emit_initialize_rw_state(
                llvm::IRBuilder<>& builder, llvm::Value *mutable_state) {}

        virtual llvm::Value *compile(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& input,
                llvm::Value *mutable_state) const = 0;

        const std::size_t mutable_state_size;
    private:
        graph_execution_context& _context;
    };

    class graph_execution_context {

        static constexpr auto default_process_func = [](std::size_t) { return 0.0f; };

    public:
        friend class compile_node_class;

        graph_execution_context(std::size_t instance_num = 1u);
        ~graph_execution_context();

        /**
         *   Compile Thread API
         **/
        void compile(compile_node_class& output_node, llvm::JITEventListener *listener = nullptr);
        void compile_and_dump_to_file(compile_node_class& output_node, const std::string& filename);

        /**
         *   Process Thread API
         **/
        float process(std::size_t instance_num = 0u);

    private:
        using raw_func = float (*)(std::size_t);

        struct mutable_node_state {
            explicit mutable_node_state(std::size_t state_size, std::size_t instance_count)
            :   cycle_state(instance_count, 0.f),
                data(state_size * instance_count, 0u),
                size{size}
            {}

            mutable_node_state(mutable_node_state&) = delete;
            mutable_node_state(mutable_node_state&&) = default;

            std::vector<float> cycle_state;
            std::vector<uint8_t> data{};
            std::size_t size;
        };

        class delete_sequence {
        public:
            explicit delete_sequence(std::unique_ptr<llvm::ExecutionEngine>&& engine)
            : _execution_engine{std::move(engine)}
            {}

            delete_sequence() = default;
            delete_sequence(delete_sequence&&) = default;
            delete_sequence(delete_sequence&) = delete;

            void add_deleted_node(mutable_node_state && state) { _node_states.emplace_back(std::move(state)); }

        private:
            std::unique_ptr<llvm::ExecutionEngine> _execution_engine;
            std::vector<mutable_node_state> _node_states;
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
            llvm::Value *instance_num_value,
            std::map<const compile_node_class*, llvm::Value*>& values);

        llvm::Value *compile_node_value(
            llvm::IRBuilder<>& builder,
            const compile_node_class& node,
            llvm::Value *instance_num_value);

        llvm::Value *create_array_ptr(
            llvm::IRBuilder<>& builder,
            llvm::Value *base,
            llvm::Value *index,
            std::size_t block_size);

        /* Nodes callbacks */
        void notify_delete_node(compile_node_class*);
        void notify_graph_change() {}     /*  TODO : Any parameters to know what is the modification ? it could be useful for dependencies computations */

        /* ack msg process*/
        void _process_ack_msg(const ack_msg msg);

        std::map<const compile_node_class*, mutable_node_state> _state;
        std::map<compile_sequence_t, delete_sequence> _delete_sequences;
        compile_sequence_t _sequence;
        std::size_t _instance_count;

        /**
         *   Process Thread
         **/

        /* compile done msg process*/
        void _process_compile_done_msg(const compile_done_msg msg);

        raw_func _process_func{};

        /**
         *   Both Threads : Lock free inter thread comunication queues
         **/
        lock_free_queue<ack_msg> _ack_msg_queue;
        lock_free_queue<compile_done_msg> _compile_done_msg_queue;
    };


}

#endif //JITTEST_COMPILE_NODE_CLASS_H
