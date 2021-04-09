
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <chrono>
#include <iostream>

#include "graph_execution_context.h"
#include "compile_node_class.h"
#include "ir_optimization.h"
#include "ir_helper.h"
#include "log.h"

namespace DSPJIT {

    graph_execution_context::graph_execution_context(
        llvm::LLVMContext& llvm_context,
        std::size_t instance_count,
        const opt_level level,
        const llvm::TargetOptions& options)
    :   _llvm_context{llvm_context},
        _current_sequence{0u},
        _state_manager{llvm_context, instance_count, _current_sequence},
        _ack_msg_queue{256},
        _compile_done_msg_queue{256}
    {
        std::string error_string{};
        static auto llvm_native_was_init = false;
        if (llvm_native_was_init == false) {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            LLVMLinkInMCJIT();
            llvm_native_was_init = true;
        }

        //  Initialize executionEngine
        auto memory_mgr =
            std::unique_ptr<llvm::RTDyldMemoryManager>(new llvm::SectionMemoryManager());

        _execution_engine =
            std::unique_ptr<llvm::ExecutionEngine>(
                llvm::EngineBuilder{std::make_unique<llvm::Module>("dummy", _llvm_context)}
                .setErrorStr(&error_string)
                .setEngineKind(llvm::EngineKind::JIT)
                .setTargetOptions(options)
                .setOptLevel(level)
                .setMCJITMemoryManager(std::move(memory_mgr))
                .create());

        if (!_execution_engine) {
            LOG_ERROR("[graph_execution_context] Failed to initialize execution engine : %s\n", error_string.c_str());
            throw std::runtime_error("Failed to initialize execution engine ");
        }
    }

    graph_execution_context::~graph_execution_context()
    {

    }

    void graph_execution_context::add_module(std::unique_ptr<llvm::Module>&& module)
    {
        _modules.emplace_back(std::move(module));
    }

    void graph_execution_context::compile(
            node_ref_list input_nodes,
            node_ref_list output_nodes)
    {
        auto begin = std::chrono::steady_clock::now();

        // Process acq_msg : Clean unused stuff
        ack_msg msg;
        if (_ack_msg_queue.dequeue(msg))
            _process_ack_msg(msg);

        //  Create module and link all dependencies into it
        auto module = std::make_unique<llvm::Module>("GRAPH_EXECUTION_CONTEXT", _llvm_context);
        _link_dependency_modules(*module);

        _current_sequence++;
        _state_manager.begin_sequence(_current_sequence);

        //  Compile process function
        auto process_function =
            _compile_process_function(
                input_nodes,
                output_nodes,
                *module);

        auto initialize_function =
            _state_manager.finish_sequence(*_execution_engine, *module);

#ifdef GAMMOU_PRINT_IR
        LOG_INFO("[graph_execution_context][compile thread] IR code before optimization\n");
        ir_helper::print_function(*process_function);
        ir_helper::print_function(*initialize_function);
#endif

        //  Check generated IR code
        llvm::raw_os_ostream stream{std::cout};
        if (llvm::verifyFunction(*process_function, &stream) || llvm::verifyFunction(*initialize_function, &stream)) {
            LOG_ERROR("\n[graph_execution_context][Compile Thread] Malformed IR code, canceling compilation\n");
            //  Do not compile to native code because malformed code could lead to crash
            //  Stay at last process_func.
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

        auto end = std::chrono::steady_clock::now();
        LOG_INFO("[graph_execution_context][compile thread] graph compilation finished (%u ms)\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(end - begin));
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

    void graph_execution_context::_link_dependency_modules(llvm::Module& graph_module)
    {
        for (const auto& module : _modules)
            llvm::Linker::linkModules(graph_module, llvm::CloneModule(*module));
    }

    llvm::Function * graph_execution_context::_compile_process_function(
        node_ref_list input_nodes,
        node_ref_list output_nodes,
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

        //  Create instruction builder
        llvm::IRBuilder builder(_llvm_context);
        builder.SetInsertPoint(basic_block);

        //  Get arguments
        auto arg_begin = function->arg_begin();
        auto instance_num_value = arg_begin++;
        auto inputs_array_value = arg_begin++;
        auto outputs_array_value = arg_begin++;

        //  Create graph compiler
        graph_compiler compiler{builder, instance_num_value, _state_manager};

        //  generate code that load inputs from input array and
        //  register input_nodes output as value.
        _load_graph_input_values(
            compiler, input_nodes, inputs_array_value);

        //  Compute output_nodes inputs and store them to output array
        _compile_and_store_graph_output_values(
            compiler, output_nodes, outputs_array_value, instance_num_value);

        //  Finish function by insterting a ret instruction
        builder.CreateRetVoid();
        return function;
    }

    void graph_execution_context::_load_graph_input_values(
        graph_compiler& compiler,
        node_ref_list input_nodes,
        llvm::Argument *input_array)
    {
        auto& builder = compiler.builder();
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

            compiler.assign_values(&input_node.get(), std::move(input_values));
        }
    }

    void graph_execution_context::_compile_and_store_graph_output_values(
        graph_compiler& compiler,
        node_ref_list output_nodes,
        llvm::Argument *output_array,
        llvm::Value *instance_num)
    {
        auto& builder = compiler.builder();

        auto output_index = 0u;
        for (const auto& output_node : output_nodes) {
            const auto input_count = output_node.get().get_input_count();

            for (auto i = 0u; i < input_count; ++i) {
                unsigned int output_id = 0u;
                const auto dependency_node = output_node.get().get_input(i, output_id);
                auto value = compiler.node_value(dependency_node, output_id);
                auto index_value = llvm::ConstantInt::get(_llvm_context, llvm::APInt(64, output_index));
                auto output_ptr = builder.CreateGEP(output_array, index_value);
                builder.CreateStore(value, output_ptr);
                output_index++;
            }
        }
    }

    void graph_execution_context::_emit_native_code(
        std::unique_ptr<llvm::Module>&& graph_module,
        llvm::Function *process_func,
        llvm::Function *initialize_func)
    {
        //  Compile module to native code
        _execution_engine->addModule(std::move(graph_module));
        _execution_engine->finalizeObject();

        auto process_func_pointer =
            reinterpret_cast<native_process_func>(_execution_engine->getPointerToFunction(process_func));
        auto initialize_func_pointer =
            reinterpret_cast<native_initialize_func>(_execution_engine->getPointerToFunction(initialize_func));

        /*
         *      Notify process thread that a native function is ready
         */
        if (_compile_done_msg_queue.enqueue({_current_sequence, process_func_pointer, initialize_func_pointer})) {
            LOG_DEBUG("[graph_execution_context][compile thread] Send compile_done message to process thread (seq = %u)\n", _current_sequence);
        }
        else {
            LOG_ERROR("[graph_execution_context][compile thread] Cannot send compile done msg to process thread : queue is full !\n");
            LOG_ERROR("[graph_execution_context][compile thread] Is process thread running ?\n");
        }
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

    void graph_execution_context::_process_ack_msg(const ack_msg msg)
    {
        LOG_DEBUG("[graph_execution_context][compile thread] received acknowledgment from process thread (seq = %u)\n", msg);
        _state_manager.using_sequence(msg);
    }

}