#ifndef JITTEST_COMPILE_NODE_CLASS_H
#define JITTEST_COMPILE_NODE_CLASS_H

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>

#include <vector>
#include <map>
#include <set>

#include "node.h"
#include "graph_state_manager.h"
#include "log.h"

namespace DSPJIT {

    class graph_compiler;

   /**
    * \brief
    */ 
    class compile_node_class : public node<compile_node_class> {

    public:
        /**
         * \brief 
         * \param input_count
         * \param output_count
         * \param mutable_state_size
         */
        compile_node_class(
            const unsigned int input_count,
            const unsigned int output_count,
            std::size_t mutable_state_size = 0u,
            bool use_static_memory = false );
        
        compile_node_class(const compile_node_class&) = delete;
        compile_node_class(compile_node_class&&) = delete;
        virtual ~compile_node_class() = default;

        /**
         * \brief 
         * \param builder
         * \param mutable_state
         * \param static_memory
         */
        virtual void initialize_mutable_state(
            llvm::IRBuilder<>& builder,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const
        {}

        /**
         * \param compiler
         * \param inputs
         * \param mutable_state
         * \param static_memory
         * \return 
         */
        virtual std::vector<llvm::Value*> emit_outputs(
            graph_compiler& compiler,
            const std::vector<llvm::Value*>& inputs,
            llvm::Value *mutable_state,
            llvm::Value *static_memory) const
        { return {}; }

        const std::size_t mutable_state_size;
        const bool use_static_memory;
    };

}

#endif //JITTEST_COMPILE_NODE_CLASS_H
