cmake_minimum_required (VERSION 3.7.0)
project (JitTest)

set (CMAKE_CXX_STANDARD_REQUIRED ON)

# LLVM
# REGARDER https://github.com/llvm-mirror/llvm/tree/master/examples

find_package (LLVM REQUIRED CONFIG)
message (STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message (STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Google benchmark
find_package(benchmark REQUIRED)
message (STATUS "Found Google benchmark in ${benchmark_DIR}")


###----------###
### Targets  ###
###----------###

## _______________
## LLVM Setup Test

add_executable(setup_test setup_test.cpp jit_compiler.h jit_compiler.cpp)
target_link_libraries(setup_test LLVM)
set_target_properties(setup_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)

## _______________
## Graph compilation

set(SRC
    compile_node_class.cpp
    compile_node_class.h
    ir_helper.cpp
    ir_helper.h
    jit_compiler.h
    jit_compiler.cpp
    lock_free_queue.h
    log.h
    node.h
    process_node.h
    test_implentations.cpp
    test_implentations.h)

# Tests

add_executable(run_test test.cpp ${SRC})
target_link_libraries(run_test LLVM)
set_target_properties(run_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)

# Benchmarks

add_executable(run_benchmark benchmark.cpp ${SRC})
target_link_libraries(run_benchmark LLVM benchmark)
set_target_properties(run_benchmark PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)