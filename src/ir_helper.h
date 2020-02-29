#ifndef IR_HELPER_H_
#define IR_HELPER_H_

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include <type_traits>

namespace ir_helper {

    void print_module(const llvm::Module& module);

    namespace runtime {

        template <typename T>
        auto get_type(llvm::LLVMContext& context)
        {
            using type = std::decay_t<T>;
            if constexpr (std::is_same_v<type, float>)             return llvm::Type::getFloatTy(context);
            else if constexpr (std::is_same_v<type, double>)       return llvm::Type::getDoubleTy(context);
            else if constexpr (std::is_same_v<type, uint32_t>)     return llvm::Type::getInt32Ty(context);
        }

        llvm::Value *get_raw_pointer(llvm::IRBuilder<>& builder, const void *ptr);

        template <typename T>
        llvm::Value *raw2typed_ptr(llvm::IRBuilder<>& builder, llvm::Value* ptr)
        {
            auto& context = builder.getContext();
            return builder.CreateIntToPtr(
                ptr,
                llvm::PointerType::getUnqual(get_type<T>(context)));
        }


        template <typename T>
        llvm::Value *get_pointer(llvm::IRBuilder<>& builder, T *ptr)
        {
            return raw2typed_ptr<T>(
                    builder, get_raw_pointer(builder, ptr));
        }

        template <typename T>
        llvm::Value *create_load(llvm::IRBuilder<>& builder, T *ptr)
        {
            llvm::Value *ptr_value = get_pointer<T>(builder, ptr);
            return builder.CreateLoad(ptr_value);
        }

        template <typename T>
        void create_store(llvm::IRBuilder<>& builder, llvm::Value *val, T *ptr)
        {
            llvm::Value *ptr_value = get_pointer<T>(builder, ptr);
            builder.CreateStore(val, ptr_value);
        }

    }
}

#endif