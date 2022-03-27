#ifndef LOG_H_
#define LOG_H_

// Log levels definition

#define LOG_LEVEL_NONE    (0)
#define LOG_LEVEL_ERROR   (1)
#define LOG_LEVEL_WARNING (2)
#define LOG_LEVEL_INFO      (3)
#define LOG_LEVEL_DEBUG      (4)
#define LOG_LEVEL_ALL      (LOG_LEVEL_DEBUG)

// Default log level

#ifndef LOG_LEVEL

    #ifdef NDEBUG
        //    Release
        #define LOG_LEVEL LOG_LEVEL_INFO
    #else
        //    Debug
        #define LOG_LEVEL LOG_LEVEL_ALL
    #endif

#endif

// Generic Log function

namespace DSPJIT {

    void log_function(const char* fmt, ...);

}

/**
 *    NOCOLOR    0
 *    RED        31
 *    GREEN      32
 *    YELLOW     33
 *    BLUE       34
 *    MAGENTA    35
 *    CYAN       36
 **/
#define _BEGIN_COLOR(bold, color) "\x1B[" #bold ";" #color "m"
#define _END_COLOR()              "\033[0m"

#define ERROR_PREFIX    _BEGIN_COLOR(1, 31) "[  ERROR  ]\t" _END_COLOR()
#define WARNING_PREFIX  _BEGIN_COLOR(1, 35) "[ WARNING ]\t" _END_COLOR()
#define INFO_PREFIX     _BEGIN_COLOR(1, 32) "[  INFO   ]\t" _END_COLOR()
#define DEBUG_PREFIX    _BEGIN_COLOR(1, 34) "[  DEBUG  ]\t" _END_COLOR()

#if LOG_LEVEL >= LOG_LEVEL_ERROR
    #define LOG_ERROR(...)        DSPJIT::log_function(ERROR_PREFIX __VA_ARGS__)
#else
    #define LOG_ERROR(...)        { }
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARNING
    #define LOG_WARNING(...)    DSPJIT::log_function(WARNING_PREFIX __VA_ARGS__)
#else
    #define LOG_WARNING(...)    {  }
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
    #define LOG_INFO(...)        DSPJIT::log_function(INFO_PREFIX __VA_ARGS__)
#else
    #define LOG_INFO(...)        {  }
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(...)        DSPJIT::log_function(DEBUG_PREFIX __VA_ARGS__)
#else
    #define LOG_DEBUG(...)        { }
#endif

#endif /* LOG_H */