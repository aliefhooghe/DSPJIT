
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include "test_implentations.h"
#include "ir_helper.h"

namespace ProcessGraph {

    // Compile :

    // Constant
    llvm::Value *constant_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value*>&, llvm::Value*) const
    {
        using namespace llvm;
        return ConstantFP::get(builder.getContext(), APFloat(_value));
    }

    // Reference
    llvm::Value *reference_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value*>&, llvm::Value*) const
    {
        return ir_helper::runtime::create_load(builder, &_ref);
    }

    // Add
    llvm::Value *add_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value *> &input, llvm::Value*) const
    {
        using namespace llvm;
        return builder.CreateFAdd(input[0], input[1]);
    }

    // Mul
    llvm::Value *mul_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value *> &input, llvm::Value*) const
    {
        using namespace llvm;
        return builder.CreateFMul(input[0], input[1]);
    }

    // Last
    llvm::Value *last_compile_node::compile(
        llvm::IRBuilder<> &builder, const std::vector<llvm::Value *> &input, llvm::Value* state) const
    {
        using namespace llvm;
        using namespace ir_helper::runtime;

        //  Get pointer to state (= float instance)
        Value *ir_state_ptr = raw2typed_ptr<float>(builder, state);

        //  output <- State :   Load State
        Value *output = builder.CreateLoad(ir_state_ptr);

        //  state <- input  :   Store State
        builder.CreateStore(input[0], ir_state_ptr);

        return output;
    }
}