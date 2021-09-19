
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
        if (node == nullptr) {
            return _create_zero();
        }

        auto node_value_it = _nodes_value.find(node);
        if (node_value_it != _nodes_value.end()) {
            // This node should outputs should have been computed
            auto& output_value = node_value_it->second[output_id];

            if (output_value == nullptr)
                throw std::runtime_error("graph_compiler::node_value node already present in value map with a null value");

            return output_value;
        }
        else {
            std::vector<const compile_node_class*> dependency_stack{node};

            while (!dependency_stack.empty()) {
                const auto dependency = dependency_stack.back();

                if (_nodes_value.find(dependency) == _nodes_value.end()) {
                    // This is the first time we visit this node
                    _nodes_value.emplace(
                        dependency,
                        std::vector<llvm::Value*>{dependency->get_output_count(), nullptr});
                }

                // Scan dependency inputs
                const auto input_values = _scan_inputs(dependency_stack, *dependency);

                // Do dependency computation if all dependency are computed
                if (input_values.has_value()) {
                    _emit_node_output_values(*dependency, input_values.value());
                    dependency_stack.pop_back();
                }
                // else: input value are not computed yet
            }
        }

        // Node value must have been computed at this point
        node_value_it = _nodes_value.find(node);
        if (node_value_it == _nodes_value.end())
            throw std::runtime_error("graph_compiler::node_value node is not present in value map after compilation");
        else
            return node_value_it->second[output_id];
    }

    std::optional<std::vector<llvm::Value*>> graph_compiler::_scan_inputs(
        std::vector<const compile_node_class*>& dependency_stack,
        const compile_node_class& node)
    {
        bool all_input_computed = true;
        const auto input_count = node.get_input_count();
        std::vector<llvm::Value*> input_values(input_count);

        for (auto i = 0u; i < input_count; ++i) {
            unsigned int out_id = 0u;
            const auto input_node = node.get_input(i, out_id);

            if (input_node == nullptr) {
                // Nothing plugged-in
                input_values[i] = _create_zero();
            }
            else {
                // Check if this input have been visited
                auto input_values_it = _nodes_value.find(input_node);
                if (input_values_it != _nodes_value.end()) {
                    const auto input_value = input_values_it->second[out_id];

                    if (input_value == nullptr) {
                        // Found an unresolved cycle
                        auto& state = _state_mgr.get_or_create(*input_node);
                        auto cycle_ptr =
                            state.get_cycle_state_ptr(_builder, _instance_num, out_id);

                        //  Store temporarily the cycle state value as output value.
                        //  It will be replaced when this node will be compiled
                        const auto cycle_value = _builder.CreateLoad(cycle_ptr);
                        input_values_it->second[out_id] = cycle_value;
                        input_values[i] = cycle_value;
                    }
                    else {
                        // Input is already computed
                        input_values[i] = input_value;
                    }
                }
                else {
                    // This input_node was never visited and is not computed
                    dependency_stack.push_back(input_node);
                    all_input_computed = false;
                }
            }

            if (all_input_computed) {
                return input_values;
            }
            else {
                return std::nullopt;
            }
        }
    }

    void graph_compiler::_emit_node_output_values(
        const compile_node_class& node,
        const std::vector<llvm::Value*>& inputs)
    {
        auto& state = _state_mgr.get_or_create(node);
        const auto node_value_it = _nodes_value.find(&node);

        if (node_value_it == _nodes_value.end())
            throw std::runtime_error("graph_compiler::node_value node values have not been initialized");
        auto &node_output = node_value_it->second;

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
                std::vector<llvm::Value*> zeros(node.get_output_count());
                std::generate(node_output.begin(), node_output.end(), [this]() { return _create_zero();} );
                return;
            }
            else {
                // Use the registered static memory chunk for compilation
                static_memory_chunk = memory_chunk;
            }
        }

        // compile processing
        const auto output_values =
            node.emit_outputs(*this, inputs, state_ptr, static_memory_chunk);

        for (auto i = 0u; i < output_values.size(); ++i) {
            // This output was delayed because of a cycle
            if (node_output[i] != nullptr) {
                auto cycle_ptr =
                    state.get_cycle_state_ptr(_builder, _instance_num, i);
                _builder.CreateStore(output_values[i], cycle_ptr);
            }
            node_output[i] = output_values[i];
        }
    }

    llvm::Value *graph_compiler::_create_zero()
    {
        return llvm::ConstantFP::get(
                _builder.getContext(),
                llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));
    }
}
