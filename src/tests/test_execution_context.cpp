
#include <catch2/catch.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include <DSPJIT/graph_execution_context_factory.h>
#include <DSPJIT/graph_compiler.h>
#include <DSPJIT/common_nodes.h>

using namespace llvm;
using namespace DSPJIT;

/**
 *
 *      Compile Node Class
 *      Graph Execution Context
 *
 **/

TEST_CASE("input to output", "input_output_one_instance")
{
    LLVMContext llvm_context;
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);

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
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);

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
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);

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
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);

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

    } //  delete add

    //  The program is independent from the graph (graph = source code)
    context.process(&input, &output);
    REQUIRE(output == Approx(input));

    //  Recompile program
    context.compile({in}, {out});
    context.update_program();

    context.process(&input, &output);
    REQUIRE(output == Approx(0.f));
}

TEST_CASE("node state/non dependant process : z-1")
{
    LLVMContext llvm_context;
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);
    float input, output;
    compile_node_class in{0u, 1u}, out{1u, 0u};

    last_node node;

    in.connect(node, 0u);
    node.connect(out, 0u);

    context.compile({in}, {out});
    context.update_program();

    input = 1.f;
    context.process(&input, &output);
    //  State are zero initialized on creation
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

TEST_CASE("node state/non dependant process : z-1 integrator with delayless cycle")
{
    LLVMContext llvm_context;
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);

    compile_node_class in{0u, 1u}, out{1u, 0u};
    add_node add;
    last_node delay;

    const float input = 1.0f;
    float output = 0.0f;

    in.connect(add, 0u);
    add.connect(delay, 0u);
    delay.connect(add, 1u);
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
}

class static_memory_simple_test : public compile_node_class
{
public:
    static_memory_simple_test()
        : compile_node_class(0u, 1u, 0u, true)
    {
        // 1 input, float size static memory
    }

    std::vector<llvm::Value *> emit_outputs(
        graph_compiler &compiler,
        const std::vector<llvm::Value *> &,
        llvm::Value *,
        llvm::Value *static_memory) const override
    {
        // Load the static memory chunk to outputs
        auto &builder = compiler.builder();
        auto float_ptr = builder.CreateBitCast(
            static_memory, llvm::Type::getFloatPtrTy(builder.getContext()));
        return { builder.CreateLoad(builder.getFloatTy(), float_ptr) };
    }
};

static std::vector<uint8_t> create_dummy_chunk(float value)
{
    std::vector<uint8_t> data(sizeof(float));
    std::memcpy(data.data(), &value, sizeof(float));
    return data;
}

TEST_CASE("Static memory : simple")
{
    LLVMContext llvm_context;
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);
    compile_node_class out{1u, 0u};
    static_memory_simple_test node;

    float output = 1.f;

    node.connect(out, 0);

    // First compilation, without registering static memory
    context.compile({}, {out});
    context.update_program();
    context.process(nullptr, &output);

    REQUIRE(output == Approx(0.f));

    // Register a static memory chunk and recompile
    context.register_static_memory_chunk(node, create_dummy_chunk(42.f));
    context.compile({}, {out});
    context.update_program();
    context.process(nullptr, &output);

    REQUIRE(output == Approx(42.f));

    // Free the static memory chunk
    context.free_static_memory_chunk(node);

    // it still exist for the process thread
    context.process(nullptr, &output);
    REQUIRE(output == Approx(42.f));

    // recompile
    context.compile({}, {out});

    // it still exist for the process thread (program have not be updated)
    context.process(nullptr, &output);
    REQUIRE(output == Approx(42.f));

    // chunk removal was taken into account
    context.update_program();
    context.process(nullptr, &output);
    REQUIRE(output == Approx(0.f));

    // refree the chunk should not trigger an exception (does nothing)
    context.free_static_memory_chunk(node);

    // Set another chunk
    context.register_static_memory_chunk(node, create_dummy_chunk(11.f));
    context.compile({}, {out});
    context.update_program();
    context.process(nullptr, &output);
    REQUIRE(output == Approx(11.f));

    // Change chunk
    context.register_static_memory_chunk(node, create_dummy_chunk(45.f));
    context.process(nullptr, &output);
    REQUIRE(output == Approx(11.f));

    context.compile({}, {out});
    context.process(nullptr, &output);
    REQUIRE(output == Approx(11.f));

    context.update_program();
    context.process(nullptr, &output);
    REQUIRE(output == Approx(45.f));
}
