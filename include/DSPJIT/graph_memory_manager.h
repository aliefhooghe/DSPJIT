#ifndef DSPJIT_GRAPH_STATE_MANAGER_H_
#define DSPJIT_GRAPH_STATE_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "abstract_graph_memory_manager.h"
#include "abstract_execution_engine.h"

#include "node_state.h"

namespace DSPJIT {

    class compile_node_class;

    /**
     * \class
     * \brief manage the state of a graph program accross recompilations
     */
    class graph_memory_manager : public abstract_graph_memory_manager {

        friend class node_state;

    public:

        /**
         * \brief
         * \param llvm_context LLVM context used for ir code generation
         * \param instance_count The number of graph state instances to be managed
         * \param initial_sequence_number The initial compilation sequence number
         */
        graph_memory_manager(
            llvm::LLVMContext& llvm_context,
            std::size_t instance_count,
            compile_sequence_t initial_sequence_number);

        void begin_sequence(const compile_sequence_t seq) override;
        initialize_functions finish_sequence(abstract_execution_engine& engine, llvm::Module& module) override;

        void using_sequence(const compile_sequence_t seq) override;

        node_state& get_or_create(const compile_node_class& node) override;

        void register_static_memory_chunk(const compile_node_class& node, std::vector<uint8_t>&& chunk) override;
        void free_static_memory_chunk(const compile_node_class& node) override;
        llvm::Value *get_static_memory_ref(llvm::IRBuilder<>& builder, const compile_node_class& node) override;

        llvm::LLVMContext& get_llvm_context() const noexcept override;
        std::size_t get_instance_count() const noexcept override;
    private:

        /**
         * \class delete_sequence
         * \brief
         */
        class delete_sequence {
        public:
            explicit delete_sequence(
                abstract_execution_engine* engine = nullptr,
                llvm::Module *m = nullptr) noexcept;

            delete_sequence(const delete_sequence&) = delete;
            delete_sequence(delete_sequence&& o) noexcept;
            ~delete_sequence();

            void add_deleted_node(node_state && state);
            void add_deleted_static_data(std::vector<uint8_t>&& data);

        private:
            abstract_execution_engine* _engine;
            llvm::Module *_module{nullptr};
            std::vector<node_state> _node_states;               //< Nodes states to be removed when the sequence is over
            std::vector<std::vector<uint8_t>> _static_data_chunks{};    //< Static memory chunk to be removed when the sequence is over
        };

        using node_list = std::vector<const compile_node_class*>;
        using node_set = std::set<const compile_node_class*>;
        using cycle_state_set = std::set<std::pair<node_state*, unsigned int>>;
        using state_map = std::map<const compile_node_class*, node_state>;
        using static_memory_map = std::map<const compile_node_class*, std::vector<uint8_t>>;
        using delete_sequence_map = std::map<compile_sequence_t, delete_sequence>;

        void _trash_static_memory_chunk(static_memory_map::iterator chunk_it);

        llvm::Function* _compile_initialize_function(
            const std::string& symbol,
            const node_list& nodes,
            cycle_state_set* cycles_states,   // can be null
            llvm::Module& module);

        void _declare_used_cycle_state(node_state* state, unsigned int output_id);

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
