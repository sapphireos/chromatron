
#ifndef DATATYPES_H
#define DATATYPES_H

#include <stdint.h>

typedef int32_t fixed16_t;
#define F16_MAX 32767
#define F16_MIN -32767

#define TYPE_ARRAY 		0x80
#define TYPE_OBJECT     0x40
#define MASK_OBJECT     0x3F

typedef enum{
	TYPE_NONE,
	// TYPE_BOOL,
	TYPE_NUMBER,
	TYPE_FIXED16,
	TYPE_GFX16,
	TYPE_STRING,
	TYPE_FUNCTION,
	TYPE_REF_FUNCTION,
	TYPE_REF_COROUTINE,
	TYPE_OBJECT_DEF,
	TYPE_OBJ_FIELD,
	TYPE_ANY,
} data_type_t;



#endif
