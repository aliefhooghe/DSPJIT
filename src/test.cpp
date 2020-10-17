

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch2/catch.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include "common_nodes.h"
#include "composite_node.h"
#include "graph_execution_context.h"

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
    context.update_program();

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
    context.update_program();

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
    add_node add;

    in1.connect(add, 0u);
    in2.connect(add, 1u);
    add.connect(out, 0u);

    context.compile({in1, in2}, {out});
    context.update_program();

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
    const float input = 1.0f;
    float output = 0.0f;

    //  scope to control add lifetime
    {
        add_node add;

        in.connect(add, 0u);
        add.connect(add, 1u); // cycle : state
        add.connect(out, 0u);

        context.compile({in}, {out});
        context.update_program();

        context.process(&input, &output);
        REQUIRE(output == Approx(1.0f));

        context.process(&input, &output);
        REQUIRE(output == Approx(2.0f));

        //  Recompilation
        context.compile({in}, {out});
        context.update_program();

        context.process(&input, &output);
        REQUIRE(output == Approx(3.0f));

        context.process(&input, &output);
        REQUIRE(output == Approx(4.0f));

        //  Recompilation again
        context.compile({in}, {out});
        context.update_program();

        context.process(&input, &output);
        REQUIRE(output == Approx(5.0f));

        //  Disconnect the cycle and recompile
        add.disconnect(1u);
        context.compile({in}, {out});
        context.update_program();

        context.process(&input, &output);
        REQUIRE(output == Approx(input));

    }   //  delete add

    //  The program is independent from the graph (graph = source code)
    context.process(&input, &output);
    REQUIRE(output == Approx(input));

    //  Recompile program
    context.compile({in}, {out});
    context.update_program();

    context.process(&input, &output);
    REQUIRE(output == Approx(0.f));
}

TEST_CASE("node state : z-1")
{
    LLVMContext llvm_context;
    graph_execution_context context{llvm_context};
    float input, output;
    compile_node_class in{0u, 1u}, out{1u, 0u};
    last_node node;

    in.connect(node, 0u);
    node.connect(out, 0u);

    context.compile({in}, {out});
    context.update_program();

    input = 1.f;
    context.process(&input, &output);
    //  State are zeo initialized on creation
    REQUIRE(output == Approx(0.f));

    input = 2.f;
    context.process(&input, &output);
    REQUIRE(output == Approx(1.f));
    context.process(&input, &output);
    REQUIRE(output == Approx(2.f));

    context.initialize_state();
    context.process(&input, &output);
    REQUIRE(output == Approx(0.f));
    context.process(&input, &output);
    REQUIRE(output == Approx(2.f));
}

/**
 *
 *      Composite Compile Node
 *
 **/

TEST_CASE("Composite Compile Node", "composite_node")
{
    LLVMContext llvm_context;
    graph_execution_context context{llvm_context};
    compile_node_class in{0u, 1u}, out{1u, 0u};
    float input, output;

    add_node add{};
    composite_node composite{1, 1};

    composite.input().connect(add, 0);
    composite.input().connect(add, 1);
    add.connect(composite.output(), 0);

    in.connect(composite, 0);
    composite.connect(out, 0);

    context.compile({in}, {out});
    context.update_program();

    input = 1.f;
    context.process(&input, &output);
    REQUIRE(output == Approx(2.f));

    composite.output().disconnect(0);

    context.compile({in}, {out});
    context.update_program();
    context.process(&input, &output);
    REQUIRE(output == Approx(0.f));
}

