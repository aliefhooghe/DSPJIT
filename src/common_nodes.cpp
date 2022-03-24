
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include <DSPJIT/common_nodes.h>
#include <DSPJIT/graph_compiler.h>
#include <DSPJIT/ir_helper.h>

namespace DSPJIT {

    // Compile :

    // Constant
    std::vector<llvm::Value*> constant_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        using namespace llvm;
        return {ConstantFP::get(compiler.builder().getContext(), APFloat(_value))};
    }

    // Reference
    std::vector<llvm::Value*> reference_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        auto& builder = compiler.builder();
        auto ptr = builder.CreateIntToPtr(
            llvm::ConstantInt::get(builder.getIntNTy(sizeof(float*)*8), reinterpret_cast<intptr_t>(_ref)),
            llvm::Type::getFloatPtrTy(builder.getContext()));

        return {builder.CreateLoad(builder.getFloatTy(), ptr)};
    }

    // Reference multiply node
    std::vector<llvm::Value*> reference_multiply_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        auto& builder = compiler.builder();
        auto ptr = builder.CreateIntToPtr(
            llvm::ConstantInt::get(builder.getIntNTy(sizeof(float*)*8), reinterpret_cast<intptr_t>(_ref)),
            llvm::Type::getFloatPtrTy(builder.getContext()));

        return {builder.CreateFMul(builder.CreateLoad(builder.getFloatTy(), ptr), inputs[0])};
    }

    // Add
    std::vector<llvm::Value*> add_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        return {compiler.builder().CreateFAdd(inputs[0], inputs[1])};
    }

    // Sub
    std::vector<llvm::Value*> substract_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        return {compiler.builder().CreateFSub(inputs[0], inputs[1])};
    }

    // Mul
    std::vector<llvm::Value*> mul_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        return {compiler.builder().CreateFMul(inputs[0], inputs[1])};
    }


    void last_node::initialize_mutable_state(
        llvm::IRBuilder<>& builder,
        llvm::Value *mutable_state, llvm::Value*) const
    {
        auto zero = llvm::ConstantFP::get(
            builder.getContext(),
            llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));

        auto state_ptr = builder.CreateBitCast(mutable_state, llvm::Type::getFloatPtrTy(builder.getContext()));
        builder.CreateStore(zero, state_ptr);
    }

    std::vector<llvm::Value*> last_node::pull_output(
        graph_compiler& compiler,
        llvm::Value *mutable_state,
        llvm::Value *static_memory) const
    {
        auto& builder = compiler.builder();
        auto state_ptr = builder.CreateBitCast(mutable_state, llvm::Type::getFloatPtrTy(builder.getContext()));
        return {builder.CreateLoad(builder.getFloatTy(), state_ptr)};
    }

    void last_node::push_input(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value *mutable_state,
        llvm::Value *static_memory) const
    {
        auto& builder = compiler.builder();
        auto state_ptr = builder.CreateBitCast(mutable_state, llvm::Type::getFloatPtrTy(builder.getContext()));
        builder.CreateStore(inputs[0], state_ptr);
    }

    // invert
    std::vector<llvm::Value*> invert_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        auto& builder = compiler.builder();
        return {builder.CreateFDiv(
            llvm::ConstantFP::get(builder.getFloatTy(), 1.), inputs[0])};
    }

    // negate
    std::vector<llvm::Value*> negate_node::emit_outputs(
        graph_compiler& compiler,
        const std::vector<llvm::Value*>& inputs,
        llvm::Value*, llvm::Value*) const
    {
        auto& builder = compiler.builder();
        return {builder.CreateFNeg(inputs[0])};
    }
}