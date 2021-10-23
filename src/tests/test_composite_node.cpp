
#include <catch2/catch.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include <DSPJIT/graph_execution_context_factory.h>
#include <DSPJIT/composite_node.h>
#include <DSPJIT/common_nodes.h>

using namespace llvm;
using namespace DSPJIT;

/**
 *
 *      Composite Compile Node
 *
 **/

TEST_CASE("Composite Compile Node", "composite_node")
{
    LLVMContext llvm_context;
    graph_execution_context context =
        graph_execution_context_factory::build(llvm_context);
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
