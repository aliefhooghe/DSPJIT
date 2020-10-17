#ifndef DSPJIT_GRAPH_COMPILER_H_
#define DSPJIT_GRAPH_COMPILER_H_

#include <llvm/IR/IRBuilder.h>
#include <graph_state_manager.h>

namespace DSPJIT {

    /**
     * \brief Helper class for graph compilation
     */
    class graph_compiler 
    {
        using value_memoize_map = std::map<const compile_node_class*, std::vector<llvm::Value*>>;

    public:
        /**
         * \brief create a graph compiler
         * \param builder a llm instrcution builder
         */
        graph_compiler(
            llvm::IRBuilder<>& builder,
            llvm::Value *instance_num,
            graph_state_manager& state_mgr);

        /**
         * \brief assign values to a node
         * \note can be used to implement graph input nodes 
         */
        void assign_values(
            const compile_node_class* node,
            std::vector<llvm::Value*>&& values);

        /**
         * \brief Compile a graph from a node and get node output value. The 
         * visited nodes values are memoized for further calls
         * \param node The node whose output value is compiled
         * \param output_id Output whose value is needed
         * \return 
         */
        llvm::Value* node_value(
            const compile_node_class* node,
            unsigned int output_id);

        /**
         * \return reference to the llvm instruction builder which emit ir code at relevant
         * insert point
         */
        auto& builder() noexcept { return _builder; }

    private:
        void compile_node(
            const compile_node_class* node,
            std::vector<llvm::Value*>& output);

        value_memoize_map _nodes_value{};   ///< Used to memoize the output values produced by nodes during compilation
        llvm::IRBuilder<>& _builder;        ///< builder used to emit ir code at relevant insert point
        llvm::Value *const _instance_num;   ///< used instance number value
        graph_state_manager& _state_mgr;    ///< calling execution context
    };

}

#endif /* DSPJIT_GRAPH_COMPILER_H_ */