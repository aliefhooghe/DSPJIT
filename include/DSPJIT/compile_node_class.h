#ifndef DSPJIT_COMPILE_NODE_CLASS_H
#define DSPJIT_COMPILE_NODE_CLASS_H

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>

#include <vector>
#include <map>
#include <set>

#include "node.h"

namespace DSPJIT {

    class graph_compiler;

   /**
    * \brief Emit code which consume input values and produce output values
    */
    class compile_node_class : public node<compile_node_class> {

    public:
        /**
         * \param input_count
         * \param output_count
         * \param mutable_state_size
         */
        compile_node_class(
            const unsigned int input_count,
            const unsigned int output_count,
            std::size_t mutable_state_size = 0u,
            bool use_static_memory = false,
            bool dependant_process = true);

        compile_node_class(const compile_node_class&) = delete;
        compile_node_class(compile_node_class&&) = delete;
        virtual ~compile_node_class() = default;

        /**
         * \brief Emit the initialization code for the mutable state
         * \note Implement this if the node use a mutable_state (mutable_state_size > 0)
         * \param builder Instruction builder
         * \param mutable_state Mutable state pointer
         * \param static_memory Static memory chunk if the node use it, else null
         */
        virtual void initialize_mutable_state(
            llvm::IRBuilder<>& builder,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const
        {}

        /**
         * \brief Emit the process code in the case of a dependant process node
         * \note Implement this if this node is a dependant process node
         * \param compiler
         * \param inputs
         * \param mutable_state
         * \param static_memory
         * \return The computed output values
         */
        virtual std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const
        { return {}; }

        /**
         * \brief Emit the process code which produce output value in case of non dependant process node
         */
        virtual std::vector<llvm::Value*> pull_output(
            graph_compiler& compiler,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const
        { return {}; }

        /**
         * \brief Emit the process code which consume input value in case of non dependant process node
         */
        virtual void push_input(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const
        {}

        const std::size_t mutable_state_size;
        const bool use_static_memory;
        const bool dependant_process;
    };

}

#endif //JITTEST_COMPILE_NODE_CLASS_H
