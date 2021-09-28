
#include "log.h"
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
            std::deque<const compile_node_class*> dependency_stack{node};

            while (!dependency_stack.empty()) {
                const auto dependency = dependency_stack.back();

                if (dependency->dependant_process == true) {
                    // Dependant process: Record node as visited and compute inputs

                    if (_nodes_value.find(dependency) == _nodes_value.end()) {
                        // This is the first time we visit this node
                        _assign_null_values(*dependency);
                    }

                    // Scan dependency inputs
                    const auto input_values = _scan_inputs(dependency_stack, *dependency);

                    // Do dependency computation if all dependency are computed
                    if (input_values.has_value()) {
                        _compute_node_output_values(*dependency, input_values.value());
                        dependency_stack.pop_back();
                    }
                }
                else {
                    // Non dependant process:

                    if (_nodes_value.find(dependency) == _nodes_value.end()) {
                        // First visit: Pull outputs
                        _compute_node_output_values(*dependency, {});
                        dependency_stack.pop_back();
                        // Remember to push the input after the curent cycle have been resolver without delay
                        dependency_stack.push_front(dependency);
                    }
                    else {
                        // Output already computed: push inputs

                        // Scan dependency inputs
                        const auto input_values = _scan_inputs(dependency_stack, *dependency);

                        // Do dependency computation if all input are computed
                        if (input_values.has_value()) {
                            // Push inputs
                            _push_node_input_values(*dependency, input_values.value());
                            dependency_stack.pop_back();
                        }
                    }
                }
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
        std::deque<const compile_node_class*>& dependency_stack,
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
                        LOG_DEBUG("[graph_compiler][_scan_input] Resolving a cycle with an additional delay\n");

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
                    break; // do not push several not at once because we dont want a given node to be pushed twice
                }
            }
        }

        if (all_input_computed) {
            return input_values;
        }
        else {
            return std::nullopt;
        }
    }

    void graph_compiler::_push_node_input_values(
        const compile_node_class& node,
        const std::vector<llvm::Value*>& inputs)
    {
        auto& state = _state_mgr.get_or_create(node);
        llvm::Value *state_ptr = nullptr;
        llvm::Value *static_memory_chunk = nullptr;

        if (node.mutable_state_size != 0u)
            state_ptr = state.get_mutable_state_ptr(_builder, _instance_num);

        if (node.use_static_memory) {
            auto memory_chunk = _state_mgr.get_static_memory_ref(_builder, node);

            if (memory_chunk == nullptr) {
                // No memory chunk was registered for this node : it can't be compiled
                return;
            }
            else {
                // Use the registered static memory chunk for compilation
                static_memory_chunk = memory_chunk;
            }
        }

        node.push_input(*this, inputs, state_ptr, static_memory_chunk);
    }

    void graph_compiler::_compute_node_output_values(
        const compile_node_class& node,
        const std::vector<llvm::Value*>& inputs)
    {
        auto& state = _state_mgr.get_or_create(node);

        // get mutable state and static memory ptr if needed
        llvm::Value *state_ptr = nullptr;
        llvm::Value *static_memory_chunk = nullptr;

        if (node.mutable_state_size != 0u)
            state_ptr = state.get_mutable_state_ptr(_builder, _instance_num);

        if (node.use_static_memory) {
            auto memory_chunk = _state_mgr.get_static_memory_ref(_builder, node);

            if (memory_chunk == nullptr) {
                // No memory chunk was registered for this node : it can't be compiled
                // => Set output with dummy zeros
                const auto node_value_it = _nodes_value.find(&node);
                auto& node_output = node_value_it == _nodes_value.end() ?
                    _assign_null_values(node) :
                    node_value_it->second;

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
        if (node.dependant_process == true) {
            const auto node_value_it = _nodes_value.find(&node);

            if (node_value_it == _nodes_value.end())
                throw std::runtime_error("graph_compiler::node_value node values have not been initialized");

            auto &node_output = node_value_it->second;
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
        else {
            // not entry at this point
            auto success =
                _nodes_value.emplace(
                    &node,
                    node.pull_output(*this, state_ptr, static_memory_chunk)).second;

            if (!success)
                throw std::runtime_error("graph_compiler::_get_node_output_values: Could not insert output values of a non dependant process node");
        }
    }

    std::vector<llvm::Value*>& graph_compiler::_assign_null_values(const compile_node_class &node)
    {
        const auto result = _nodes_value.emplace(
            &node,
            std::vector<llvm::Value *>{node.get_output_count(), nullptr});

        if (result.second == false)
            throw std::runtime_error("graph_compiler::_assign_null_values node values have already been initialized");

        return result.first->second;
    }

    llvm::Value *graph_compiler::_create_zero()
    {
        return llvm::ConstantFP::get(
                _builder.getContext(),
                llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));
    }
}
