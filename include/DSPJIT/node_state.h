#ifndef NAIVE_MUTABLE_NODE_STATE_H_
#define NAIVE_MUTABLE_NODE_STATE_H_

#include <vector>

#include "abstract_node_state.h"

namespace DSPJIT
{
    class graph_memory_manager;

    class node_state : public abstract_node_state
    {
        friend class graph_memory_manager;
    public:
        node_state(
            graph_memory_manager& manager,
            std::size_t state_size,
            std::size_t instance_count,
            std::size_t output_count);

        node_state(const node_state&) = delete;
        node_state(node_state&&) = default;

        llvm::Value *get_cycle_state_ptr(
            llvm::IRBuilder<>& builder,
            llvm::Value *instance_num_value,
            std::size_t output_id) override;

        llvm::Value *get_mutable_state_ptr(
            llvm::IRBuilder<>& builder,
            llvm::Value *instance_num_value) override;

    private:
        void _update_output_count(std::size_t output_count);

        graph_memory_manager& _manager;
        std::vector<float> _cycle_state;
        std::vector<uint8_t> _data{};
        std::size_t _node_output_count;
        std::size_t _instance_count;
        std::size_t _size;
    };
}

#endif /* NAIVE_MUTABLE_NODE_STATE_H_ */