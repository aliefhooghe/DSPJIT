#ifndef JITTEST_NODE_H
#define JITTEST_NODE_H

#include <vector>
#include <unordered_set>
#include <functional>

namespace DSPJIT {

    template <typename Tderived>
    class node {

        class input {
            public:
                input() = default;

                input(const input&i)    {   plug(i._source);    }
                input(input&& i)        {   plug(i._source);    }

                ~input()
                {
                    unplug();
                }

                void plug(Tderived* n, unsigned int output_id)
                {
                    unplug();
                    _source = n;
                    _source->_users.insert(this);
                    _output_id = output_id;
                }

                void unplug()
                {
                    if (_source == nullptr)
                        return;

                    _source->_users.erase(this);
                    _source = nullptr;
                }

                auto get_output_id() const noexcept { return _output_id;   };
                auto get_source() const noexcept { return _source;      };

            private:
                Tderived *_source{nullptr};
                unsigned int _output_id{0u};
        };


    public:
        node(unsigned int input_count, unsigned int output_count = 1u) :
            _input(input_count),
            _output_count(output_count)
        {}

        virtual ~node()
        {
            //  this avoid iterators invalidation hazard
            for (auto it = _users.begin(); it != _users.end(); (*it++)->unplug());
        }

        void connect(Tderived& target, unsigned int target_input_id)
        {
            //  default is first output
            connect(0u, target, target_input_id);
        }

        void connect(unsigned int output_id, Tderived& target, unsigned int target_input_id)
        {
            if (target_input_id >= target.get_input_count() || output_id >= get_output_count())
                throw std::runtime_error("Node : connect : invalid I/O");
            target._input[target_input_id].plug(static_cast<Tderived*>(this), output_id);
        }

        Tderived *get_input(unsigned int input_id) const
        {
            if (input_id >= get_input_count())
                throw std::runtime_error("Node : get_input : invalid I/O");
            return _input[input_id].get_source();
        }

        Tderived *get_input(unsigned int input_id, unsigned int &output_id) const
        {
            auto& input = _input[input_id];

            if (input.get_source() != nullptr) {
                output_id = input.get_output_id();
                return input.get_source();
            }

            return nullptr;
        }

        const unsigned int get_input_count() const noexcept { return _input.size(); }
        const unsigned int get_output_count() const noexcept { return _output_count; }

    private:
        std::unordered_set<input*> _users{};
        std::vector<input> _input;
        const unsigned int _output_count;
    };



}

#endif //JITTEST_NODE_H
