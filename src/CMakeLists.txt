cmake_minimum_required (VERSION 3.7.0)
project (DSPJIT)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set (CMAKE_CXX_STANDARD_REQUIRED ON)

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
## DSPJIT

set(DSPJIT_SRC
    compile_node_class.cpp
    compile_node_class.h
    composite_compile_node.cpp
    composite_compile_node.h
    external_plugin.h
    external_plugin.cpp
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

add_library(DSPJIT ${DSPJIT_SRC})
set_target_properties(DSPJIT PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(DSPJIT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DSPJIT PUBLIC LLVM)

# Tests
add_executable(run_test test.cpp)
target_link_libraries(run_test DSPJIT)

# Benchmarks
add_executable(run_benchmark benchmark.cpp)
target_link_libraries(run_benchmark DSPJIT benchmark)
