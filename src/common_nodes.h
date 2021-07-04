#ifndef DSPJIT_COMMON_NODES_H_
#define DSPJIT_COMMON_NODES_H_

#include "compile_node_class.h"

#include <math.h>

namespace DSPJIT {

    // Constant node

    class constant_node : public compile_node_class {

    public:
        explicit constant_node(const float value)
        :   compile_node_class{0u, 1u},
            _value(value)
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;
    private:
        const float _value;
    };

    //  Reference node

    class reference_node : public compile_node_class {
    public:
        explicit reference_node(const float* ref)
        :   compile_node_class{0u, 1u},
            _ref{ref}
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;

    private:
        const float* _ref;
    };

    //  Reference multiply node

    class reference_multiply_node : public compile_node_class {
    public:
        explicit reference_multiply_node(const float* ref)
        :   compile_node_class{1u, 1u},
            _ref{ref}
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;

    private:
        const float* _ref;
    };

    // Add

    class add_node : public compile_node_class {
    public:
        add_node() :
            compile_node_class{2u, 1u}
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;
    };

    // Sub
    class substract_node : public compile_node_class {
    public:
        substract_node() :
            compile_node_class{ 2u, 1u }
        {}

        std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value*, llvm::Value*) const override;
    };

    // Mull

    class mul_node : public compile_node_class {
    public:
        mul_node()
        :   compile_node_class{2u, 1u}
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;
    };

    // Z^-1

    class last_node : public compile_node_class {

    public:
        last_node()
        :   compile_node_class{1u, 1u, sizeof(float)}
        {}

        void initialize_mutable_state(
                llvm::IRBuilder<>& builder,
                llvm::Value *mutable_state, llvm::Value*) const override;

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr, llvm::Value*) const override;
    };

    // Invert node

    class invert_node : public compile_node_class {
    public:
        invert_node()
        :   compile_node_class{1u, 1u}
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;
    };

    // Negate node

    class negate_node : public compile_node_class {
    public:
        negate_node()
        :   compile_node_class{1u, 1u}
        {}

        std::vector<llvm::Value*> emit_outputs(
                graph_compiler& compiler,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value*, llvm::Value*) const override;
    };


}

#endif