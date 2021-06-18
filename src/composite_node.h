#ifndef DSPJIT_COMPOSITE_NODE_H_
#define DSPJIT_COMPOSITE_NODE_H_

#include "compile_node_class.h"

namespace DSPJIT {

    class composite_node : public compile_node_class {

    public:
        composite_node(
            const unsigned int input_count,
            const unsigned output_count);

        std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value* /* stateless */, llvm::Value*) const override;

        auto& input() noexcept { return _input; }
        auto& output() noexcept { return _output; }

        void add_input() override;
        void remove_input() override;
        void add_output() override;
        void remove_output() override;

    private:
        //  I/O nodes
        compile_node_class _input;
        compile_node_class _output;
    };
}


#endif /* COMPOSITE_COMPILE_NODE_H_ */