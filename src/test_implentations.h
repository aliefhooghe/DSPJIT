#ifndef JITTEST_TEST_IMPLENTATIONS_H
#define JITTEST_TEST_IMPLENTATIONS_H

#include "compile_node_class.h"
#include "process_node.h"

#include <math.h>

namespace ProcessGraph {

    // Constant node

    template <typename T>
    class constant_process_node : public process_node<T> {

    public:
        explicit constant_process_node(const T value) :
            process_node<T>(0u),
            _value{value}
        {}

    protected:
        void process(const T*) override
        {
            process_node<T>::output = _value;
        }

        const T _value;
    };

    class constant_compile_node : public compile_node_class {

    public:
        explicit constant_compile_node(
            graph_execution_context& context,const float value)
        :   compile_node_class(context, 0u),
            _value(value)
        {}

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override;
    private:
        const float _value;
    };

    //  Reference node

    template <typename T>
    class reference_process_node : public process_node<T> {
    public:
        explicit reference_process_node(const T& ref) :
            process_node<T>{0u},
            _ref{ref}
        {}

    protected:
        void process(const T*) override
        {
            process_node<T>::output = _ref;
        }

    private:
        const T& _ref;
    };

    class reference_compile_node : public compile_node_class {
    public:
        explicit reference_compile_node(
            graph_execution_context& context,
            const float& ref)
        :   compile_node_class{context, 0u},
            _ref{ref}
        {}

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override;

    private:
        const float& _ref;
    };

    // Add

    template <typename T>
    class add_process_node : public process_node<T> {

    public:
        add_process_node() :
            process_node<T>(2) {}

    protected:
        void process(const T *input) override
        {
            process_node<T>::output = input[0] + input[1];
        }
    };

    class add_compile_node : public compile_node_class {
    public:
        add_compile_node(graph_execution_context& context) :
            compile_node_class{context, 2}
        {}

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override;
    };

    // Mull

    template <typename Tsample>
    class mul_process_node : public process_node<Tsample> {

    public:
        mul_process_node() :
            process_node<Tsample>(2) {}

    protected:
        void process(const Tsample *input) override
        {
            process_node<Tsample>::output = input[0] * input[1];
        }
    };

    class mul_compile_node : public compile_node_class {
    public:
        mul_compile_node(graph_execution_context& context)
        :   compile_node_class{context, 2}
        {}

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override;
    };

    // Z^-1

    template <typename T>
    class last_process_node : public process_node<T> {

    public:
        last_process_node(const T& initial_value) :
            process_node<T>(1),
            _last{initial_value}
        {}

    protected:
        void process(const T *input) override
        {
            const T ret = _last;
            _last = input[0];
            process_node<T>::output = ret;
        }

    private:
        T _last;
    };

    class last_compile_node : public compile_node_class {

    public:
        last_compile_node(
            graph_execution_context& context,
            const float initial_value)
        :   compile_node_class{context, 1, sizeof(float)},
            _initial_value{initial_value}
        {}

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override;

    private:
        const float _initial_value;
    };

    // unary function node (only function, no lambda or callablo object because for compile node we need a symbol)

    template <float unary(float)>
    class unary_function_process_node : public process_node<float> {

    protected:
        void process(const float *input) override
        {
            process_node<float>::output = unary(input[0]);
        }
    };

    template <float unary(float)>
    class unary_function_compile_node : public compile_node_class {
    public:
        unary_function_compile_node(graph_execution_context& context)
        : compile_node_class{context, 1}
        {}

        std::vector<llvm::Value*> emit_outputs(
                llvm::IRBuilder<>& builder,
                const std::vector<llvm::Value*>& inputs,
                llvm::Value *mutable_state_ptr) const override
        {
            //  TODO
            return {nullptr};
        }
    };


}

#endif