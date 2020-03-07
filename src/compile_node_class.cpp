
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/CommandLine.h>

#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>

#include <map>
#include <iostream>
#include <algorithm>

#include "ir_optimization.h"
#include "ir_helper.h"
#include "compile_node_class.h"

namespace DSPJIT {

    compile_node_class::compile_node_class(
            const unsigned int input_count,
            std::size_t mutable_state_size_bytes)
    : node<compile_node_class>{input_count},
            mutable_state_size{mutable_state_size_bytes}
    {}

    //---

    graph_execution_context::graph_execution_context(
        std::size_t instance_count,
        const llvm::TargetOptions& options)
    :   _sequence{0u},
        _instance_count{instance_count},
        _ack_msg_queue{3},
        _compile_done_msg_queue{3}
    {
        static auto llvm_native_was_init = false;
        if (llvm_native_was_init == false) {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm_native_was_init = true;
        }

        //  Initialize executionEngine

        /**
         *  todo options, InitializeNativeTarget, ...,
         **/
        auto memory_mgr =
            std::unique_ptr<llvm::RTDyldMemoryManager>(new llvm::SectionMemoryManager());

        _execution_engine =
            std::unique_ptr<llvm::ExecutionEngine>(
                llvm::EngineBuilder{std::make_unique<llvm::Module>("dummy", _llvm_context)}
                .setEngineKind(llvm::EngineKind::JIT)
                .setTargetOptions(options)
                .setMCJITMemoryManager(std::move(memory_mgr))
                .create());

        //  Default native function does nothing
        _process_func = default_process_func;

        //  Create the associated delete_sequence
        _delete_sequences.emplace(_sequence, delete_sequence{*_execution_engine});
    }

    graph_execution_context::~graph_execution_context()
    {

    }

    void graph_execution_context::register_JITEventListener(llvm::JITEventListener* listener)
    {
        _execution_engine->RegisterJITEventListener(listener);
    }

    void graph_execution_context::add_module(std::unique_ptr<llvm::Module>&& module)
    {
        _execution_engine->addModule(std::move(module));
    }

    void graph_execution_context::compile(
            const node_ref_vector& input_nodes,
            const node_ref_vector& output_nodes)
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

        //  Memoise nodes noutput values
        std::map<const compile_node_class*, llvm::Value*> node_values;

        // Create ir function

        //  function signature : void _(int64 instance_num, float *inputs, float *outputs)
        std::vector<llvm::Type*> arg_types{
            Type::getInt64Ty(_llvm_context),
            Type::getFloatPtrTy(_llvm_context),
            Type::getFloatPtrTy(_llvm_context)};

        auto func_type = FunctionType::get(Type::getVoidTy(_llvm_context), arg_types, false /* is_var_arg */);
        auto function = Function::Create(func_type, Function::ExternalLinkage, "", module.get());

        //  Create function code block
        BasicBlock *basic_block = BasicBlock::Create(_llvm_context, "", function);
        builder.SetInsertPoint(basic_block);

        //  Get arguments
        auto arg_begin = function->arg_begin();
        auto instance_num_value = arg_begin++;
        auto inputs_array_value = arg_begin++;
        auto outputs_array_value = arg_begin++;

        //  generate code that load inputs from input array and
        //  register input_nodes output as value. (for now 1 output per node)
        {
            auto input_index = 0u;
            for (const auto& input_node : input_nodes) {
                auto index_value = ConstantInt::get(_llvm_context, APInt(64, input_index));
                auto input_ptr =  builder.CreateGEP(inputs_array_value, index_value);
                auto input_val = builder.CreateLoad(input_ptr);
                node_values[&input_node.get()] = input_val;
                input_index++;
            }
        }

        //  Compute output_nodes inputs and store them to output array
        {
            auto output_index = 0u;
            for (const auto& output_node : output_nodes) {
                const auto input_count = output_node.get().get_input_count();

                for (auto i = 0u; i < input_count; ++i) {
                    const auto dependency_node = output_node.get().get_input(i);
                    auto value = compile_node_helper(builder, dependency_node, instance_num_value, node_values);
                    auto index_value = llvm::ConstantInt::get(_llvm_context, llvm::APInt(64, output_index));
                    auto output_ptr = builder.CreateGEP(outputs_array_value, index_value);
                    builder.CreateStore(value, output_ptr);
                    output_index++;
                }
            }
        }

        //  Finish function by insterting a ret instruction
        builder.CreateRetVoid();

