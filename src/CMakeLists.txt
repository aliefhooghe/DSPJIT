cmake_minimum_required (VERSION 3.15)
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

# Use static runtime library on MSVC
if (WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

###----------###
### Targets  ###
###----------###

## _______________
## DSPJIT

set(DSPJIT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/common_nodes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common_nodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_node_class.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_node_class.h
    ${CMAKE_CURRENT_SOURCE_DIR}/composite_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/composite_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external_plugin/external_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external_plugin/external_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external_plugin/external_plugin_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external_plugin/external_plugin_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_compiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_execution_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_execution_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_state_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_state_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ir_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ir_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ir_optimization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ir_optimization.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lock_free_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/object_dumper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/object_dumper.h
)


# DSPJIT library
add_library(DSPJIT ${DSPJIT_SRC})
set_target_properties(DSPJIT PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(DSPJIT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LLVM_INCLUDE_DIRS})
target_link_libraries(DSPJIT PUBLIC LLVMCore LLVMTarget LLVMExecutionEngine LLVMTransformUtils LLVMPasses LLVMMCJIT LLVMX86CodeGen)


# Tests
add_executable(run_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_composite_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_execution_context.cpp)
target_link_libraries(run_test PRIVATE DSPJIT Catch2::Catch2)


# Benchmarks
# add_executable(run_benchmark benchmark.cpp)
# target_link_libraries(run_benchmark PRIVATE DSPJIT benchmark::benchmark benchmark::benchmark_main)
