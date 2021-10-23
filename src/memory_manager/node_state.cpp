
#include <DSPJIT/log.h>
#include <DSPJIT/graph_memory_manager.h>
#include <DSPJIT/node_state.h>

namespace DSPJIT
{
    node_state::node_state(
        graph_memory_manager& manager,
        std::size_t state_size,
        std::size_t instance_count,
        std::size_t output_count)
    :   _manager{manager},
        _cycle_state(instance_count * output_count, 0.f),
        _data(state_size * instance_count, 0u),
        _node_output_count{output_count},
        _instance_count{instance_count},
        _size{state_size}
    {

    }

    llvm::Value *node_state::get_cycle_state_ptr(
        llvm::IRBuilder<>& builder,
        llvm::Value *instance_num_value,
        std::size_t output_id)
    {
        const auto pointer =
            _cycle_state.data() + output_id * _instance_count;
        _manager._declare_used_cycle_state(this, output_id);
        return
            builder.CreateGEP(
                builder.CreateIntToPtr(
                    llvm::ConstantInt::get(
                        builder.getIntNTy(sizeof(float*) * 8),
                        reinterpret_cast<intptr_t>(pointer)),
                    llvm::Type::getFloatPtrTy(_manager.get_llvm_context())),
                instance_num_value);
    }

    llvm::Value *node_state::get_mutable_state_ptr(
        llvm::IRBuilder<>& builder,
        llvm::Value *instance_num_value)
    {
        if (_size == 0u) {
            return nullptr;
        }
        else {
            return
                builder.CreateGEP(
                    builder.CreateIntToPtr(
                        llvm::ConstantInt::get(
                            builder.getIntNTy(sizeof(uint8_t*) * 8),
                            reinterpret_cast<intptr_t>(_data.data())),
                        builder.getInt8PtrTy()),
                    builder.CreateMul(
                        instance_num_value,
                        llvm::ConstantInt::get(builder.getInt64Ty(), _size)));
        }
    }

    void node_state::_update_output_count(std::size_t output_count)
    {
        LOG_DEBUG("Update output count %llu -> %llu\n", _node_output_count, output_count);
        const auto cycle_state_size = output_count * _instance_count;
        _node_output_count = output_count;
        if (_cycle_state.size() <= cycle_state_size)
            _cycle_state.resize(cycle_state_size);
    }
}