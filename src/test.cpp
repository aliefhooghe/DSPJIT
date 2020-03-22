

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch2/catch.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include "test_implentations.h"

using namespace llvm;
using namespace DSPJIT;


/**
 *
 *      Node
 *
 *
 **/

class test_node : public node<test_node> {
    public:
        test_node(const unsigned int input_count):
            node<test_node>(input_count, 1u)
        {}
};

TEST_CASE("Node initial state", "node_initial_state")
{
    test_node n{2u};

    REQUIRE(n.get_input_count() == 2u);
    REQUIRE(n.get_input(0u) == nullptr);
    REQUIRE(n.get_input(1u) == nullptr);
}

TEST_CASE("Node Conection", "node_connection")
{
    test_node n1{0u}, n3{2u};

    {
        test_node n2{0u};
        n1.connect(n3, 0u);
        n2.connect(n3, 1u);

        REQUIRE(n3.get_input(0u) == &n1);
        REQUIRE(n3.get_input(1u) == &n2);
    }

    REQUIRE(n3.get_input(0u) == &n1);
    REQUIRE(n3.get_input(1u) == nullptr);
}


/**
 *
 *      Compile Node Class
 *      Graph Execution Context
 *
 **/

TEST_CASE("input to output", "input_output_one_instance")
{
    LLVMContext llvm_context;
    graph_execution_context context{llvm_context};

    compile_node_class input{0u, 1u};
    compile_node_class output{1u, 0u};

    input.connect(output, 0u);

    context.compile({input}, {output});

    const float in = 42.0f;
    float out = 0.0f;

    context.process(&in, &out);

    REQUIRE(out == in);
}

TEST_CASE("output alone", "input_alone")
{
    LLVMContext llvm_context;
    graph_execution_context context{llvm_context};

    compile_node_class output{1u, 0u};
    context.compile({}, {output});

    float out = 42.0f;
    context.process(nullptr, &out);

    REQUIRE(out == Approx(0.0f));
}

TEST_CASE("Add graph 1", "add_graph 1")
{
    LLVMContext llvm_context;
    graph_execution_context context{llvm_context};

    compile_node_class in1{0u, 1u}, in2{0u, 1u};
    compile_node_class out{1u, 0u};
    add_compile_node add;

    in1.connect(add, 0u);
    in2.connect(add, 1u);
    add.connect(out, 0u);

    context.compile({in1, in2}, {out});

    const float input[2] = {1.f, 10.f};
    float output = 0.f;

    context.process(input, &output);

    REQUIRE(output == Approx(input[0] + input[1]));
}

TEST_CASE("cycle state : integrator")
{
    LLVMContext llvm_context;
    graph_execution_context context{llvm_context};

    compile_node_class in{0u, 1u}, out{1u, 0u};
    add_compile_node add;

    in.connect(add, 0u);
    add.connect(add, 1u); // cycle : state
    add.connect(out, 0u);

    context.compile({in}, {out});

    const float input = 1.0f;
    float output = 0.0f;

    context.process(&input, &output);
    REQUIRE(output == Approx(1.0f));

    context.process(&input, &output);
    REQUIRE(output == Approx(2.0f));

    //  Recompilation
    context.compile({in}, {out});

    context.process(&input, &output);
    REQUIRE(output == Approx(3.0f));

    context.process(&input, &output);
    REQUIRE(output == Approx(4.0f));

    //  Recompilation again
    context.compile({in}, {out});

    context.process(&input, &output);
    REQUIRE(output == Approx(5.0f));
}

/**
 *
 *      ProcessNode
 *
 **/

TEST_CASE("DYN cycle state : integrator")
{
    process_node<float> in{0u}, out{1u};
    add_process_node<float> add;

    in.connect(add, 0u);
    add.connect(add, 1u); // cycle : state
    add.connect(out, 0u);

    const float input = 1.0f;
    float output = 0.0f;

    graph_process({in}, {out}, &input, &output);
    REQUIRE(output == Approx(1.0f));

    graph_process({in}, {out}, &input, &output);
    REQUIRE(output == Approx(2.0f));

    graph_process({in}, {out}, &input, &output);
    REQUIRE(output == Approx(3.0f));

    graph_process({in}, {out}, &input, &output);
    REQUIRE(output == Approx(4.0f));
}