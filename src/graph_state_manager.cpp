
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
                        reinterpret_cast<intptr_t>(_cycle_state.data() + output_id)),
                    llvm::Type::getFloatPtrTy(_llvm_context)),
                builder.CreateMul(
                    instance_num_value,
                    llvm::ConstantInt::get(builder.getInt64Ty(), _node_output_count)));
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
        for (auto state_it = _state.begin(); state_it != _state.end();) {
            //  avoid iterator invalidation
            const auto cur_it = state_it++;

            //  if this node is not used anymore
            if (_sequence_used_nodes.count(cur_it->first) == 0)
            {
                //  Move the state in the previous delete_sequence in order to make it deleted when the current sequence will be used
                previous_delete_sequence_it->second.add_deleted_node(std::move(cur_it->second));

                //  Remove the coresponding entry in state store
                _state.erase(cur_it);
            }
            else
            {
                //  The node is used, its state must be initialized at graph initialization
                const auto& node = (*cur_it->first);

                //  Ignore stateless nodes
                if (node.mutable_state_size == 0u)
                    continue;

                //  emit the node mutable state initialization code
                node.initialize_mutable_state(
                    builder,
                    cur_it->second.get_mutable_state_ptr(builder, instance_num_value));
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

    graph_state_manager::mutable_node_state& graph_state_manager::get_or_create(const compile_node_class *node)
    {
        auto state_it = _state.find(node);

        if (state_it == _state.end()) {
            //  Create the state if needed
            state_it = _state.emplace(
                node,
                mutable_node_state{_llvm_context, node->mutable_state_size, _instance_count, node->get_output_count()}).first;
        }

        return state_it->second;
    }

}



