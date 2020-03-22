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
#include <functional>

#include "node.h"
#include "lock_free_queue.h"
#include "object_dumper.h"
#include "log.h"

namespace DSPJIT {

    class graph_execution_context;

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
        virtual void emit_initialize_rw_state(
                llvm::IRBuilder<>& builder, llvm::Value *mutable_state) {}

        virtual std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
                { return {}; }

        const std::size_t mutable_state_size;
    };

    class graph_execution_context {
        // void _(int64 instance_num, float *inputs, float *outputs)
        using raw_func = void (*)(std::size_t instance_num, const float *inputs, float *outputs);
        static constexpr auto default_process_func = [](std::size_t, const float*, float*) {};

    public:
        friend class compile_node_class;
        using node_ref_vector = std::vector<std::reference_wrapper<compile_node_class>>;
        using opt_level = llvm::CodeGenOpt::Level;

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
            const node_ref_vector& input_nodes,
            const node_ref_vector& output_nodes);

        /**
         *   Process Thread API
         **/
        void process(std::size_t instance_num, const float * inputs, float *outputs);
        void process(const float *inputs, float *outputs)   {   process(0u, inputs, outputs);   }

    private:

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
        using compile_done_msg = std::pair<compile_sequence_t, raw_func>;

        /**
         *   Compile Thread
         **/

        using value_memoize_map = std::map<const compile_node_class*, std::vector<llvm::Value*>>;
        using state_map = std::map<const compile_node_class*, mutable_node_state>;
        using delete_sequence_map = std::map<compile_sequence_t, delete_sequence>;

        llvm::LLVMContext& _llvm_context;
        std::unique_ptr<llvm::ExecutionEngine> _execution_engine;
        std::vector<std::unique_ptr<llvm::Module>> _modules{};

        /* Compiling helpers */
        void _emit_native_code(std::unique_ptr<llvm::Module>&&, llvm::Function*);
        void _link_dependency_modules(llvm::Module& graph_module);
        state_map::iterator _get_node_mutable_state(const compile_node_class*);

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

        llvm::Value *create_array_ptr(
            llvm::IRBuilder<>& builder,
            llvm::Value *raw_ptr_base,
            llvm::Value *index,
            std::size_t block_size);

        /* ack msg process*/
        void _process_ack_msg(const ack_msg msg);

        state_map _state;
        delete_sequence_map _delete_sequences;
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
