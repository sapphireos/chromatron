#ifndef _DEBUG_PRINTF_H
#define _DEBUG_PRINTF_H

void _debug_printf(char* format, ...);

#ifdef FX4_COMPILER
	#define debug_printf _debug_printf
	// #define debug_printf(...)
#else
	#define debug_printf(...)
	// #define debug_printf trace_printf
#endif

void error_printf(int line, char* format, ... );

void enable_debug_print(void);
void disable_debug_print(void);

#endif