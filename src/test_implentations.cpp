
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include "test_implentations.h"
#include "ir_helper.h"

namespace ProcessGraph {

    // Compile :

    // Constant
    llvm::Value *constant_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value*>&, void*) const
    {
        using namespace llvm;
        return ConstantFP::get(builder.getContext(), APFloat(_value));
    }

    // Reference
    llvm::Value *reference_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value*>&, void*) const
    {
        using namespace llvm;
        auto& context = builder.getContext();
        auto *pointer_as_integer =
                ConstantInt::get(context, APInt(64, reinterpret_cast<uint64_t>(&_ref)));

        auto *pointer = builder.CreateIntToPtr(pointer_as_integer, PointerType::getUnqual(Type::getFloatTy(context)));

        return builder.CreateLoad(Type::getFloatTy(context), pointer);
    }

    // Add
    llvm::Value *add_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value *> &input, void*) const
    {
        using namespace llvm;
        return builder.CreateFAdd(input[0], input[1]);
    }

    // Mul
    llvm::Value *mul_compile_node::compile(
            llvm::IRBuilder<>& builder, const std::vector<llvm::Value *> &input, void*) const
    {
        using namespace llvm;
        return builder.CreateFMul(input[0], input[1]);
    }

    // Last

    llvm::Value *last_compile_node::compile(
        llvm::IRBuilder<> &builder, const std::vector<llvm::Value *> &input, void* state_raw_ptr) const
    {
        using namespace llvm;

        //  Get pointer to state (= float instance)
        float *state = reinterpret_cast<float*>(state_raw_ptr);
        Value *ir_state_ptr = ir_helper::get_pointer(builder, state);
        
        //  output <- State :   Load
        Value *output = builder.CreateLoad(ir_state_ptr);
        
        //  state <- input  :   Store
        builder.CreateStore(input[0], ir_state_ptr);        
        
        return output;
    }
}