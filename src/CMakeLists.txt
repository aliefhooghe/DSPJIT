cmake_minimum_required (VERSION 3.7.0)
project (JitTest)

set (CMAKE_CXX_STANDARD_REQUIRED ON)

# LLVM
# REGARDER https://github.com/llvm-mirror/llvm/tree/master/examples

find_package (LLVM REQUIRED CONFIG)
message (STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message (STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

## Targets

# LLVM Setup Test
add_executable(setup_test setup_test.cpp jit_compiler.h jit_compiler.cpp)
target_link_libraries(setup_test LLVM)
set_target_properties(setup_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)
