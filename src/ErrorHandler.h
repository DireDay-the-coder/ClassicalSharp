#ifndef CC_ERRORHANDLER_H
#define CC_ERRORHANDLER_H
#include "String.h"
/* Support methods for checking and handling errors.
   NOTE: Methods here use raw characters pointers, DO NOT apply this style elsewhere.
   Copyright 2017 ClassicalSharp | Licensed under BSD-3
*/

void ErrorHandler_Init(void);
void ErrorHandler_Log(const String* msg);
void ErrorHandler_Fail(const char* raw_msg);
CC_NOINLINE void ErrorHandler_Fail2(ReturnCode result, const char* raw_msg);
#endif
