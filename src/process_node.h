#ifndef JITTEST_PROCESS_NODE_H
#define JITTEST_PROCESS_NODE_H

#include <functional>
#include <vector>

#include "node.h"

namespace ProcessGraph {

    template <typename Tsample>
    class process_node : public node<process_node<Tsample>> {
    public:
        process_node(const unsigned int input_count) :
            node<process_node<Tsample>>(input_count)
        {}

        virtual ~process_node() {}
        virtual void process(const Tsample *in) {}

        unsigned int process_cycle{0u};
        Tsample output{};
    };

    template <typename Tsample>
    void graph_process_helper(process_node<Tsample>& node)
    {
        const auto input_count = node.get_input_count();
        std::vector<Tsample> input_buffer(input_count);

        //  Compute dependencies input buffer
        for (auto i = 0u; i < input_count; ++i) {
            auto *input = node.get_input(i);

            if (input == nullptr) {
                input_buffer[i] = Tsample{};
            }
            else {
                if (node.process_cycle != input->process_cycle) {
                    graph_process_helper(*input);
                    input->process_cycle = node.process_cycle;
                }
                input_buffer[i] = input->output;
            }
        }

        // do processing
        node.process(input_buffer.data());
    }

    template <typename Tsample>
    using process_node_ref_vector =
        std::vector<
            std::reference_wrapper<
                process_node<Tsample>
        >>;

    template <typename Tsample>
    void graph_process(
        const process_node_ref_vector<Tsample>& inputs,
        const process_node_ref_vector<Tsample>& outputs,
        const Tsample *input_array,
        Tsample *output_array)
    {
        static unsigned int process_cycle = 0u;

        process_cycle++;

        {
            auto input_idx = 0u;
            for (const auto& ref : inputs) {
                ref.get().process_cycle = process_cycle;
                ref.get().output = input_array[input_idx];
                input_idx++;
            }
        }

        {
            auto output_idx = 0u;

            for (const auto& ref : outputs) {
                const auto input_count = ref.get().get_input_count();

                for (auto i = 0u; i < input_count; ++i) {
                    auto *input = ref.get().get_input(i);

                    if (input == nullptr) {
                        output_array[output_idx] = Tsample{};
                    }
                    else {
                        if (input->process_cycle != process_cycle) {
                            input->process_cycle = process_cycle;
                            graph_process_helper(*input);
                        }
                        output_array[output_idx] = input->output;
                    }

                    output_idx++;
                }
            }
        }

    }


}

#endif //JITTEST_PROCESS_NODE_H
