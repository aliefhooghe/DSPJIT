#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>

#define LOG_RED 31
#define LOG_GREEN 32
#define LOG_YELLOW 33
#define LOG_BLUE 34
#define LOG_MAGENTA 35
#define LOG_CYAN 36

namespace logger {

    enum class color : unsigned int {
        normal = 0,
        red = 31u,
        green,
        yellow,
        blue,
        magenta,
        cyan
    };

    template <typename ...Targs>
    void print(FILE *stream, const color c, const bool bold, const char *fmt, const Targs ...args)
    {
        if (c == color::normal) {
            fprintf(stream, fmt, args...);
        }
        else {
            fprintf(stream, "\033[%u;%um",
                static_cast<unsigned int>(bold),
                static_cast<unsigned int>(c));
            fprintf(stream, fmt, args...);
            fprintf(stream, "\033[0m");
        }

        fprintf(stream, "\n");
    }

}


#define LOG_ERROR(...) { logger::print(stderr, logger::color::red, true, __VA_ARGS__); }
#define LOG_WARNING(...) { logger::print(stderr, logger::color::magenta, true, __VA_ARGS__); }
#define LOG_INFO(...) { logger::print(stderr, logger::color::normal, false, __VA_ARGS__); }

#ifndef NDEBUG
#define LOG_DEBUG(...) { logger::print(stderr, logger::color::blue, false, __VA_ARGS__); }
#else
#define LOG_DEBUG(...) {  }
#endif

#endif