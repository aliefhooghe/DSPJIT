
#include "jit_compiler.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/APFloat.h>

#include <iostream>

using namespace jit_test;

int main()
{
    using namespace llvm;

    LLVMContext context;
    auto module = std::make_unique<Module>("Setup Test", context);
    IRBuilder builder(context);

    auto func_type = FunctionType::get(Type::getInt32Ty(context), false);
    auto function = Function::Create(func_type, Function::ExternalLinkage, "", module.get());
    auto* block = BasicBlock::Create(context, "", function);
    
    //  Ir generation
    builder.SetInsertPoint(block);
    builder.CreateRet(ConstantInt::get(context, APInt(32, 42, true)));

    //  Compilation to machine code
    auto exec_engine = build_execution_engine(std::move(module));
    int (*native_function)() = (int(*)())exec_engine->getPointerToFunction(function);

    //  Call compiled code
    int ret = native_function();

    if (ret == 42)
        std::cout << "LLVM JIT success !" << std::endl;
    else 
        std::cout << "Failed to run llvm jit" << std::endl;

    return 0;
}