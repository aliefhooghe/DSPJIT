

#include <iostream>
#include <algorithm>

#include "ir_optimization.h"
#include "ir_helper.h"
#include "compile_node_class.h"
#include "graph_state_manager.h"

namespace DSPJIT {

    compile_node_class::compile_node_class(
            const unsigned int input_count,
            const unsigned int output_node,
            std::size_t mutable_state_size_bytes,
            bool use_static_mem,
            bool dependant_process)
    : node<compile_node_class>{input_count, output_node},
            mutable_state_size{mutable_state_size_bytes},
            use_static_memory{use_static_mem},
            dependant_process{dependant_process}
    {}
}