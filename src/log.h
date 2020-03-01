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


#define _BEGIN_COLOR(bold, color) "\033[" #bold ";" #color "m"
#define _END_COLOR "\033[0m"


#define LOG_ERROR(...)      { fprintf(stdout, _BEGIN_COLOR(1, 31) "[  ERROR  ]\t" _END_COLOR __VA_ARGS__); fprintf(stdout, "\n"); }
#define LOG_WARNING(...)    { fprintf(stdout, _BEGIN_COLOR(1, 35) "[ WARNING ]\t" _END_COLOR __VA_ARGS__); fprintf(stdout, "\n"); }
#define LOG_INFO(...)       { fprintf(stdout, _BEGIN_COLOR(1, 1) "[  INFO   ]\t" _END_COLOR __VA_ARGS__);  fprintf(stdout, "\n"); }

#ifndef NDEBUG
#define LOG_DEBUG(...)      { fprintf(stdout, _BEGIN_COLOR(1, 32) "[  DEBUG  ]\t" _END_COLOR __VA_ARGS__); fprintf(stdout, "\n"); }
#else
#define LOG_DEBUG(...) {  }
#endif

#endif