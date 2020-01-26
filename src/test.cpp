#include <iostream>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include "test_implentations.h"
#include "jit_compiler.h"

using namespace llvm;
using namespace ProcessGraph;


void test_last()
{
    std::cout << "TEST LAST COMPONENT (state)" << std::endl;
    graph_execution_context context;

    float input;

    reference_compile_node inputc{context, input};
    reference_process_node inputp{input};

    last_compile_node lc{context, 0.0f};
    last_process_node lp{0.0f};

    inputc.connect(lc, 0u);
    inputp.connect(lp, 0u);

    context.compile(lc);

    for (auto i = 0; i < 10; ++i) {
        input = static_cast<float>(i);
        std::cout << "input : " << input << ", lp : " << graph_process(lp) << ", lc :" << context.process() << std::endl;
    }
}

void test_cycle()
{
    std::cout << "TEST INTEGRATOR CIRCUIT (cycle state)" << std::endl;
    graph_execution_context context;

    float input = 0;

    reference_compile_node inputc{context, input};
    reference_process_node inputp{input};

    add_compile_node integrator_c{context};
    add_process_node<float> integrator_p{};

    //  Use cyle to integrate
    inputc.connect(integrator_c, 0);
    integrator_c.connect(integrator_c, 1);

    inputp.connect(integrator_p, 0);
    integrator_p.connect(integrator_p, 1);

    context.compile(integrator_c);
    for (auto i = 0; i < 10; ++i) {
        input = static_cast<float>(i);
        std::cout << "input : " << input << ", lp : " << graph_process(integrator_p) << ", lc :" << context.process() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    float x;

    /**
     *   Compute 41 + x (x as ref)
     **/

    //**  dynamic circuit
    reference_process_node<float> rp{x};
    constant_process_node<float> cp2{41.0f};
    add_process_node<float> ap{};

    cp2.connect(ap, 1u);

    //** compiled circuit
    graph_execution_context context;
    reference_compile_node rc{context, x};
    constant_compile_node cc2{context, 41.0f};
    add_compile_node ac{context};

    cc2.connect(ac, 1u);

    /* compile to native code */
    context.compile(ac);

    //--------------------------------------------------------------

    // Execute

    std::cout << "Set x = 10" << std::endl;
    x = 10;

    std::cout << "dynamic version return " << graph_process(ap) << std::endl;
    std::cout << "compiled version return " << context.process() << std::endl;

    std::cout << "link ref to add and recompile" << std::endl;

    rp.connect(ap, 0u);
    rc.connect(ac, 0u);

    context.compile(ac);

    std::cout << "dynamic version return " << graph_process(ap) << std::endl;
    std::cout << "compiled version return " << context.process() << std::endl;


    test_last();
    test_cycle();

    return 0;
}