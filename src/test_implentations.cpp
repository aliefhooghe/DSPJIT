
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include "test_implentations.h"
#include "ir_helper.h"

namespace DSPJIT {

    // Compile :

    // Constant
    std::vector<llvm::Value*> constant_compile_node::emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
    {
        using namespace llvm;
        return {ConstantFP::get(builder.getContext(), APFloat(_value))};
    }

    // Reference
    std::vector<llvm::Value*> reference_compile_node::emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
    {
        auto ptr = builder.CreateIntToPtr(
            llvm::ConstantInt::get(builder.getIntNTy(sizeof(float*)*8), reinterpret_cast<intptr_t>(_ref)),
            llvm::Type::getFloatPtrTy(builder.getContext()));

        return {builder.CreateLoad(ptr)};
    }

    // Add
    std::vector<llvm::Value*> add_compile_node::emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
    {
        using namespace llvm;
        return {builder.CreateFAdd(inputs[0], inputs[1])};
    }

    // Mul
    std::vector<llvm::Value*> mul_compile_node::emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
    {
        using namespace llvm;
        return {builder.CreateFMul(inputs[0], inputs[1])};
    }

    // Last
    std::vector<llvm::Value*> last_compile_node::emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const
    {
        using namespace llvm;

        //  Get pointer to state (= float instance)
        Value *state_ptr = builder.CreateBitCast(mutable_state_ptr, llvm::Type::getFloatPtrTy(builder.getContext()));

        //  output <- State :   Load State
        Value *output = builder.CreateLoad(state_ptr);

        //  state <- input  :   Store State
        builder.CreateStore(inputs[0], state_ptr);

        return {output};
    }

    void last_compile_node::initialize_mutable_state(
                llvm::IRBuilder<>& builder,
                llvm::Value *mutable_state) const
    {
        auto zero = llvm::ConstantFP::get(
                builder.getContext(),
                llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));

        auto state_ptr = builder.CreateBitCast(mutable_state, llvm::Type::getFloatPtrTy(builder.getContext()));
        builder.CreateStore(zero, state_ptr);
    }

    std::vector<llvm::Value*> invert_compile_node::emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *) const
    {
        return {builder.CreateFDiv(
            llvm::ConstantFP::get(builder.getFloatTy(), 1.), inputs[0])};
    }
}