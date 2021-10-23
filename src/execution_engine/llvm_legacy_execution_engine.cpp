
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/TargetSelect.h>

#include <DSPJIT/log.h>
#include <DSPJIT/llvm_legacy_execution_engine.h>

namespace DSPJIT
{
    static llvm::Triple _choose_native_target_triple()
    {
        return llvm::Triple(
#if defined(__linux__) || defined(__APPLE__)
        // Select the defaut on linux and OSX machine
        ""
#elif defined _WIN32
        // Force elf on windows, as COFF relocation seems to cause trouble
        "x86_64-pc-win32-elf"
#endif
        );
    }

    llvm_legacy_execution_engine::llvm_legacy_execution_engine(
        std::unique_ptr<llvm::ExecutionEngine>&& execution_engine)
    :   _execution_engine{std::move(execution_engine)}
    {
    }

    llvm_legacy_execution_engine::llvm_legacy_execution_engine(
        llvm::LLVMContext& llvm_context,
        llvm::CodeGenOpt::Level opt_level,
        const llvm::TargetOptions &target_options)
    {
        // Initialize LLVM native target
        static auto llvm_jit_was_init = false;
        if (llvm_jit_was_init == false) {
            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            LLVMLinkInMCJIT();
            llvm_jit_was_init = true;
        }

        // Initialize the llvm execution engine
        auto memory_mgr =
            std::unique_ptr<llvm::SectionMemoryManager>();

        llvm::EngineBuilder engine_builder
        {
            std::make_unique<llvm::Module>("graph_base", llvm_context)
        };

        std::string error_string{};
        _execution_engine =
            std::unique_ptr<llvm::ExecutionEngine>{
                engine_builder
                .setErrorStr(&error_string)
                .setEngineKind(llvm::EngineKind::JIT)
                .setTargetOptions(target_options)
                .setOptLevel(opt_level)
                .setMCJITMemoryManager(std::move(memory_mgr))
                .create(engine_builder.selectTarget(
                    _choose_native_target_triple(),
                    "" /* MArch" */,
                    "" /* MCPU */,
                    llvm::SmallVector<std::string>{}))};

        if (!_execution_engine)
            throw std::runtime_error("Failed to initialize execution engine :" + error_string);

        _execution_engine->DisableLazyCompilation();
    }

    void llvm_legacy_execution_engine::add_module(std::unique_ptr<llvm::Module> &&module)
    {
        // Set a data layout matching the execution engine
        module->setDataLayout(_execution_engine->getDataLayout());
        _execution_engine->addModule(std::move(module));
    }

    void llvm_legacy_execution_engine::delete_module(llvm::Module *module)
    {
        if (_execution_engine->removeModule(module)) {
            // removeModule does not delete the module instance
            delete module;
        }
    }

    void llvm_legacy_execution_engine::emit_native_code()
    {
        _execution_engine->finalizeObject();

        if (_execution_engine->hasError()) {
            LOG_ERROR("[llvm_legacy_execution_engine] Execution engine encountered an error while generation native code : %s\n",
                _execution_engine->getErrorMessage().c_str());
            _execution_engine->clearErrorMessage();
            throw std::runtime_error("[llvm_legacy_execution_engine] Error while generating native code");
        }
    }

    void *llvm_legacy_execution_engine::get_function_pointer(llvm::Function *function)
    {
        return _execution_engine->getPointerToFunction(function);
    }
}