
#include <DSPJIT/log.h>

#include <DSPJIT/graph_memory_manager.h>

namespace DSPJIT {

    // Delete sequence implementation

    graph_memory_manager::delete_sequence::delete_sequence(
        abstract_execution_engine* engine,
        llvm::Module *module) noexcept
    :   _engine{engine}, _module{module}
    {
    }

    graph_memory_manager::delete_sequence::~delete_sequence()
    {
        if (_engine && _module) {
            LOG_DEBUG("[graph_execution_context][compile thread] ~delete_sequence : delete module and %u node stats\n",
                      static_cast<unsigned int>(_node_states.size()));
            _engine->delete_module(_module);
        }
    }

    graph_memory_manager::delete_sequence::delete_sequence(delete_sequence &&o) noexcept
        : _engine{o._engine}, _module{o._module}, _node_states{std::move(o._node_states)}
    {
        o._engine = nullptr;
        o._module = nullptr;
    }

    void graph_memory_manager::delete_sequence::add_deleted_node(node_state && state)
    {
        _node_states.emplace_back(std::move(state));
    }

    void graph_memory_manager::delete_sequence::add_deleted_static_data(std::vector<uint8_t>&& data)
    {
        _static_data_chunks.emplace_back(std::move(data));
    }

    // Graph state manager implementation

    graph_memory_manager::graph_memory_manager(
        llvm::LLVMContext& llvm_context,
        std::size_t instance_count,
        compile_sequence_t initial_sequence_number)
    :   _llvm_context{llvm_context},
        _instance_count{instance_count},
        _current_sequence_number{initial_sequence_number}
    {
        _delete_sequence.emplace(initial_sequence_number, delete_sequence{});
    }

    void graph_memory_manager::begin_sequence(const compile_sequence_t seq)
    {
        _sequence_new_nodes.clear();
        _sequence_used_nodes.clear();
        _sequence_used_cycle_states.clear();
        _current_sequence_number = seq;
    }

    abstract_graph_memory_manager::initialize_functions graph_memory_manager::finish_sequence(
        abstract_execution_engine& engine, llvm::Module& module)
    {
        node_list used_nodes{};

        //  map must not be empty since (a unused sequence is supposed to be started)
        //  get an iterator on previous sequence
        auto previous_delete_sequence_it = _delete_sequence.rbegin();

        for (auto state_it = _state.begin(); state_it != _state.end();) {
            //  avoid iterator invalidation
            const auto cur_it = state_it++;
            //  Collect unused nodes : this node is not used anymore
            if (_sequence_used_nodes.count(cur_it->first) == 0)
            {
                //  Move the state in the previous delete_sequence in order to make it deleted when the current sequence will be used
                previous_delete_sequence_it->second.add_deleted_node(std::move(cur_it->second));

                //  Remove the coresponding entry in state store
                _state.erase(cur_it);
            }
            else
            {
                used_nodes.push_back(cur_it->first);
            }
        }

        //  Create a delete sequence for the current compilation sequence
        _delete_sequence.emplace(_current_sequence_number, delete_sequence{&engine, &module});

        LOG_DEBUG("[graph_state_manager][finish_sequence] Compile init func for %lu nodes (%lu news)\n",
            used_nodes.size(), _sequence_new_nodes.size());

        return {
            _compile_initialize_function("graph__initialize", used_nodes, &_sequence_used_cycle_states, module),
            _compile_initialize_function("graph__initialize_new_nodes", _sequence_new_nodes, nullptr, module)
        };
    }

