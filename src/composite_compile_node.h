#ifndef COMPOSITE_COMPILE_NODE_H_
#define COMPOSITE_COMPILE_NODE_H_

#include "compile_node_class.h"

namespace DSPJIT {

    class composite_compile_node : public compile_node_class {

    public:
        composite_compile_node(
            graph_execution_context &execution_context,
            const unsigned int input_count,
            const unsigned output_count);

        std::vector<llvm::Value*> emit_outputs(
            llvm::IRBuilder<>& builder,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state_ptr) const override;

        auto& input() noexcept { return _input; }
        auto& output() noexcept { return _output; }

    private:
        graph_execution_context &_execution_context;

        //  I/O nodes
        compile_node_class _input;
        compile_node_class _output;
    };
}


#endif /* COMPOSITE_COMPILE_NODE_H_ */