#ifndef JITTEST_NODE_H
#define JITTEST_NODE_H

#include <vector>
#include <set>
#include <functional>

namespace DSPJIT {

    template <typename Derived>
    class node {

        class input {
            public:
                input() = default;

                input(const input&i) noexcept
                {
                    if (i._source)
                        plug(i._source, i._output_id);
                }

                input(input&& i) noexcept
                {
                    if (i._source)
                        plug(i._source, i._output_id);
                }

                ~input()
                {
                    unplug();
                }

                void plug(Derived* n, unsigned int output_id)
                {
                    unplug();
                    _source = n;
                    _source->_users.insert(std::make_pair(this, output_id));
                    _output_id = output_id;
                }

                void unplug()
                {
                    if (_source == nullptr)
                        return;

                    _source->_users.erase(std::make_pair(this, _output_id));
                    _source = nullptr;
                }

                auto get_output_id() const noexcept { return _output_id;   };
                auto get_source() const noexcept { return _source;      };

            private:
                Derived *_source{nullptr};
                unsigned int _output_id{0u};
        };


    public:
        node(unsigned int input_count, unsigned int output_count) :
            _input(input_count),
            _output_count(output_count)
        {}

        virtual ~node()
        {
            //  this avoid iterators invalidation hazard
            for (auto it = _users.begin(); it != _users.end(); (*it++).first->unplug());
        }

        void connect(Derived& target, unsigned int target_input_id)
        {
            //  default is first output
            connect(0u, target, target_input_id);
        }

        void connect(unsigned int output_id, Derived& target, unsigned int target_input_id)
        {
            if (target_input_id >= target.get_input_count() || output_id >= get_output_count())
                throw std::runtime_error("Node : connect : invalid I/O");
            target._input[target_input_id].plug(static_cast<Derived*>(this), output_id);
        }

        void disconnect(unsigned int input_id)
        {
            if (input_id >= get_input_count())
                throw std::runtime_error("Node : disconnect : invalid I/O");
            _input[input_id].unplug();
        }

        Derived *get_input(unsigned int input_id) const
        {
            if (input_id >= get_input_count())
                throw std::runtime_error("Node : get_input : invalid I/O");
            return _input[input_id].get_source();
        }

        Derived *get_input(unsigned int input_id, unsigned int &output_id) const
        {
            auto& input = _input[input_id];

            if (input.get_source() != nullptr) {
                output_id = input.get_output_id();
                return input.get_source();
            }

            return nullptr;
        }

        virtual void add_input()
        {
            _input.push_back(input{});
        }

        virtual void remove_input()
        {
            if (get_input_count() > 0u)
                _input.pop_back();
        }

        virtual void add_output()
        {
            _output_count++;
        }

        virtual void remove_output()
        {
            if (get_output_count() > 0u) {
                const auto removed_output_id = _output_count - 1u;

                for (auto it = _users.begin(); it != _users.end();)
                {
                    if (it->second == removed_output_id)
                        (*it++).first->unplug();
                    else
                        it++;
                }

                _output_count--;
            }
        }

        const unsigned int get_input_count() const noexcept { return _input.size(); }
        const unsigned int get_output_count() const noexcept { return _output_count; }

    private:
        std::set<std::pair<input*, unsigned int>> _users{}; // user input, output id
        std::vector<input> _input;
        unsigned int _output_count;
    };

}

#endif //JITTEST_NODE_H
