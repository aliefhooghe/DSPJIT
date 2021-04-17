cmake_minimum_required (VERSION 3.9.0)
project (DSPJIT)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Catch2 test framework
find_package(Catch2 REQUIRED)
message (STATUS "Found Catch2 IN_TRY_COMPILE ${Catch2_Dir}")

# Catch2 test framework
find_package (LLVM REQUIRED)
message (STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message (STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


###----------###
### Targets  ###
###----------###

## _______________
## DSPJIT

set(DSPJIT_SRC
    common_nodes.cpp
    common_nodes.h
    compile_node_class.cpp
    compile_node_class.h
    composite_node.cpp
    composite_node.h
    external_plugin.cpp
    external_plugin.h
    graph_compiler.cpp
    graph_compiler.h
    graph_execution_context.cpp
    graph_execution_context.h
    graph_state_manager.cpp
    graph_state_manager.h
    ir_helper.cpp
    ir_helper.h
    ir_optimization.cpp
    ir_optimization.h
    lock_free_queue.h
    log.h
    log.cpp
    node.h
    object_dumper.cpp
    object_dumper.h
)


# DSPJIT library
add_library(DSPJIT ${DSPJIT_SRC})
set_target_properties(DSPJIT PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(DSPJIT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LLVM_INCLUDE_DIRS})
target_link_libraries(DSPJIT PUBLIC LLVMCore LLVMTarget LLVMExecutionEngine LLVMTransformUtils LLVMPasses LLVMMCJIT LLVMX86CodeGen)


# Tests
add_executable(run_test test.cpp)
target_link_libraries(run_test PRIVATE DSPJIT Catch2::Catch2)


# Benchmarks
# add_executable(run_benchmark benchmark.cpp)
# target_link_libraries(run_benchmark PRIVATE DSPJIT benchmark::benchmark benchmark::benchmark_main)