    llvm::Function* graph_memory_manager::_compile_initialize_function(
        const std::string& symbol,
        const node_list& nodes,
        cycle_state_set* cycles_states,
        llvm::Module& module)
    {
        //  Create the graph state initialize function
        auto func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(_llvm_context), { llvm::Type::getInt64Ty(_llvm_context) }, false);
        auto function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, symbol, &module);
        auto instance_num_value = function->arg_begin();
        auto basic_block = llvm::BasicBlock::Create(_llvm_context, "", function);

        //  Create instruction builder
        llvm::IRBuilder builder(_llvm_context);
        builder.SetInsertPoint(basic_block);

        for (const auto node : nodes) {
            if (node->mutable_state_size != 0u) {
                const auto state_it = _state.find(node);

                if (state_it != _state.end()) {
                    // Try to retrieve static memory chunk if the node ise static memory
                    llvm::Value *static_memory = nullptr;
                    if (node->use_static_memory) {
                        const auto static_mem_ref = get_static_memory_ref(builder, *node);

                        if (static_mem_ref != nullptr)
                            static_memory = static_mem_ref;
                        else
                            continue;   // Ignore this node if there is not available static memory chunk for the node
                    }

                    //  emit the node mutable state initialization code
                    node->initialize_mutable_state(
                        builder,
                        state_it->second.get_mutable_state_ptr(builder, instance_num_value),
                        static_memory);
                }
                else {
                    LOG_ERROR("[graph_state_manager][_compile_initialize_function] Could not find state for node %p\n", node);
                }
            }
        }

        // Initialize cycles states if any
        if (cycles_states != nullptr) {
            const auto zero =
                llvm::ConstantFP::get(
                    _llvm_context,
                    llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));
            LOG_DEBUG("[graph_state_manager][_compile_initialize_function] Initialize %u cycles states\n",
                cycles_states->size());

            for (const auto& cycle_state : *cycles_states) {
                const auto cycle_state_ptr =
                    cycle_state.first->get_cycle_state_ptr(builder, instance_num_value, cycle_state.second);
                builder.CreateStore(zero, cycle_state_ptr);
            }
        }

        //  Finish and return the initialize function
        builder.CreateRetVoid();

        return function;
    }

    void graph_memory_manager::_declare_used_cycle_state(node_state* state, unsigned int output_id)
    {
        _sequence_used_cycle_states.emplace(state, output_id);
    }

    void graph_memory_manager::using_sequence(const compile_sequence_t seq)
    {
        //  Erase all delete sequences older than the used sequence
        _delete_sequence.erase(
            _delete_sequence.begin(),
            _delete_sequence.lower_bound(seq));
    }

    node_state& graph_memory_manager::get_or_create(const compile_node_class& node)
    {
        auto state_it = _state.find(&node);

        if (state_it == _state.end()) {
            //  Create the state if needed
            state_it = _state.emplace(
                &node,
                node_state{*this, node.mutable_state_size, _instance_count, node.get_output_count()}).first;
            //  Remember that this state is a new state
            _sequence_new_nodes.push_back(&node);
        }
        else {
            if (node.get_output_count() != state_it->second._node_output_count) {
                state_it->second._update_output_count(node.get_output_count());
            }
        }

        // Remember that this state is used in the current sequence
        _sequence_used_nodes.insert(&node);
        return state_it->second;
    }

    void graph_memory_manager::register_static_memory_chunk(const compile_node_class& node, std::vector<uint8_t>&& data)
    {
        const auto chunk_it = _static_memory.find(&node);

        if (chunk_it == _static_memory.end()) {
            _static_memory.emplace(&node, std::move(data));
        }
        else {
            _trash_static_memory_chunk(chunk_it);   // trash the old chunk
            chunk_it->second = std::move(data);     // put the new chunk in place
        }
    }

    void graph_memory_manager::free_static_memory_chunk(const compile_node_class& node)
    {
        auto chunk_it = _static_memory.find(&node);

        if (chunk_it != _static_memory.end()){
            _trash_static_memory_chunk(chunk_it);
            _static_memory.erase(chunk_it);
        }
        // else: not an error, the node could have been inserted but not compiled
    }

    llvm::Value *graph_memory_manager::get_static_memory_ref(llvm::IRBuilder<>& builder, const compile_node_class& node)
    {
        const auto it = _static_memory.find(&node);

        if (it == _static_memory.end()) {
            return nullptr;
        }
        else {
            return builder.CreateIntToPtr(
                llvm::ConstantInt::get(
                    builder.getIntNTy(sizeof(float*) * 8),
                    reinterpret_cast<intptr_t>(it->second.data())),
                builder.getInt8PtrTy());
        }
    }

    llvm::LLVMContext& graph_memory_manager::get_llvm_context() const noexcept
    {
        return _llvm_context;
    }

    std::size_t graph_memory_manager::get_instance_count() const noexcept
    {
        return _instance_count;
    }

    void graph_memory_manager::_trash_static_memory_chunk(static_memory_map::iterator chunk_it)
    {
        auto previous_delete_sequence_it = _delete_sequence.rbegin();

        // Move the chunk into the delete sequence
        previous_delete_sequence_it->second.add_deleted_static_data(std::move(chunk_it->second));
    }
}
