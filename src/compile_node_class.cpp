
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/CommandLine.h>

#include <map>
#include <iostream>
#include <algorithm>

#include "jit_compiler.h"
#include "ir_helper.h"
#include "compile_node_class.h"
#include "log.h"

namespace ProcessGraph {

    compile_node_class::compile_node_class(
            graph_execution_context& context,
            const unsigned int input_count,
            std::size_t mutable_state_size_bytes)
    : node<compile_node_class>{input_count},
            _context{context},
            mutable_state_size{mutable_state_size_bytes}
    {}

    compile_node_class::~compile_node_class()
    {
        _context.notify_delete_node(this);
    }

    //---

    graph_execution_context::graph_execution_context(std::size_t instance_count)
    :   _sequence{0u},
        _instance_count{instance_count},
        _ack_msg_queue{3},
        _compile_done_msg_queue{3}
    {
        //  Default native function does nothing
        _process_func = default_process_func;

        //  Create the associated delete_sequence (without execution engine)
        _delete_sequences.emplace(_sequence, delete_sequence{});
    }

    graph_execution_context::~graph_execution_context()
    {

    }

    static void print_module(const llvm::Module& module)
    {
        llvm::raw_os_ostream stream{std::cout};
        module.print(stream, nullptr);
    }

    llvm::Value *compile_node_value(
            llvm::IRBuilder<>& builder,
            const compile_node_class& node);

    void graph_execution_context::compile(compile_node_class& output_node)
    {
        using namespace llvm;

        LOG_INFO("[graph_execution_context][compile thread] graph compilation");

        /**
         *      Process acq_msg : Clean unused stuff
         **/
        ack_msg msg;
        if (_ack_msg_queue.dequeue(msg))
            _process_ack_msg(msg);

        auto module = std::make_unique<Module>("MODULE", _llvm_context);

        /**
         *      Compile graph to LLVM IR
         **/
        IRBuilder builder(_llvm_context);

        // Create a function
        std::vector<llvm::Type*> arg_types{Type::getInt64Ty(_llvm_context)};
        auto func_type = FunctionType::get(Type::getFloatTy(_llvm_context), arg_types, false /* is_var_arg */);
        auto function = Function::Create(func_type, Function::ExternalLinkage, "", module.get());

        BasicBlock *basic_block = BasicBlock::Create(_llvm_context, "", function);
        builder.SetInsertPoint(basic_block);

        //  Get argument
        auto instance_num_value = function->arg_begin();

        // Compile return value
        auto ret_val = compile_node_value(
            builder, output_node, instance_num_value);
        builder.CreateRet(ret_val);

#ifndef NDEBUG
        LOG_INFO("[graph_execution_context][compile thread] IR code before optimization");
        print_module(*module);
#endif

        //  Check generated IR code
        raw_os_ostream stream{std::cout};
        if (verifyFunction(*function, &stream)) {
            LOG_ERROR("[graph_execution_context][Compile Thread] Malformed IR code, canceling compilation");
            //  Do not compile to native code because malformed code could lead to crash
            //  Stay at last process_func
        }
        else {
            /**
             *      Compile LLVM IR to native code
             **/
            jit_test::run_optimization(*module, *function);

#ifndef NDEBUG
            LOG_INFO("[graph_execution_context][compile thread] IR code after optimization");
            print_module(*module);
#endif

            auto engine = jit_test::build_execution_engine(std::move(module));

            raw_func native_func =
                reinterpret_cast<raw_func>(engine->getPointerToFunction(function));

            /**
             *      Notify process thread that a native function is ready
             **/
            _sequence++;
            _compile_done_msg_queue.enqueue(compile_done_msg{_sequence, native_func});
            _delete_sequences.emplace(_sequence, delete_sequence{std::move(engine)});
            LOG_DEBUG("[graph_execution_context][compile thread] graph compilation finnished, send compile_done message to process thread (seq = %u)", _sequence);
        };
    }

