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

                auto get_output_id() const { return _output_id;   };
                auto get_source()    const { return _source;      };

            private:
                Tderived *_source{nullptr};
                unsigned int _output_id{0u};
        };


    public:
        node(const unsigned int input_count) :
            _input(input_count)
        {}

        virtual ~node()
        {
            //  this avoid iterators invalidation hazard
            for (auto it = _users.begin(); it != _users.end(); (*it++)->unplug());
        }

        void connect(Tderived& target, const unsigned int target_input)
        {
            if (target_input >= target.get_input_count())
                throw std::runtime_error("Node :  connect : invalid input");
            target._input[target_input].plug(static_cast<Tderived*>(this), 0u);
        }

        Tderived *get_input(const unsigned int input_id) const
        {
            return _input[input_id].get_source();
        }

        const unsigned int get_input_count() const { return _input.size(); }

    private:
        std::unordered_set<input*> _users{};
        std::vector<input> _input;
    };



}

#endif //JITTEST_NODE_H
