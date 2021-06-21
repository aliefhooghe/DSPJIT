
#include "graph_state_manager.h"
#include "compile_node_class.h"

namespace DSPJIT {

    graph_state_manager::mutable_node_state::mutable_node_state(
        llvm::LLVMContext& llvm_context,
        std::size_t state_size,
        std::size_t instance_count,
        std::size_t output_count)
    :   _llvm_context{llvm_context},
        _cycle_state(instance_count * output_count, 0.f),
        _data(state_size * instance_count, 0u),
        _node_output_count{output_count},
        _instance_count{instance_count},
        _size{state_size}
    {

    }

    llvm::Value *graph_state_manager::mutable_node_state::get_cycle_state_ptr(
        llvm::IRBuilder<>& builder,
        llvm::Value *instance_num_value,
        std::size_t output_id)
    {
        return
            builder.CreateGEP(
                builder.CreateIntToPtr(
                    llvm::ConstantInt::get(
                        builder.getIntNTy(sizeof(float*) * 8),
                        reinterpret_cast<intptr_t>(_cycle_state.data() + output_id * _instance_count)),
                    llvm::Type::getFloatPtrTy(_llvm_context)),
                instance_num_value
            );
    }

    llvm::Value *graph_state_manager::mutable_node_state::get_mutable_state_ptr(
        llvm::IRBuilder<>& builder,
        llvm::Value *instance_num_value)
    {
        if (_size == 0u) {
            return nullptr;
        }
        else {
            return
                builder.CreateGEP(
                    builder.CreateIntToPtr(
                        llvm::ConstantInt::get(
                            builder.getIntNTy(sizeof(uint8_t*) * 8),
                            reinterpret_cast<intptr_t>(_data.data())),
                        builder.getInt8PtrTy()),
                    builder.CreateMul(
                        instance_num_value,
                        llvm::ConstantInt::get(builder.getInt64Ty(), _size)));
        }
    }

    void graph_state_manager::mutable_node_state::_update_output_count(std::size_t output_count)
    {
        LOG_DEBUG("Update output count %llu -> %llu\n", _node_output_count, output_count);
        const auto cycle_state_size = output_count * _instance_count;
        _node_output_count = output_count;
        if (_cycle_state.size() <= cycle_state_size)
            _cycle_state.resize(cycle_state_size);
    }

    // Delete sequence implementation

    graph_state_manager::delete_sequence::delete_sequence(
        llvm::ExecutionEngine* engine,
        llvm::Module *module) noexcept
    :   _engine{engine}, _module{module}
    {
    }

    graph_state_manager::delete_sequence::~delete_sequence()
    {
        if (_engine && _module) {
            LOG_DEBUG("[graph_execution_context][compile thread] ~delete_sequence : delete module and %u node stats\n",
                      static_cast<unsigned int>(_node_states.size()));
            //  llvm execution transfert module's ownership,so we must delete it
            if (_engine->removeModule(_module))
                delete _module;
            else
                LOG_ERROR("[graph_execution_context][compile thread] ~delete_sequence : cannot delete the module !\n");
        }
    }

    graph_state_manager::delete_sequence::delete_sequence(delete_sequence &&o) noexcept
        : _engine{o._engine}, _module{o._module}, _node_states{std::move(o._node_states)}
    {
        o._engine = nullptr;
        o._module = nullptr;
    }

    void graph_state_manager::delete_sequence::add_deleted_node(mutable_node_state && state)
    {
        _node_states.emplace_back(std::move(state));
    }

    void graph_state_manager::delete_sequence::add_deleted_static_data(std::vector<uint8_t>&& data)
    {
        _static_data_chunks.emplace_back(std::move(data));
    }

    // Graph state manager implementation

    graph_state_manager::graph_state_manager(
        llvm::LLVMContext& llvm_context,
        std::size_t instance_count,
        compile_sequence_t initial_sequence_number)
    :   _llvm_context{llvm_context},
        _instance_count{instance_count},
        _current_sequence_number{initial_sequence_number}
    {
        _delete_sequence.emplace(initial_sequence_number, delete_sequence{});
    }

    void graph_state_manager::begin_sequence(const compile_sequence_t seq)
    {
        _sequence_used_nodes.clear();
        _current_sequence_number = seq;
    }

