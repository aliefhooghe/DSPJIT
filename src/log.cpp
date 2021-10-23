
#include <stdarg.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <DSPJIT/log.h>

namespace DSPJIT {


	void log_function(const char* fmt, ...)
	{
        va_list args;
        va_start(args, fmt);

#ifdef WIN32
        char buffer[512];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        OutputDebugString(buffer);
#else
        vfprintf(stdout, fmt, args);
#endif

        va_end(args);
	}

}