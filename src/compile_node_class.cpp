
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/CommandLine.h>

#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>

#include <llvm/Linker/Linker.h>

#include <map>
#include <iostream>
#include <algorithm>

#include "ir_optimization.h"
#include "ir_helper.h"
#include "compile_node_class.h"

namespace DSPJIT {

    compile_node_class::compile_node_class(
            const unsigned int input_count,
            const unsigned int output_node,
            std::size_t mutable_state_size_bytes)
    : node<compile_node_class>{input_count, output_node},
            mutable_state_size{mutable_state_size_bytes}
    {}

    //---

    graph_execution_context::graph_execution_context(
        llvm::LLVMContext& llvm_context,
        std::size_t instance_count,
        const opt_level level,
        const llvm::TargetOptions& options)
    :   _llvm_context{llvm_context},
        _sequence{0u},
        _instance_count{instance_count},
        _ack_msg_queue{256},
        _compile_done_msg_queue{256}
    {
        static auto llvm_native_was_init = false;
        if (llvm_native_was_init == false) {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm_native_was_init = true;
        }

        //  Initialize executionEngine
        auto memory_mgr =
            std::unique_ptr<llvm::RTDyldMemoryManager>(new llvm::SectionMemoryManager());

        _execution_engine =
            std::unique_ptr<llvm::ExecutionEngine>(
                llvm::EngineBuilder{std::make_unique<llvm::Module>("dummy", _llvm_context)}
                .setEngineKind(llvm::EngineKind::JIT)
                .setTargetOptions(options)
                .setOptLevel(level)
                .setMCJITMemoryManager(std::move(memory_mgr))
                .create());

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
        _modules.emplace_back(std::move(module));
    }

    void graph_execution_context::compile(
            node_ref_list input_nodes,
            node_ref_list output_nodes)
    {
        LOG_INFO("[graph_execution_context][compile thread] graph compilation\n");

        _last_compiled_nodes.clear();

        // Process acq_msg : Clean unused stuff
        ack_msg msg;
        if (_ack_msg_queue.dequeue(msg))
            _process_ack_msg(msg);

        //  Create module and link all dependencies into it
        auto module = std::make_unique<llvm::Module>("GRAPH_EXECUTION_CONTEXT", _llvm_context);
        _link_dependency_modules(*module);

        value_memoize_map node_values{}; //  Memoise nodes output values

        //  Compile process function
        auto process_function =
            _compile_process_function(
                input_nodes,
                output_nodes,
                node_values,
                *module);

        auto initialize_function =
            _compile_initialize_function(*module);

        //  Remove state for nodes that are not anymore used
        _collect_unused_states();

#ifdef GAMMOU_PRINT_IR
        LOG_INFO("[graph_execution_context][compile thread] IR code before optimization\n");
        ir_helper::print_function(*process_function);
        ir_helper::print_function(*initialize_function);
#endif

        //  Check generated IR code
        llvm::raw_os_ostream stream{std::cout};
        if (llvm::verifyFunction(*process_function, &stream) || llvm::verifyFunction(*initialize_function, &stream)) {
            LOG_ERROR("[graph_execution_context][Compile Thread] Malformed IR code, canceling compilation\n");
            //  Do not compile to native code because malformed code could lead to crash
            //  Stay at last process_func
            return;
        }

        run_optimization(*module);

#ifdef GAMMOU_PRINT_IR
        LOG_INFO("[graph_execution_context][compile thread] IR code after optimization\n");
        ir_helper::print_function(*process_function);
        ir_helper::print_function(*initialize_function);
#endif

        //  Compile LLVM IR to native code
        _emit_native_code(std::move(module), process_function, initialize_function);
    }

