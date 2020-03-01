cmake_minimum_required (VERSION 3.7.0)
project (DSPJIT)

set (CMAKE_CXX_STANDARD_REQUIRED ON)

# LLVM
# REGARDER https://github.com/llvm-mirror/llvm/tree/master/examples

find_package (LLVM REQUIRED CONFIG)
message (STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message (STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Google benchmark
find_package(benchmark REQUIRED)
message (STATUS "Found Google benchmark in ${benchmark_DIR}")

# Catch2 test framework
find_package(Catch2 REQUIRED)


###----------###
### Targets  ###
###----------###

## _______________
## Graph compilation

set(SRC
    compile_node_class.cpp
    compile_node_class.h
    ir_helper.cpp
    ir_helper.h
    ir_optimization.h
    ir_optimization.cpp
    lock_free_queue.h
    log.h
    node.h
    object_dumper.h
    object_dumper.cpp
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