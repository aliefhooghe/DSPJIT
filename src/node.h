#ifndef JITTEST_NODE_H
#define JITTEST_NODE_H

#include <vector>

namespace ProcessGraph {

    template <typename Tderived>
    class node {

    public:
        node(const unsigned int input_count) :
            _input(input_count, nullptr)
        {}

        virtual ~node() {}

        void connect(Tderived& target, const unsigned int target_input)
        {
            target._input[target_input] =
                    static_cast<Tderived*>(this);
        }

        Tderived *get_input(const unsigned int input_id) const
        {
            return _input[input_id];
        }

        const unsigned int get_input_count() const { return _input.size(); }

    private:
        std::vector<Tderived*> _input;
    };

}

#endif //JITTEST_NODE_H
