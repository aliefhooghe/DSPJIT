#ifndef DSPJIT_GRAPH_STATE_MANAGER_H_
#define DSPJIT_GRAPH_STATE_MANAGER_H_

#include <map>
#include <set>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/IRBuilder.h>

#include "log.h"

namespace DSPJIT {

    /**
     * \brief compile sequence number type
     */
    using compile_sequence_t = uint32_t;

    class compile_node_class;

    /**
     * \class graph_state_manager
     * \brief manage the state of a graph program accross recompilations 
     */
    class graph_state_manager {

    public:
        /**
         *  \class mutable_node_state
         *  \brief store compiled program data across re-compilation, for every instances number
         *  \details Each mutable_node_state instance is associated with a compile_node_class_instance
         *      but their lifetime are not linked, as a compile_node_class instance can be removed during the program execution.
         *      The state is removed only when we are sure that the process thread is not anymore using this state.
         */
        class mutable_node_state {
        public:
            explicit mutable_node_state(
                    llvm::LLVMContext& llvm_context,
                    std::size_t state_size,
                    std::size_t instance_count,
                    std::size_t output_count);
            mutable_node_state(const mutable_node_state&) = delete;
            mutable_node_state(mutable_node_state&&) = default;

            /**
             * \brief Return a pointer to the node cycle resolving state as a llvm::Value 
             */
            llvm::Value *get_cycle_state_ptr(
                llvm::IRBuilder<>& builder,
                llvm::Value *instance_num_value,
                std::size_t output_id);

            /**
             * \brief Return a pointer to the node muteable state
             */
            llvm::Value *get_mutable_state_ptr(
                llvm::IRBuilder<>& builder,
                llvm::Value *instance_num_value);

        private:
            llvm::LLVMContext& _llvm_context;
            std::vector<float> _cycle_state;
            std::vector<uint8_t> _data{};
            std::size_t _node_output_count;
            std::size_t _size;
        };

        /**
         * \brief
         * \param llvm_context LLVM context used for ir code generation
         * \param instance_count The number of graph state instances to be managed
         * \param initial_sequence_number The initial compilation sequence number 
         */
        graph_state_manager(
            llvm::LLVMContext& llvm_context,
            std::size_t instance_count, 
            compile_sequence_t initial_sequence_number);

        /**
         * \brief notify the state manager that a new compilation sequence begins
         * \param seq the new sequence number. Must be greater than the previous ones
         * \note a compilation sequence can be canceled by begining another one without having
         * finnished the previous
         */
        void begin_sequence(const compile_sequence_t seq);
        
        /**
         * \brief notify the state manager that a node was used during the current sequence
         * \note can only be called when a compilation sequence has been started
         * \param node the node whose state is used
         */
        void declare_used_node(const compile_node_class *node);
        
        /**
         * \brief notify the state manager that a new compilation sequence was finished an compile
         * the graph state initialization function
         * \note can only be called when a compilation sequence has been started
         * \param engine reference to the execution engine on which the native code have been emitted
         * \param module a reference to the module which is builded
         * \return a pointer to the graph state initialize function (compiled in module)
         */
        llvm::Function *finish_sequence(
            llvm::ExecutionEngine& engine, 
            llvm::Module& module);

        /**
         * \brief notify the state manager that the program generated at a given sequence is now being executed.
         * \note the state manager will free all unused nodes states. Can only be called on a finished compilation sequence
         * \param seq the sequence whose program is now being executed
         */
        void using_sequence(const compile_sequence_t seq);

        /**
         * \brief return a reference to the stored node's state. State is created if it doesn't exist  
         * \param node the node whose state is needed
         */
        mutable_node_state& get_or_create(const compile_node_class *node);

    private:

        /**
         * \class delete_sequence
         * \brief 
         */
        class delete_sequence {
        public:
            explicit delete_sequence(llvm::ExecutionEngine* e = nullptr, llvm::Module *m = nullptr) noexcept
            : _engine{e},  _module{m}
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

        using state_map = std::map<const compile_node_class*, mutable_node_state>;
        using delete_sequence_map = std::map<compile_sequence_t, delete_sequence>;

        llvm::LLVMContext& _llvm_context;
        state_map _state{};
        std::set<const compile_node_class*> _sequence_used_nodes{}; 
        delete_sequence_map _delete_sequence{};
        const std::size_t _instance_count;
        compile_sequence_t _current_sequence_number;
    };


}

#endif
