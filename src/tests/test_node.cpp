
#include <catch2/catch.hpp>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include "node.h"

using namespace DSPJIT;

/**
 *      Node
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
