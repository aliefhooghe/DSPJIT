#ifndef JITTEST_PROCESS_NODE_H
#define JITTEST_PROCESS_NODE_H

#include "node.h"

namespace ProcessGraph {

    template <typename Tsample>
    class process_node : public node<process_node<Tsample>> {
    public:
        process_node(const unsigned int input_count) :
            node<process_node<Tsample>>(input_count)
        {}

        virtual ~process_node() {}

        void process(const std::vector<Tsample>& input)
        {
            process_cycle++;
            do_process(input);
        }

        Tsample get_output() const { return output; }
        auto get_process_cycle() const { return process_cycle; }
        void next_process_cycle() { process_cycle++; }

    protected:
        virtual void do_process(const std::vector<Tsample>& input) = 0;

        unsigned int process_cycle{0u};
        Tsample output{};
    };

    template <typename Tsample>
    void graph_process_helper(process_node<Tsample>& node)
    {
        const auto input_count = node.get_input_count();
        std::vector<Tsample> input_buffer(input_count);

        node.next_process_cycle();

        //  Compute dependencies input buffer
        for (auto i = 0u; i < input_count; ++i) {
            auto *input = node.get_input(i);

            if (input == nullptr) {
                input_buffer[i] = Tsample{};
            }
            else {
                if (node.get_process_cycle() != input->get_process_cycle())
                    graph_process_helper(*input);
                input_buffer[i] = input->get_output();
            }
        }

        // do processing
        node.process(input_buffer);
    }

    template <typename Tsample>
    Tsample graph_process(process_node<Tsample>& node)
    {
        graph_process_helper<Tsample>(node);
        return node.get_output();
    }


}

#endif //JITTEST_PROCESS_NODE_H
