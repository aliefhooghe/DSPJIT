#ifndef IR_hELPER_H_
#define IR_HELPER_H_

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include <type_traits>

namespace ir_helper {

    namespace runtime {

        template <typename T>
        llvm::Type *get_type(llvm::LLVMContext& context)
        {
            if constexpr (std::is_same_v<T, float>)             return llvm::Type::getFloatTy(context);
            else if constexpr (std::is_same_v<T, double>)       return llvm::Type::getDoubleTy(context);
            else if constexpr (std::is_same_v<T, uint32_t>)     return llvm::Type::getInt32Ty(context);
        }

        template <typename T>
        llvm::Value *get_pointer(llvm::IRBuilder<>& builder, T *ptr)
        {
            constexpr auto ptr_size = sizeof(void*);
            constexpr auto ptr_bit_count = ptr_size * 8;

            auto& context = builder.getContext();
            auto ptr_as_integer =
                llvm::ConstantInt::get(context,
                    llvm::APInt(ptr_bit_count, reinterpret_cast<intptr_t>(ptr)));

            return builder.CreateIntToPtr(
                ptr_as_integer,
                llvm::PointerType::getUnqual(get_type<T>(context)));
        }

        template <typename T>
        llvm::Value *create_load(llvm::IRBuilder<>& builder, T *ptr)
        {
            llvm::Value *ptr_value = get_pointer(ptr);
            return builder.CreateLoad(ptr_value);
        }

        template <typename T>
        void create_store(llvm::IRBuilder<>& builder, llvm::Value *val, T *ptr)
        {
            llvm::Value *ptr_value = get_pointer(ptr);
            builder.CreateStore(val, ptr);
        }

    }
}

#endif