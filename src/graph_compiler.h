#ifndef DSPJIT_GRAPH_COMPILER_H_
#define DSPJIT_GRAPH_COMPILER_H_

#include <deque>
#include <optional>

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
         * \param builder a llvm instrcution builder
         * \param instance_num the llvm value containing the instance number
         * \param state_mgr the graph state manager
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
        std::optional<std::vector<llvm::Value*>> _scan_inputs(
            std::deque<const compile_node_class*>& dependency_stack,
            const compile_node_class& node);

        void _push_node_input_values(
            const compile_node_class& node,
            const std::vector<llvm::Value*>& inputs);

        void _compute_node_output_values(
            const compile_node_class& node,
            const std::vector<llvm::Value*>& inputs);

        std::vector<llvm::Value*>& _assign_null_values(const compile_node_class&);

        llvm::Value *_create_zero();

        value_memoize_map _nodes_value{};   ///< Used to record the output values produced by nodes during compilation
        llvm::IRBuilder<>& _builder;        ///< builder used to emit ir code at relevant insert point
        llvm::Value *const _instance_num;   ///< used instance number value
        graph_state_manager& _state_mgr;    ///< calling execution context
    };

}

#endif /* DSPJIT_GRAPH_COMPILER_H_ */