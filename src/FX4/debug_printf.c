
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "debug_printf.h"


static bool enable_debug;

void enable_debug_print(void){

	enable_debug = true;
}

void disable_debug_print(void){

	enable_debug = false;
}

void _debug_printf(char* format, ... ){

	if(!enable_debug){

		return;
	}

	va_list ap;

    // parse variable arg list
    va_start(ap, format);

    // print string
    vprintf(format, ap);

    va_end( ap );
}

void error_printf(int line, char* format, ... ){

	printf("ERROR: ");

	va_list ap;

    // parse variable arg list
    va_start(ap, format);

    // print string
    vprintf(format, ap);

    va_end( ap );

    printf(" @ line: %d\n", line);
}
