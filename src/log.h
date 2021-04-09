#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>

/**
 *    NOCOLOR    0
 *    RED        31
 *    GREEN      32
 *    YELLOW     33
 *    BLUE       34
 *    MAGENTA    35
 *    CYAN       36
 **/

#define _LOG_FD (stderr)
#define _BEGIN_COLOR(bold, color) "\033[" #bold ";" #color "m"
#define _END_COLOR "\033[0m"


#define LOG_ERROR(...)      { fprintf(_LOG_FD, _BEGIN_COLOR(1, 31) "[  ERROR  ]\t" _END_COLOR __VA_ARGS__); }
#define LOG_WARNING(...)    { fprintf(_LOG_FD, _BEGIN_COLOR(1, 35) "[ WARNING ]\t" _END_COLOR __VA_ARGS__); }
#define LOG_INFO(...)       { fprintf(_LOG_FD, _BEGIN_COLOR(1, 32) "[  INFO   ]\t" _END_COLOR __VA_ARGS__); }

#if !defined(NDEBUG) || defined(ENABLE_DEBUG_LOG)
#define LOG_DEBUG(...)      { fprintf(_LOG_FD, _BEGIN_COLOR(1, 34) "[  DEBUG  ]\t" _END_COLOR __VA_ARGS__); }
#else
#define LOG_DEBUG(...)      { (void)(__VA_ARGS__); }
#endif

#endif