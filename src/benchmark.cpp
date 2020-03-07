//
// Benchmark : Compare jit vs dynamic graph execution
//

#include <benchmark/benchmark.h>

#include "test_implentations.h"

using namespace DSPJIT;
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
    reference_compile_node node{x};
    compile_node_class out{1u};

    context.compile({}, {out});

    for (auto _ : state) {
        float f;
        context.process(nullptr, &f);
    }
}
BENCHMARK(deref_pointer_jit);

static void deref_pointer_dyn(benchmark::State& state)
{
    float x = 42;
    reference_process_node<float> node{x};
    process_node<float> out{1u};

    node.connect(out, 0u);

    for (auto _ : state) {
        float f;
        graph_process<float>({}, {node}, nullptr, &f);
    }
}
BENCHMARK(deref_pointer_dyn);

/*
 *
 *      Add to value
 *
 */

static void add1_jit(benchmark::State& state)
{
    graph_execution_context context;

    compile_node_class in1{0u}, in2{0u};
    add_compile_node add{};
    compile_node_class out{1u};

    in1.connect(add, 0);
    in2.connect(add, 1);
    add.connect(out, 0u);

    context.compile({in1, in2}, {out});

    float input[2] = {41.0f, 1.0f};
    float output;
    for (auto _ : state) {
        context.process(input, &output);
    }
}
BENCHMARK(add1_jit);

static void add1_dyn(benchmark::State& state)
{
    process_node<float> in1{0u}, in2{0u};
    add_process_node<float> add;
    process_node<float> out{1u};

    in1.connect(add, 0);
    in2.connect(add, 1);
    add.connect(out, 0u);

    float input[2] = {41.0f, 1.0f};
    float output;
    for (auto _ : state) {
        graph_process({in1, in2}, {out}, input, &output);
    }
}
BENCHMARK(add1_dyn);

/*
 *
 *      Compute x <- in1 * in2 + in3
 *
 */

static void affine_jit(benchmark::State& state)
{
    graph_execution_context context;

    compile_node_class in1{0u}, in2{0u}, in3{0u};
    add_compile_node add{};
    mul_compile_node mul{};
    compile_node_class out{1u};

    in1.connect(mul, 0u);
    in2.connect(mul, 1u);
    mul.connect(add, 0u);
    in3.connect(add, 1u);
    add.connect(out, 0u);

    context.compile({in1, in2, in3}, {out});

    float input[3] = {1.0f, 2.0f, 3.0f};
    float output;

    for (auto _ : state) {
        context.process(input, &output);
    }
}
BENCHMARK(affine_jit);

static void affine_dyn(benchmark::State& state)
{
    process_node<float> in1{0u}, in2{0u}, in3{0u};
    add_process_node<float> add;
    mul_process_node<float> mul;
    process_node<float> out{1u};

    in1.connect(mul, 0u);
    in2.connect(mul, 1u);
    mul.connect(add, 0u);
    in3.connect(add, 1u);
    add.connect(out, 0u);

    float input[3] = {1.0f, 2.0f, 3.0f};
    float output;

    for (auto _ : state) {
        graph_process<float>({in1, in2, in3}, {out}, input, &output);
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

    compile_node_class in{0u};
    add_compile_node add{};
    compile_node_class out{1u};

    in.connect(add, 0u);
    add.connect(add, 1u);
    add.connect(out, 0u);

    context.compile({in}, {out});

    float input = 1.0f;
    float output = 0.0f;

    for (auto _ : state) {
        context.process(&input, &output);
    }
}
BENCHMARK(integrator_jit);

static void integrator_dyn(benchmark::State& state)
{
    process_node<float> in{0u};
    add_process_node<float> add;
    process_node<float> out{1u};

    in.connect(add, 0u);
    add.connect(add, 1u);
    add.connect(out, 0u);

    float input = 1.0f;
    float output = 0.0f;

    for (auto _ : state) {
        graph_process<float>({in}, {out}, &input, &output);
    }
}
BENCHMARK(integrator_dyn);

// MAIN
BENCHMARK_MAIN();