        {
            //  get iterator to current (last) delete sequence (map can't be empty)
            auto del_seq_it = _delete_sequences.rbegin();

            //  State management : find states that are not anymore useds
            for (auto state_it = _state.begin(); state_it != _state.end(); ++state_it) {
                //  if this node is not used anymore
                if (node_values.find(state_it->first) == node_values.end())
                {
                    //  Move the state in the delete_sequence in order to make it deleted when possible
                    del_seq_it->second.add_deleted_node(std::move(state_it->second));

                    //  Remove the coresponding entry in state store
                    _state.erase(state_it);
                }
            }
        }

#ifndef NDEBUG
        LOG_INFO("[graph_execution_context][compile thread] IR code before optimization");
        ir_helper::print_module(*module);
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
            run_optimization(*module, *function);

#ifndef NDEBUG
            LOG_INFO("[graph_execution_context][compile thread] IR code after optimization");
            ir_helper::print_module(*module);
#endif

            auto module_ptr = module.get();

            //  Compile module to native code
            _execution_engine->addModule(std::move(module));
            _execution_engine->finalizeObject();

            raw_func native_func =
                reinterpret_cast<raw_func>(_execution_engine->getPointerToFunction(function));

            /**
             *      Notify process thread that a native function is ready
             **/
            _sequence++;
            _compile_done_msg_queue.enqueue(compile_done_msg{_sequence, native_func});
            _delete_sequences.emplace(_sequence, delete_sequence{*_execution_engine, module_ptr});
            LOG_DEBUG("[graph_execution_context][compile thread] graph compilation finnished, send compile_done message to process thread (seq = %u)", _sequence);
        };
    }

    llvm::Value *graph_execution_context::compile_node_helper(
            llvm::IRBuilder<>& builder,
            const compile_node_class* node,
            llvm::Value *instance_num_value,
            std::map<const compile_node_class*, llvm::Value*>& values)
    {
        if (node == nullptr)
            return llvm::ConstantFP::get(builder.getContext(), llvm::APFloat(0.0f));


        const auto input_count = node->get_input_count();
        std::vector<llvm::Value*> input_values(input_count);

        // Check if a state have been created for this node
        auto state_it = _state.find(node);
        if (state_it == _state.end()) {
            //  If not create state
            auto ret = _state.emplace(node, mutable_node_state(node->mutable_state_size, _instance_count));
            assert(ret.second);
            state_it = ret.first;
        }

        //
        auto value_it = values.lower_bound(node);

        if (value_it != values.end() && !(values.key_comp()(node, value_it->first))) {
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
            value_it = values.emplace_hint(value_it, node, nullptr);
        }

        //  Compile dependencies input
        for (auto i = 0u; i < input_count; ++i) {
            const auto *input = node->get_input(i);
            input_values[i] = compile_node_helper(builder, input, instance_num_value, values);
        }

        //  get state ptr
        llvm::Value *state_ptr = nullptr;

        if (node->mutable_state_size != 0u) {
            // state_ptr <- base + instance_num * state size
            state_ptr =
                create_array_ptr(
                    builder,
                    ir_helper::runtime::get_raw_pointer(builder, state_it->second.data.data()),
                    instance_num_value,
                    node->mutable_state_size);
        }

        /**
         *
         *  TODO :      GERER LES OUTPUTS MULTIPLES !!!
         * ########
         *
         **/

        // compile processing
        const auto output_values =
            node->emit_outputs(builder, input_values, state_ptr);

        //  emit instruction to store output into cycle_state if cycle_state value was created
        if (value_it->second != nullptr) {
            auto cycle_ptr_value =
                    create_array_ptr(
                        builder,
                        ir_helper::runtime::get_raw_pointer(builder, state_it->second.cycle_state.data()),
                        instance_num_value,
                        sizeof(float));

            builder.CreateStore(
                output_values[0], ir_helper::runtime::raw2typed_ptr<float>(builder, cycle_ptr_value));
        }

        // record output value
        value_it->second = output_values[0];

        return output_values[0];
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

    void graph_execution_context::process(std::size_t instance_num, const float * inputs, float *outputs)
    {
        compile_done_msg msg;

        //  Process one compile done msg (if any)
        //  and update native code ptr
        if (_compile_done_msg_queue.dequeue(msg))
            _process_compile_done_msg(msg);

        //  run native code
        _process_func(instance_num, inputs, outputs);
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