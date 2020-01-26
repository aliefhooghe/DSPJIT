
#include "ir_helper.h"

namespace ir_helper {

    namespace runtime {

        llvm::Value *get_raw_pointer(llvm::IRBuilder<>& builder, const void *ptr)
        {
            constexpr auto ptr_size = sizeof(void*);
            constexpr auto ptr_bit_count = ptr_size * 8;

            auto& context = builder.getContext();

            return llvm::ConstantInt::get(context,
                llvm::APInt(ptr_bit_count, reinterpret_cast<intptr_t>(ptr)));
        }

    }
}