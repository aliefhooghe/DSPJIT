#ifndef OBJECT_DUMPER_H_
#define OBJECT_DUMPER_H_

#include <llvm/ExecutionEngine/JITEventListener.h>

namespace DSPJIT {

    class graph_execution_context;

    class object_dumper : public llvm::JITEventListener {

    public:
        object_dumper(graph_execution_context&);

        void notifyObjectLoaded(
            ObjectKey,
            const llvm::object::ObjectFile &,
            const llvm::RuntimeDyld::LoadedObjectInfo &) override;
    private:
        graph_execution_context& _context;
    };

}

#endif