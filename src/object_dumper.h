#ifndef OBJECT_DUMPER_H_
#define OBJECT_DUMPER_H_

#include <llvm/ExecutionEngine/JITEventListener.h>

namespace ProcessGraph {

    class object_dumper : public llvm::JITEventListener {

    public:
        object_dumper(const std::string& filename);

        void notifyObjectLoaded(
            ObjectKey,
            const llvm::object::ObjectFile &,
            const llvm::RuntimeDyld::LoadedObjectInfo &) override;
    private:
        const std::string _filename;
    };

}

#endif