    llvm::Value *graph_execution_context::compile_node_helper(
            llvm::IRBuilder<>& builder,
            const compile_node_class& node,
            llvm::Value *instance_num_value,
            std::map<const compile_node_class*, llvm::Value*>& values)
    {
        const auto input_count = node.get_input_count();
        std::vector<llvm::Value*> input_values(input_count);

        // Check if a state have been created for this node
        auto state_it = _state.find(&node);
        if (state_it == _state.end()) {
            //  If not create state
            auto ret = _state.emplace(&node, mutable_node_state(node.mutable_state_size, _instance_count));
            assert(ret.second);
            state_it = ret.first;
        }

        //
        auto value_it = values.lower_bound(&node);

        if (value_it != values.end() && !(values.key_comp()(&node, value_it->first))) {
            //  This node have been visited : there is a cycle

            if (value_it->second == nullptr) {
                //  This cycle have not been discovered : create a value with cycle state

                auto cycle_ptr_value =
                    create_array_ptr(
                        builder,
                        ir_helper::runtime::get_raw_pointer(builder, state_it->second.cycle_state.data()),
                        instance_num_value,
                        sizeof(float));

                value_it->second =
                    builder.CreateLoad(ir_helper::runtime::raw2typed_ptr<float>(builder, cycle_ptr_value));
            }

            //  Return cycle_state value
            return value_it->second;
        }
        else {
            //  This node have not been visited, create a null output value
            value_it = values.emplace_hint(value_it, &node, nullptr);
        }

        //  Compile dependencies input
        for (auto i = 0u; i < input_count; ++i) {
            const auto *input = node.get_input(i);

            if (input == nullptr) {
                input_values[i] =
                    llvm::ConstantFP::get(builder.getContext(), llvm::APFloat(0.0f));
            }
            else {
                input_values[i] = compile_node_helper(builder, *input, instance_num_value, values);
            }
        }

        //  get state ptr
        llvm::Value *state_ptr = nullptr;

        if (node.mutable_state_size != 0u) {
            // state_ptr <- base + instance_num * state size
            state_ptr =
                create_array_ptr(
                    builder,
                    ir_helper::runtime::get_raw_pointer(builder, state_it->second.data.data()),
                    instance_num_value,
                    node.mutable_state_size);
        }

        // compile processing
        auto *value =
            node.compile(
                builder,
                input_values,
                state_ptr);

        //  emit instruction to store output into cycle_state if cycle_state value was created
        if (value_it->second != nullptr) {
            auto cycle_ptr_value =
                    create_array_ptr(
                        builder,
                        ir_helper::runtime::get_raw_pointer(builder, state_it->second.cycle_state.data()),
                        instance_num_value,
                        sizeof(float));

            builder.CreateStore(
                value, ir_helper::runtime::raw2typed_ptr<float>(builder, cycle_ptr_value));
        }

        // record output value
        value_it->second = value;

        return value;
    }

    llvm::Value *graph_execution_context::compile_node_value(
            llvm::IRBuilder<>& builder,
            const compile_node_class& node,
            llvm::Value *instance_num_value)
    {
        std::map<const compile_node_class*, llvm::Value*> values;
        return compile_node_helper(builder, node, instance_num_value, values);
    }

    llvm::Value *graph_execution_context::create_array_ptr(
            llvm::IRBuilder<>& builder,
            llvm::Value *base,
            llvm::Value *index,
            std::size_t block_size)
    {
        return builder.CreateAdd(
                    base,
                    builder.CreateMul(
                        index,
                        llvm::ConstantInt::get(_llvm_context,llvm::APInt(64, block_size))));
    }

    void graph_execution_context::notify_delete_node(compile_node_class *node)
    {
        /**
         *      We can't remove directly the state as it is maybe used by the process_thread
         */

        auto it = _state.find(node);
        //  If we have a state recorded for this node
        if (it != _state.end()) {
            //  Move the state in the delete_sequence in order to make it deleted when possible
            _delete_sequences[_sequence].add_deleted_node(std::move(it->second));

            //  Remove the coresponding enrty in state store
            _state.erase(it);
        }
    }

    float graph_execution_context::process(std::size_t instance_num)
    {
        compile_done_msg msg;

        //  Process one compile done msg (if any)
        if (_compile_done_msg_queue.dequeue(msg))
            _process_compile_done_msg(msg);

        return _process_func(instance_num);
    }


    void graph_execution_context::_process_ack_msg(const ack_msg msg)
    {
        LOG_DEBUG("[graph_execution_context][compile thread] received acknowledgment from process thread (seq = %u)", msg);

        //  Erase all previous delete_sequence
        _delete_sequences.erase(
            _delete_sequences.begin(),
            _delete_sequences.lower_bound(msg));
    }

    void graph_execution_context::_process_compile_done_msg(const compile_done_msg msg)
    {
        LOG_DEBUG("[graph_execution_context][process thread] received compile done from compile thread (seq = %u). Send acknoledgment to compile thread", msg.first);
        //  Use the new process func
        _process_func = msg.second;

        //  Send ack message to notify that old function is not anymore in use
        _ack_msg_queue.enqueue(msg.first);
    }

}