#ifndef DSPJIT_ABSTRACT_NODE_STATE_H_
#define DSPJIT_ABSTRACT_NODE_STATE_H_

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>

namespace DSPJIT
{
    /**
     *  \class mutable_node_state
     *  \brief store compiled program data across re-compilation, for every instances number
     *  \details Each mutable_node_state instance is associated with a compile_node_class_instance
     *      but their lifetime are not linked, as a compile_node_class instance can be removed during the program execution.
     *      The state is removed only when we are sure that the process thread is not anymore using this state.
     */
    class abstract_node_state
    {
    public:
        virtual ~abstract_node_state() noexcept = default;

        /**
         * \brief Return a pointer to the node cycle resolving state as a llvm::Value
         */
        virtual llvm::Value *get_cycle_state_ptr(
            llvm::IRBuilder<> &builder,
            llvm::Value *instance_num_value,
            std::size_t output_id) = 0;

        /**
         * \brief Return a pointer to the node mutable state, null if the node is stateless
         */
        virtual llvm::Value *get_mutable_state_ptr(
            llvm::IRBuilder<> &builder,
            llvm::Value *instance_num_value) = 0;
    };
}

#endif /* DSPJIT_ABSTRACT_NODE_STATE_H_ */