    void graph_state_manager::declare_used_node(const compile_node_class *node)
    {
        _sequence_used_nodes.insert(node);
    }

    llvm::Function *graph_state_manager::finish_sequence(llvm::ExecutionEngine& engine, llvm::Module& module)
    {
        //  Create the graph state initialize function
        auto func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(_llvm_context), {llvm::Type::getInt64Ty(_llvm_context)}, false);
        auto function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "graph__initialize_function", &module);
        auto instance_num_value = function->arg_begin();
        auto basic_block = llvm::BasicBlock::Create(_llvm_context, "", function);

        //  Create instruction builder
        llvm::IRBuilder builder(_llvm_context);
        builder.SetInsertPoint(basic_block);

        //  map must not be empty since (a unused sequence is supposed to be started)
        //  get an iterator on previous sequence
        auto previous_delete_sequence_it = _delete_sequence.rbegin();

        //  Collect unused node states :
        for (auto state_it = _mutable_state.begin(); state_it != _mutable_state.end();) {
            //  avoid iterator invalidation
            const auto cur_it = state_it++;

            //  if this node is not used anymore
            if (_sequence_used_nodes.count(cur_it->first) == 0)
            {
                //  Move the state in the previous delete_sequence in order to make it deleted when the current sequence will be used
                previous_delete_sequence_it->second.add_deleted_node(std::move(cur_it->second));

                //  Remove the coresponding entry in state store
                _mutable_state.erase(cur_it);
            }
            else
            {
                //  The node is used, its state must be initialized at graph initialization
                const auto& node = (*cur_it->first);

                //  Ignore stateless nodes
                if (node.mutable_state_size == 0u)
                    continue;

                // Try to retrieve static memory chunk if the node ise static memory
                llvm::Value *static_memory = nullptr;
                if (node.use_static_memory) {
                    const auto static_mem_ref = get_static_memory_ref(builder, node);

                    if (static_mem_ref != nullptr)
                        static_memory = static_mem_ref;
                    else
                        continue;   // Ignore this node if there is not available static memory chink for the node
                }

                //  emit the node mutable state initialization code
                node.initialize_mutable_state(
                    builder,
                    cur_it->second.get_mutable_state_ptr(builder, instance_num_value),
                    static_memory);
            }
        }

        //  Create a delete sequence for the current compilation sequence
        _delete_sequence.emplace(_current_sequence_number, delete_sequence{&engine, &module});

        //  Finish and return the initialize function
        builder.CreateRetVoid();

        return function;
    }

    void graph_state_manager::using_sequence(const compile_sequence_t seq)
    {
        //  Erase all delete sequences older than the used sequence
        _delete_sequence.erase(
            _delete_sequence.begin(),
            _delete_sequence.lower_bound(seq));
    }

    graph_state_manager::mutable_node_state& graph_state_manager::get_or_create(const compile_node_class& node)
    {
        auto state_it = _mutable_state.find(&node);

        if (state_it == _mutable_state.end()) {
            //  Create the state if needed
            state_it = _mutable_state.emplace(
                &node,
                mutable_node_state{_llvm_context, node.mutable_state_size, _instance_count, node.get_output_count()}).first;
        }
        else {
            if (node.get_output_count() != state_it->second._node_output_count) {
                state_it->second._update_output_count(node.get_output_count());
            }
        }

        return state_it->second;
    }

    void graph_state_manager::register_static_memory_chunk(const compile_node_class& node, std::vector<uint8_t>&& data)
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

    void graph_state_manager::free_static_memory_chunk(const compile_node_class& node)
    {
        auto chunk_it = _static_memory.find(&node);

        if (chunk_it != _static_memory.end()){
            _trash_static_memory_chunk(chunk_it);
            _static_memory.erase(chunk_it);
        }
        else { // not an error
            LOG_WARNING("[state_manager] free_static_memory_chunk : no chunk to free for node @%p\n", &node);
        }
    }

    llvm::Value *graph_state_manager::get_static_memory_ref(llvm::IRBuilder<>& builder, const compile_node_class& node)
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

    void graph_state_manager::_trash_static_memory_chunk(static_memory_map::iterator chunk_it)
    {
        auto previous_delete_sequence_it = _delete_sequence.rbegin();

        // Move the chunk into the delete sequence
        previous_delete_sequence_it->second.add_deleted_static_data(std::move(chunk_it->second));
    }
}
