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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/abstract_execution_engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/abstract_graph_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/abstract_node_state.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/common_nodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/compile_node_class.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/composite_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/external_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/graph_compiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/graph_execution_context_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/graph_execution_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/graph_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/ir_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/llvm_legacy_execution_engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/lock_free_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/node_state.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DSPJIT/node.h

    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_nodes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compile_node_class.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/composite_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graph_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graph_execution_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graph_execution_context_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ir_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ir_optimization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ir_optimization.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/execution_engine/llvm_legacy_execution_engine.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/external_plugin/external_plugin_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external_plugin/external_plugin_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external_plugin/external_plugin.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory_manager/graph_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory_manager/node_state.cpp
)


# DSPJIT library
add_library(DSPJIT ${DSPJIT_SRC})
set_target_properties(DSPJIT PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(DSPJIT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${LLVM_INCLUDE_DIRS})
target_link_libraries(DSPJIT PUBLIC LLVMCore LLVMTarget LLVMExecutionEngine LLVMTransformUtils LLVMPasses LLVMMCJIT LLVMX86CodeGen)


# Tests
add_executable(run_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_composite_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_execution_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test_node.cpp)
target_link_libraries(run_test PRIVATE DSPJIT Catch2::Catch2)
