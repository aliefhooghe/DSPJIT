
#include "composite_compile_node.h"

namespace DSPJIT {

    composite_compile_node::composite_compile_node(
        graph_execution_context &execution_context,
        const unsigned int input_count,
        const unsigned int output_count)
    :   compile_node_class{input_count, output_count},
        _execution_context{execution_context},
        _input{0u, input_count},
        _output{output_count, 0u}
    {}

    std::vector<llvm::Value*> composite_compile_node::emit_outputs(
        llvm::IRBuilder<>& builder,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *instance_num) const
    {
        graph_execution_context::value_memoize_map internal_node_values{};
     
        //  Map composite node input value to internal input node
        internal_node_values.emplace(&_input, std::vector<llvm::Value*>{inputs});

        //  Compute output value : theses are the input value of the internal output node
        const auto output_count = get_output_count();
        std::vector<llvm::Value*> output_values(output_count);

        for (auto i = 0u; i < output_count; i++) {
            unsigned int output_id;
            const auto dependency_node = _output.get_input(i, output_id);
            
            output_values[i] = 
                _execution_context.compile_node_value(
                    builder, dependency_node, instance_num, internal_node_values, output_id);
        }

        return output_values;
    }

}