    llvm::Function * graph_execution_context::_compile_process_function(
        node_ref_list input_nodes,
        node_ref_list output_nodes,
        value_memoize_map& node_values,
        llvm::Module& graph_module)
    {
        //  Create ir function : signature = void _(int64 instance_num, float *inputs, float *outputs)
        std::vector<llvm::Type*> arg_types{
            llvm::Type::getInt64Ty(_llvm_context),
            llvm::Type::getFloatPtrTy(_llvm_context),
            llvm::Type::getFloatPtrTy(_llvm_context)};

        auto func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(_llvm_context), arg_types, false /* is_var_arg */);
        auto function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "graph__process_function", &graph_module);

        //  Create function code block
        auto basic_block = llvm::BasicBlock::Create(_llvm_context, "", function);

        llvm::IRBuilder builder(_llvm_context);
        builder.SetInsertPoint(basic_block);

        //  Get arguments
        auto arg_begin = function->arg_begin();
        auto instance_num_value = arg_begin++;
        auto inputs_array_value = arg_begin++;
        auto outputs_array_value = arg_begin++;

        //  generate code that load inputs from input array and
        //  register input_nodes output as value.
        _load_graph_input_values(
            builder, input_nodes, inputs_array_value, node_values);

        //  Compute output_nodes inputs and store them to output array
        _compile_and_store_graph_output_values(
            builder, output_nodes, outputs_array_value, instance_num_value, node_values);

        //  Finish function by insterting a ret instruction
        builder.CreateRetVoid();
        return function;
    }

    llvm::Function *graph_execution_context::_compile_initialize_function(llvm::Module& graph_module)
    {
        auto func_type = llvm::FunctionType::get(llvm::Type::getVoidTy(_llvm_context), {llvm::Type::getInt64Ty(_llvm_context)}, false);
        auto function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "graph__initialize_function", &graph_module);
        auto instance_num_value = function->arg_begin();
        auto basic_block = llvm::BasicBlock::Create(_llvm_context, "", function);

        llvm::IRBuilder builder(_llvm_context);
        builder.SetInsertPoint(basic_block);

        //  for each node that have been used in the last compilation
        for (const auto& node : _last_compiled_nodes) {

            if (node->mutable_state_size == 0u)
            {
                //  Ignore stateless nodes
                continue;
            }

            auto state_it = _state.find(node);
            if (state_it == _state.end()) {
                LOG_ERROR("[graph_execution_context][Compile Thread] Compile initialize function : Did not find a state to initialize !\n");
                continue;
            }

            auto state_ptr = get_mutable_state_ptr(builder, state_it, instance_num_value);
            node->initialize_mutable_state(builder, state_ptr);
        }

        builder.CreateRetVoid();
        return function;
    }

    void graph_execution_context::_link_dependency_modules(llvm::Module& graph_module)
    {
        for (const auto& module : _modules)
            llvm::Linker::linkModules(graph_module, llvm::CloneModule(*module));
    }

    void graph_execution_context::_load_graph_input_values(
        llvm::IRBuilder<>& builder,
        node_ref_list input_nodes,
        llvm::Argument *input_array,
        value_memoize_map& values)
    {
        auto input_index = 0u;

        for (const auto &input_node : input_nodes) {
            const auto output_count = input_node.get().get_output_count();
            std::vector<llvm::Value *> input_values{output_count};

            for (auto i = 0u; i < output_count; ++i) {
                auto index_value = llvm::ConstantInt::get(_llvm_context, llvm::APInt(64, input_index));
                auto input_ptr = builder.CreateGEP(input_array, index_value);
                input_values[i] = builder.CreateLoad(input_ptr);
                input_index++;
            }

            values.emplace(&input_node.get(), input_values);
        }
    }

    void graph_execution_context::_compile_and_store_graph_output_values(
            llvm::IRBuilder<>& builder,
            node_ref_list output_nodes,
            llvm::Argument *output_array,
            llvm::Value *instance_num,
            value_memoize_map& values)
    {
        auto output_index = 0u;
        for (const auto& output_node : output_nodes) {
            const auto input_count = output_node.get().get_input_count();

            for (auto i = 0u; i < input_count; ++i) {
                unsigned int output_id = 0u;
                const auto dependency_node = output_node.get().get_input(i, output_id);
                auto value = compile_node_value(builder, dependency_node, instance_num, values, output_id);
                auto index_value = llvm::ConstantInt::get(_llvm_context, llvm::APInt(64, output_index));
                auto output_ptr = builder.CreateGEP(output_array, index_value);
                builder.CreateStore(value, output_ptr);
                output_index++;
            }
        }
    }

    void graph_execution_context::_collect_unused_states()
    {
        //  get iterator to current (last) delete sequence (map can't be empty)
        auto del_seq_it = _delete_sequences.rbegin();

        //  State management : find states that are not anymore used
        for (auto state_it = _state.begin(); state_it != _state.end();) {
            //  avoid iterator invalidation
            const auto cur_it = state_it++;

            //  if this node is not used anymore
            if (_last_compiled_nodes.find(cur_it->first) == _last_compiled_nodes.end())
            {
                //  Move the state in the delete_sequence in order to make it deleted when possible
                del_seq_it->second.add_deleted_node(std::move(cur_it->second));

                //  Remove the coresponding entry in state store
                _state.erase(cur_it);
            }
        }
    }

    graph_execution_context::state_map::iterator graph_execution_context::_get_node_mutable_state(const compile_node_class *node)
    {
        auto state_it = _state.find(node);
        if (state_it == _state.end()) {
            //  If not create state
            auto ret = _state.emplace(node, mutable_node_state(node->mutable_state_size, _instance_count, node->get_output_count()));
            assert(ret.second);
            state_it = ret.first;
        }
        return state_it;
    }

    llvm::Value* graph_execution_context::compile_node_value(
        llvm::IRBuilder<>& builder,
        const compile_node_class* node,
        llvm::Value *instance_num_value,
        value_memoize_map& values,
        unsigned int output_id)
    {
        if (node == nullptr)
            return llvm::ConstantFP::get(
                builder.getContext(),
                llvm::APFloat::getZero(llvm::APFloat::IEEEsingle()));

        auto value_it = values.find(node);

        if (value_it != values.end()) {
            //  This node have been visited : there is a cycle
            auto& value_vec = value_it->second;

            //  If the cycle was not already solved
            if (value_vec[output_id] == nullptr) {
                auto state_it = _get_node_mutable_state(node);
                auto cycle_ptr = get_cycle_state_ptr(
                            builder, state_it, instance_num_value, output_id, node->get_output_count());

                //  Store temporarily the cycle state value as output value.
                //  It will be replaced when this node will be compiled
                value_vec[output_id] = builder.CreateLoad(cycle_ptr);
            }

            return value_vec[output_id];
        }
        else {
            //  This node have not been visited, create a null filled output vector
            value_it = values.emplace_hint(value_it, node, std::vector<llvm::Value*>{node->get_output_count(), nullptr});

            //  Compile node
            compile_node(builder, node, instance_num_value, values, value_it->second);
            _last_compiled_nodes.insert(node);

            return value_it->second[output_id];
        }
    }

    void graph_execution_context::compile_node(
            llvm::IRBuilder<>& builder,
            const compile_node_class* node,
            llvm::Value *instance_num_value,
            value_memoize_map& values,
            std::vector<llvm::Value*>& output)
    {
        const auto input_count = node->get_input_count();
        std::vector<llvm::Value*> input_values(input_count);
        auto state_it = _get_node_mutable_state(node);

        assert(output.size() == node->get_output_count());

        //  Compile dependencies input
        for (auto i = 0u; i < input_count; ++i) {
            unsigned int output_id = 0u;
            const auto *input = node->get_input(i, output_id);
            input_values[i] = compile_node_value(builder, input, instance_num_value, values, output_id);
        }

        //  get state ptr
        llvm::Value *state_ptr = nullptr;

        if (node->mutable_state_size != 0u) {
            // state_ptr <- base + instance_num * state size
            state_ptr =
                get_mutable_state_ptr(
                    builder, state_it, instance_num_value);
        }
        else
        {
            state_ptr = instance_num_value;
        }

        // compile processing
        const auto output_values =
            node->emit_outputs(builder, input_values, state_ptr);

        for (auto idx = 0u; idx < output.size(); ++idx) {

            //  There was a cycle for this output
            if (output[idx] != nullptr) {
                //  Create store instruction to cycle state
                auto cycle_ptr =
                    get_cycle_state_ptr(
                        builder, state_it, instance_num_value, idx, output.size());

                builder.CreateStore(output_values[idx], cycle_ptr);
            }

            // record output value
            output[idx] = output_values[idx];
        }
    }

    void graph_execution_context::_emit_native_code(
        std::unique_ptr<llvm::Module>&& graph_module,
        llvm::Function *process_func,
        llvm::Function *initialize_func)
    {
        auto module_ptr = graph_module.get();

        //  Compile module to native code
        _execution_engine->addModule(std::move(graph_module));
        _execution_engine->finalizeObject();

        auto process_func_pointer =
            reinterpret_cast<native_process_func>(_execution_engine->getPointerToFunction(process_func));
        auto initialize_func_pointer =
            reinterpret_cast<native_initialize_func>(_execution_engine->getPointerToFunction(initialize_func));
        auto del_seq =
            delete_sequence{*_execution_engine, module_ptr};
        /*
         *      Notify process thread that a native function is ready
         */
        if (_compile_done_msg_queue.enqueue({_sequence, process_func_pointer, initialize_func_pointer})) {
            _sequence++;
            _delete_sequences.emplace(_sequence, std::move(del_seq));
            LOG_DEBUG("[graph_execution_context][compile thread] graph compilation finnished, send compile_done message to process thread (seq = %u)\n", _sequence);
        }
        else {
            LOG_ERROR("[graph_execution_context][compile thread] Cannot send compile done msg to process thread : queue is full !\n");
            LOG_ERROR("[graph_execution_context][compile thread] Is process thread running ?\n");
        }
    }

    llvm::Value *graph_execution_context::get_cycle_state_ptr(
        llvm::IRBuilder<>& builder,
        state_map::iterator state_it,
        llvm::Value *instance_num_value,
        unsigned output_id, unsigned int output_count)
    {
        return
            builder.CreateGEP(
                builder.CreateIntToPtr(
                    llvm::ConstantInt::get(
                        builder.getIntNTy(sizeof(float*) * 8),
                        reinterpret_cast<intptr_t>(state_it->second.cycle_state.data() + output_id)),
                    llvm::Type::getFloatPtrTy(_llvm_context)),
                builder.CreateMul(
                    instance_num_value,
                    llvm::ConstantInt::get(builder.getInt64Ty(), output_count)));
    }

    llvm::Value *graph_execution_context::get_mutable_state_ptr(
        llvm::IRBuilder<>& builder,
        state_map::iterator state_it,
        llvm::Value *instance_num_value)
    {
        return
            builder.CreateGEP(
                builder.CreateIntToPtr(
                    llvm::ConstantInt::get(
                        builder.getIntNTy(sizeof(uint8_t*) * 8),
                        reinterpret_cast<intptr_t>(state_it->second.data.data())),
                    builder.getInt8PtrTy()),
                builder.CreateMul(
                    instance_num_value,
                    llvm::ConstantInt::get(builder.getInt64Ty(), state_it->second.size)));
    }

    bool graph_execution_context::update_program() noexcept
    {
        compile_done_msg msg;

        //  Process one compile done msg (if any)
        //  and update native code ptr
        if (_compile_done_msg_queue.dequeue(msg)) {
            _process_compile_done_msg(msg);
            return true;
        }
        else {
            return false;
        }
    }

    void graph_execution_context::process(std::size_t instance_num, const float * inputs, float *outputs) noexcept
    {
        _process_func(instance_num, inputs, outputs);
    }

    void graph_execution_context::initialize_state(std::size_t instance_num) noexcept
    {
        _initialize_func(instance_num);
    }

    void graph_execution_context::_process_ack_msg(const ack_msg msg)
    {
        LOG_DEBUG("[graph_execution_context][compile thread] received acknowledgment from process thread (seq = %u)\n", msg);

        //  Erase all previous delete_sequence
        _delete_sequences.erase(
            _delete_sequences.begin(),
            _delete_sequences.lower_bound(msg));
    }

    void graph_execution_context::_process_compile_done_msg(const compile_done_msg msg)
    {
        LOG_DEBUG("[graph_execution_context][process thread] received compile done from compile thread (seq = %u). Send acknowledgment to compile thread\n", msg.seq);

        //  Use the new process and initialize func
        _process_func = msg.process_func;
        _initialize_func = msg.initialize_func;

        //  Send ack message to notify that old function is not anymore in use
        _ack_msg_queue.enqueue(msg.seq);
    }

}