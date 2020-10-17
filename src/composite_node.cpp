
#include "composite_node.h"
#include "graph_compiler.h"

namespace DSPJIT {

    composite_node::composite_node(
        const unsigned int input_count,
        const unsigned int output_count)
    :   compile_node_class{input_count, output_count},
        _input{0u, input_count},
        _output{output_count, 0u}
    {}

    std::vector<llvm::Value*> composite_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *) const
    {
        //  Map composite node input value to internal input node
        compiler.assign_values(&_input, std::vector<llvm::Value*>{inputs});

        //  Compute output value : theses are the input value of the internal output node
        const auto output_count = get_output_count();
        std::vector<llvm::Value*> output_values(output_count);

        for (auto i = 0u; i < output_count; i++) {
            unsigned int output_id;
            const auto dependency_node = _output.get_input(i, output_id);
            
            output_values[i] = compiler.node_value(dependency_node, output_id);
        }

        return output_values;
    }

}