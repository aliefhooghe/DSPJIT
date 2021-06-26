
#include "graph_compiler.h"
#include "compile_node_class.h"

namespace DSPJIT {

        
    graph_compiler::graph_compiler(
        llvm::IRBuilder<>& builder,
        llvm::Value *instance_num,
        graph_state_manager& state_mgr)
    :   _builder{builder},
        _instance_num{instance_num},
        _state_mgr{state_mgr}
    {
    }

    void graph_compiler::assign_values(
        const compile_node_class* node,
        std::vector<llvm::Value*>&& values)
    {
        _nodes_value.emplace(node, std::move(values));
    }

    llvm::Value* graph_compiler::node_value(
        const compile_node_class* node,
        unsigned int output_id)
    {
        if (node == nullptr)
            return create_zero();

        auto value_it = _nodes_value.find(node);

        if (value_it != _nodes_value.end()) {
            //  This node have already been visited
            auto& value_vec = value_it->second;

            //  There is a cycle that was not already solved
            if (value_vec[output_id] == nullptr) {
                auto& state = _state_mgr.get_or_create(*node);
                auto cycle_ptr = 
                    state.get_cycle_state_ptr(_builder, _instance_num, output_id);

                //  Store temporarily the cycle state value as output value.
                //  It will be replaced when this node will be compiled
                value_vec[output_id] = _builder.CreateLoad(cycle_ptr);
            }

            return value_vec[output_id];
        }
        else {
            //  This node have not been visited, create a null filled output vector
            value_it = _nodes_value.emplace_hint(value_it, node, std::vector<llvm::Value*>{node->get_output_count(), nullptr});

            //  Compile node
            compile_node(*node, value_it->second);

            return value_it->second[output_id];
        }
    }

    void graph_compiler::compile_node(
        const compile_node_class& node,
        std::vector<llvm::Value*>& output)
    {
        const auto input_count = node.get_input_count();
        std::vector<llvm::Value*> input_values(input_count);
        auto& state = _state_mgr.get_or_create(node);

        assert(output.size() == node.get_output_count());

        //  Compile dependencies input
        for (auto i = 0u; i < input_count; ++i) {
            unsigned int output_id = 0u;
            const auto *input = node.get_input(i, output_id);
            input_values[i] = node_value(input, output_id);
        }

        //  get mutable state and static memory ptr if needed
        llvm::Value *state_ptr = nullptr;
        llvm::Value *static_memory_chunk = nullptr;

        if (node.mutable_state_size != 0u)
            state_ptr = state.get_mutable_state_ptr(_builder, _instance_num);

        if (node.use_static_memory) {
            auto memory_chunk = _state_mgr.get_static_memory_ref(_builder, node);
        
            if (memory_chunk == nullptr) {
                // No memory chunk was registered for this node : it can't be compiled
                // => Set output with dummy zeros
                for (auto idx = 0u; idx < output.size(); ++idx)
                    output[idx] = create_zero();
                return;
            }
            else {
                // Use the registered static memory chunk for compilation
                static_memory_chunk = memory_chunk;
            }
        }

        // compile processing
        const auto output_values =
            node.emit_outputs(*this, input_values, state_ptr, static_memory_chunk);

        for (auto idx = 0u; idx < output.size(); ++idx) {

            //  There was a cycle for this output
            if (output[idx] != nullptr) {
                //  Create store instruction to cycle state
                auto cycle_ptr =
                    state.get_cycle_state_ptr(_builder, _instance_num, idx);

                _builder.CreateStore(output_values[idx], cycle_ptr);
            }

            // record output value
            output[idx] = output_values[idx];
        }
    }

    llvm::Value *graph_compiler::create_zero()
    {
        return llvm::ConstantFP::get(
                _builder.getContext(),
                llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));
    }
}