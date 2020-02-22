
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include "test_implentations.h"
#include "ir_helper.h"

namespace ProcessGraph {

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
        return {ir_helper::runtime::create_load(builder, &_ref)};
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
        using namespace ir_helper::runtime;

        //  Get pointer to state (= float instance)
        Value *ir_state_ptr = raw2typed_ptr<float>(builder, mutable_state_ptr);

        //  output <- State :   Load State
        Value *output = builder.CreateLoad(ir_state_ptr);

        //  state <- input  :   Store State
        builder.CreateStore(inputs[0], ir_state_ptr);

        return {output};
    }
}