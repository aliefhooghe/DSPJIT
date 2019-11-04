#include <iostream>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include "test_implentations.h"
#include "jit_compiler.h"

using namespace llvm;
using namespace ProcessGraph;

void print_module(const llvm::Module& module)
{
    llvm::raw_os_ostream stream{std::cout};
    module.print(stream, nullptr);
}

void test_last()
{
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

    return 0;
}