#ifndef DSPJIT_ABSTRACT_MEMORY_MANAGER_H_
#define DSPJIT_ABSTRACT_MEMORY_MANAGER_H_

#include <DSPJIT/compile_node_class.h>
#include <DSPJIT/abstract_node_state.h>
#include <DSPJIT/abstract_execution_engine.h>

namespace DSPJIT
{
    class abstract_graph_memory_manager
    {
    public:
        using compile_sequence_t = uint32_t;

        virtual ~abstract_graph_memory_manager() noexcept = default;

        /**
         * \brief Functions used to initialize nodes states
         */
        struct initialize_functions
        {
            llvm::Function* initialize{nullptr};
            llvm::Function* initialize_new_nodes{nullptr};
        };

        /**
         * \brief notify the state manager that a new compilation sequence begins
         * \param seq the new sequence number. Must be greater than the previous ones
         * \note a compilation sequence can be canceled by begining another one without having
         * finnished the previous
         */
        virtual void begin_sequence(const compile_sequence_t seq) = 0;

        /**
         * \brief notify the state manager that a new compilation sequence was finished an compile
         * the graph state initialization function
         * \note can only be called when a compilation sequence has been started.
         * \param compiler reference to the native code compiler on which the native code have been emitted
         * \param module a reference to the module which is builded
         * \return The graph state initialize functions (compiled in module)
         */
        virtual initialize_functions finish_sequence(
            abstract_execution_engine& execution_engine,
            llvm::Module& module) = 0;

        /**
         * \brief notify the state manager that the program generated at a given sequence is now being executed.
         * \note the state manager will free all unused nodes states. Can only be called on a finished compilation sequence
         * \param seq the sequence whose program is now being executed
         */
        virtual void using_sequence(const compile_sequence_t seq) = 0;

        /**
         * \brief return a reference to the stored node's state. State is created if it doesn't exist
         * \param node the node whose state is needed
         * \node notify the state manager that a node was used during the current sequence.
         * can only be called when a compilation sequence has been started.
         */

        virtual abstract_node_state& get_or_create(const compile_node_class& node) = 0;

        /**
         * \brief Set data used for static memory
         */
        virtual void register_static_memory_chunk(const compile_node_class& node, std::vector<uint8_t>&& chunk) = 0;

        /**
         * \brief Free the registered static memory chunk for the given node
         * \note This chunk will be freed when it will be safe to
         */
        virtual void free_static_memory_chunk(const compile_node_class& node) = 0;

        /**
         * \brief Return a pointer to the static memory chunk registered for this node
         */
        virtual llvm::Value *get_static_memory_ref(llvm::IRBuilder<>& builder, const compile_node_class& node) = 0;

        virtual llvm::LLVMContext& get_llvm_context() const noexcept = 0;
        virtual std::size_t get_instance_count() const noexcept = 0;
    };
}

#endif /* DSPJIT_ABSTRACT_MEMORY_MANAGER_H_ */