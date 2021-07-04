#include "graph_execution_context.h"
#include "object_dumper.h"
#include "log.h"

namespace DSPJIT {

    object_dumper::object_dumper(graph_execution_context& context)
    : _context{context}
    {
    }

    void object_dumper::notifyObjectLoaded(ObjectKey, const llvm::object::ObjectFile &obj, const llvm::RuntimeDyld::LoadedObjectInfo &)
    {
        auto buffer = obj.getMemoryBufferRef();
        LOG_DEBUG("[graph_execution_context] [obj dumper] Loaded native code object : size = %llu\n", buffer.getBufferSize());
        _context._last_native_code_object_data = reinterpret_cast<const uint8_t*>(buffer.getBufferStart());
        _context._last_native_code_object_size = buffer.getBufferSize();
    }

}