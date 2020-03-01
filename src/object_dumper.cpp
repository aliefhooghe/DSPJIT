
#include <fstream>
#include "object_dumper.h"

namespace DSPJIT {

    object_dumper::object_dumper(const std::string& filename)
    : _filename{filename}
    {
    }

    void object_dumper::notifyObjectLoaded(ObjectKey, const llvm::object::ObjectFile &obj, const llvm::RuntimeDyld::LoadedObjectInfo &)
    {
        std::ofstream of(_filename);

        if (of) {
            auto buffer = obj.getMemoryBufferRef();
            of.write(buffer.getBufferStart(), buffer.getBufferSize());
        }
    }

}