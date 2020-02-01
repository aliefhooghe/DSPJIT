//
// Benchmark : Compare jit vs dynamic graph execution
//

#include <benchmark/benchmark.h>

#include "test_implentations.h"
#include "jit_compiler.h"

using namespace ProcessGraph;
using namespace llvm;

/*
 *
 *      Dereference a single pointer
 *
 */

static void deref_pointer_jit(benchmark::State& state)
{
    float x = 42;
    graph_execution_context context;
    reference_compile_node node{context, x};

    context.compile(node);

    for (auto _ : state) {
        benchmark::DoNotOptimize(context.process());
    }
}
BENCHMARK(deref_pointer_jit);

static void deref_pointer_dyn(benchmark::State& state)
{
    float x = 42;
    reference_process_node<float> node{x};

    for (auto _ : state) {
        benchmark::DoNotOptimize(graph_process(node));
    }
}
BENCHMARK(deref_pointer_dyn);

/*
 *
 *      Add 1 to a value referenced by a pointer
 *
 */

static void add1_jit(benchmark::State& state)
{
    float x = 42;
    graph_execution_context context;

    reference_compile_node node1{context, x};
    constant_compile_node node2{context, 1.0f};
    add_compile_node node3{context};

    node1.connect(node3, 0);
    node2.connect(node3, 1);

    context.compile(node3);

    for (auto _ : state) {
        benchmark::DoNotOptimize(x = context.process());
    }
}
BENCHMARK(add1_jit);

static void add1_dyn(benchmark::State& state)
{
    float x = 42;
    reference_process_node<float> node1{x};
    constant_process_node<float> node2{1.0f};
    add_process_node<float> node3;

    node1.connect(node3, 0);
    node2.connect(node3, 1);

    for (auto _ : state) {
        benchmark::DoNotOptimize(x = graph_process(node3));
    }
}
BENCHMARK(add1_dyn);

/*
 *
 *      Compute x <- 0.1 * x + 5.0
 *
 */

static void affine_jit(benchmark::State& state)
{
    float x = 42;
    graph_execution_context context;

    constant_compile_node node1{context, 0.1f};
    reference_compile_node node2{context, x};
    constant_compile_node node3{context, 5.0f};

    add_compile_node add_node{context};
    mul_compile_node mul_node{context};

    node1.connect(mul_node, 0);

    node1.connect(mul_node, 0);
    node2.connect(mul_node, 1);

    mul_node.connect(add_node, 0);
    node3.connect(add_node, 1);

    context.compile(add_node);

    for (auto _ : state) {
        benchmark::DoNotOptimize(x = context.process());
    }
}
BENCHMARK(affine_jit);

static void affine_dyn(benchmark::State& state)
{
    float x = 42;
    constant_process_node<float> node1{0.1f};
    reference_process_node<float> node2{x};
    constant_process_node<float> node3{5.0f};

    add_process_node<float> add_node;
    mul_process_node<float> mul_node;

    node1.connect(mul_node, 0);

    node1.connect(mul_node, 0);
    node2.connect(mul_node, 1);

    mul_node.connect(add_node, 0);
    node3.connect(add_node, 1);

    for (auto _ : state) {
        benchmark::DoNotOptimize(x = graph_process(add_node));
    }
}
BENCHMARK(affine_dyn);

/*
 *
 *      Integrator
 *
 */

static void integrator_jit(benchmark::State& state)
{
    graph_execution_context context;
    constant_compile_node incr{context, 1.f};
    add_compile_node add{context};

    incr.connect(add, 0);
    add.connect(add, 1);

    context.compile(add);

    for (auto _ : state)
        benchmark::DoNotOptimize(context.process());
}
BENCHMARK(integrator_jit);

static void integrator_dyn(benchmark::State& state)
{
    constant_process_node incr{1.f};
    add_process_node<float> add{};

    incr.connect(add, 0);
    add.connect(add, 1);

    for (auto _ : state)
        benchmark::DoNotOptimize(graph_process(add));
}
BENCHMARK(integrator_dyn);

// MAIN
BENCHMARK_MAIN();