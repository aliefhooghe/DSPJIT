#ifndef DSPJIT_GRAPH_STATE_MANAGER_H_
#define DSPJIT_GRAPH_STATE_MANAGER_H_

#include <map>
#include <set>
#include <vector>

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
            friend class graph_state_manager;

            explicit mutable_node_state(
                    graph_state_manager& manager,
                    std::size_t state_size,
                    std::size_t instance_count,
                    std::size_t output_count);
        public:
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
             * \brief Return a pointer to the node mutable state
             */
            llvm::Value *get_mutable_state_ptr(
                llvm::IRBuilder<>& builder,
                llvm::Value *instance_num_value);

        private:
            void _update_output_count(std::size_t output_count);

            graph_state_manager& _manager;
            std::vector<float> _cycle_state;
            std::vector<uint8_t> _data{};
            std::size_t _node_output_count;
            std::size_t _instance_count;
            std::size_t _size;
        };

        friend class mutable_node_state;

        /**
         * \brief Functions used to initialize nodes states
         */
        struct initialize_functions
        {
            llvm::Function* initialize{nullptr};
            llvm::Function* initialize_new_nodes{nullptr};
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
         * \brief notify the state manager that a new compilation sequence was finished an compile
         * the graph state initialization function
         * \note can only be called when a compilation sequence has been started
         * \param engine reference to the execution engine on which the native code have been emitted
         * \param module a reference to the module which is builded
         * \return The graph state initialize functions (compiled in module)
         */
        initialize_functions finish_sequence(
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
         * \node notify the state manager that a node was used during the current sequence.
         * can only be called when a compilation sequence has been started.
         */
        mutable_node_state& get_or_create(const compile_node_class& node);

        /**
         * \brief Set data used for static memory
         */
        void register_static_memory_chunk(const compile_node_class& node, std::vector<uint8_t>&& chunk);

        /**
         * \brief Free the registered static memory chunk for the given node
         * \note This chunk will be freed when it will be safe to
         */
        void free_static_memory_chunk(const compile_node_class& node);

        /**
         * \brief Return a pointer to the static memory chunk registered for this node
         */
        llvm::Value *get_static_memory_ref(llvm::IRBuilder<>& builder, const compile_node_class& node);

        /**
         * \brief return the llvm used by the state manager
         */
        llvm::LLVMContext& get_llvm_context() noexcept { return _llvm_context; }

    private:

        /**
         * \class delete_sequence
         * \brief
         */
        class delete_sequence {
        public:
            explicit delete_sequence(llvm::ExecutionEngine* e = nullptr, llvm::Module *m = nullptr) noexcept;
            delete_sequence(const delete_sequence&) = delete;
            delete_sequence(delete_sequence&& o) noexcept;
            ~delete_sequence();

            void add_deleted_node(mutable_node_state && state);
            void add_deleted_static_data(std::vector<uint8_t>&& data);

        private:
            llvm::ExecutionEngine* _engine;
            llvm::Module *_module{nullptr};
            std::vector<mutable_node_state> _node_states;               //< Nodes states to be removed when the sequence is over
            std::vector<std::vector<uint8_t>> _static_data_chunks{};    //< Static memory chunk to be removed when the sequence is over
        };

        using node_list = std::vector<const compile_node_class*>;
        using node_set = std::set<const compile_node_class*>;
        using cycle_state_set = std::set<std::pair<mutable_node_state*, unsigned int>>;
        using state_map = std::map<const compile_node_class*, mutable_node_state>;
        using static_memory_map = std::map<const compile_node_class*, std::vector<uint8_t>>;
        using delete_sequence_map = std::map<compile_sequence_t, delete_sequence>;

        void _trash_static_memory_chunk(static_memory_map::iterator chunk_it);

        llvm::Function* _compile_initialize_function(
            const std::string& symbol,
            const node_list& nodes,
            cycle_state_set* cycles_states,   // can be null
            llvm::Module& module);

        void _declare_used_cycle_state(mutable_node_state* state, unsigned int output_id);

        llvm::LLVMContext& _llvm_context;
        state_map _state{};
        static_memory_map _static_memory{};
        node_list _sequence_new_nodes{};
        node_set _sequence_used_nodes{};
        cycle_state_set _sequence_used_cycle_states{};
        delete_sequence_map _delete_sequence{};
        const std::size_t _instance_count;
        compile_sequence_t _current_sequence_number;
    };


}

#endif
