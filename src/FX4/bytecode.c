
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "syslib.h"
#include "hash32.h"
#include "bytecode.h"
#include "pixelarray.h"
#include "debug_printf.h"

#ifdef FX4_COMPILER
#include <assert.h>

#define ASSERT assert

#include <stdbool.h>
#else
#include "sapphire.h"
#include "catbus.h"
#include "config.h"
#include "kvdb.h"
#include "vm4.h"
#endif

#if defined(ENABLE_GFX) || defined(FX4_COMPILER)

/*


Basic performance notes:

Running basic rainbow.fx/fx4

300 pixels

VM4:
peak cycles: 7221
ready time:  32
run time:    7901

us / cycle:  1.09

VM:
peak cycles: 4211
run time:    3160

us / cycle:  0.75



fader time around 2 ms (VM independent)


Pixel vector ops:


VM4:
peak cycles: 6024
ready time:  32
run time:    6990

us / cycle:  1.16



VM:
peak cycles: 4211
run time:    3160

us / cycle:  0.75





Some of the above VM4 may be slightly off because the VM may have been compiled for
debug dispatch.


push const 2:
with debug dispatch:

VM4:
peak cycles: 5121
ready time:  32
run time:    6387

us / cycle:  1.24


with fast dispatch:

VM4:
peak cycles: 5121
ready time:  32
run time:    6663

us / cycle:  1.30



pixel load opcodes:
(fast dispatch)

VM4:
peak cycles: 5117
run time:    4846

us / cycle:  0.95



pixarray fast deref

VM4:
peak cycles: 5117
run time:    2876

us / cycle:  0.56


accumulator

VM4:
peak cycles: 5118
run time:    2932

us / cycle:  0.57





*/

#define MAX_CYCLES 60000


int do_array_size(vm_t *vm, int32_t index, uint8_t rank){

    // int32_t *stack         = (int32_t *)array_get_data(vm->stack_list);
    int32_t *stack        = vm->stack;
    int32_t *globals      = (int32_t *)array_get_data(vm->globals_list);
    int32_t *array        = (int32_t *)array_get_data(vm->array_list);

    uint32_t zero = 0;

    array_meta_t *meta = (array_meta_t *)&globals[index];
    memset(meta, 0, sizeof(array_meta_t));

    meta->array_index = array_get_count(vm->array_list);

    // add to array list
    array_add(&vm->array_list, &zero);

    array_meta2_t *meta2 = (array_meta2_t *)&array[meta->array_index];
    memset(meta2, 0, sizeof(array_meta2_t));

    int size = 1;

    for(int i = 0; i < rank; i++){

        int32_t dimension = stack[--vm->sp];
        meta2->strides[i] = size;

        debug_printf("rank: %d dim: %d stride: %d\n", i, dimension, size);

        size *= dimension;
    }

    debug_printf("ARRAY_SIZE: %d rank %d\n", size, rank);


    meta->size = size;

    // allocate space
    vm->array_list = array_grow(vm->array_list, array_get_used_space(vm->array_list) + size);

    if(vm->array_list <= 0){

        return -1;
    }

    
    while(size > 0){

        array_add(&vm->array_list, &zero);

        size--;
    }

    return 0;
}

// int do_object_size(vm_t *vm, int32_t index, int32_t size){

//     uint32_t zero = 0;

//     // allocate space
//     while(size > 0){

//         array_add(&vm->object_list, &zero);

//         size--;
//     }

//     return 0;
// }

coroutine_handle_t _coroutine_alloc(uint16_t locals_count){

    uint16_t mem_size = sizeof(coroutine_state_t) + locals_count * sizeof(int32_t);

    #ifdef FX4_COMPILER
    void *ptr = malloc(mem_size);

    if(ptr == NULL){

        return 0;    
    }

    // zero init
    memset(ptr, 0, mem_size);

    return (coroutine_handle_t)ptr;
    #else
    coroutine_handle_t h = mem2_h_alloc2(mem_size, MEM_TYPE_VM_COROUTINE);

    if(h > 0){

        memset(mem2_vp_get_ptr_fast(h), 0, mem_size);
    }

    return h;
    #endif
}

void _coroutine_free(coroutine_handle_t handle){

    #ifdef FX4_COMPILER
    free((void *)handle);
    #else

    mem2_v_free(handle);
    #endif
}

int vm_run_instructions(vm_t *vm, int coroutine_index, uint32_t call_func_meta){

    pixelarray_deref();

    debug_printf("Start VM\n");

    #ifndef FX4_COMPILER
    // log_v_debug_P(PSTR("VM run: %d"), coroutine_index);
    #endif

    static void *const opcode_table[] = {
        &&opcode_trap,
        &&opcode_push_const,
        &&opcode_add,
        &&opcode_sub,
        &&opcode_mul,
        &&opcode_div,
        &&opcode_print,
        &&opcode_halt,
        &&opcode_store_global,
        &&opcode_load_global,
        &&opcode_array_size,
        &&opcode_trap,
        &&opcode_store_array,
        &&opcode_load_array,
        &&opcode_call_native,
        &&opcode_call,
        &&opcode_return,
        &&opcode_func,
        &&opcode_end_func,
        &&opcode_jump,
        &&opcode_jumpz,
        &&opcode_discard,
        &&opcode_load_return,
        &&opcode_return_void,
        &&opcode_load_obj,
        &&opcode_store_obj,
        &&opcode_load_obj_n,
        &&opcode_store_obj_n,
        &&opcode_complt,
        &&opcode_jumpnz,
        &&opcode_conv0_i32_to_f16,
        &&opcode_conv1_i32_to_f16,
        &&opcode_conv0_f16_to_i32,
        &&opcode_conv1_f16_to_i32,
        &&opcode_complte,
        &&opcode_loop,
        &&opcode_trap,
        &&opcode_compgt,
        &&opcode_compgte,
        &&opcode_icall,
        &&opcode_obj_size,
        &&opcode_load_local,
        &&opcode_store_local,
        &&opcode_coro_init,
        &&opcode_coro_start,
        &&opcode_coro_stop,
        &&opcode_zero_local,
        &&opcode_compeq,
        &&opcode_compneq,
        &&opcode_assert,
        &&opcode_trap,
        &&opcode_coro_exit,
        &&opcode_yield,
        &&opcode_delay,
        &&opcode_add_assign,
        &&opcode_mod,
        &&opcode_add_assign_local,
        &&opcode_sub_assign,
        &&opcode_sub_assign_local,
        &&opcode_store_vector,
        &&opcode_assign_op_array,
        &&opcode_assign_op_vector,
        &&opcode_mul_f16,
        &&opcode_vstore_hue,
        &&opcode_pstore_hue,
        &&opcode_assign_op_obj,
        &&opcode_assign_op_obj_n,
        &&opcode_rate,
        &&opcode_div_f16,
        &&opcode_pstore_sat,
        &&opcode_pstore_val,
        &&opcode_pstore_hsfade,
        &&opcode_pstore_vfade,
        &&opcode_vstore_sat,
        &&opcode_vstore_val,
        &&opcode_vstore_hsfade,
        &&opcode_vstore_vfade,
        &&opcode_pixel_vector_op_hue,
        &&opcode_pixel_op_hue,
        &&opcode_push_const2,
        &&opcode_pixel_vector_load,
        &&opcode_pixel_load,
        &&opcode_trap,
        &&opcode_swap,
        &&opcode_move_vector,
        &&opcode_move_obj,
        &&opcode_load_ref,
        &&opcode_pstore_hue2,
        &&opcode_pstore_sat2,
        &&opcode_pstore_val2,
        &&opcode_pstore_hsfade2,
        &&opcode_pstore_vfade2,
        &&opcode_pixel_load2,
        &&opcode_pixel_vector_op_sat,
        &&opcode_pixel_op_sat,
        &&opcode_pixel_vector_op_val,
        &&opcode_pixel_op_val,
        &&opcode_pixel_op_hue2,
        &&opcode_pixel_op_sat2,
        &&opcode_pixel_op_val2,
        &&opcode_publish,
        &&opcode_mul_assign,
        &&opcode_mul_assign_local,
        &&opcode_div_assign,
        &&opcode_div_assign_local,
        &&opcode_mod_assign,
        &&opcode_mod_assign_local,
        &&opcode_logical_and,
        &&opcode_logical_or,
    };

    uint32_t cycles = MAX_CYCLES;
    uint8_t opcode = 0;

    int32_t arg1 = 0;
    int32_t arg2 = 0;
    int32_t *ptr = 0;
    int32_t arg_count = 0;
    int32_t call_args[MAX_FUNC_ARGS] = {0};
    uint8_t rank = 0;
    uint8_t obj_rank = 0;
    uint8_t field_rank = 0;
    int32_t index = 0;
    int32_t index2 = 0;
    int32_t size = 0;
    int32_t offset = 0;
    int32_t val = 0;
    uint8_t op = 0;
    uint8_t type = 0;
    uint32_t count = 0;
    uint32_t obj_hash = 0;
    uint32_t name_hash = 0;
    uint32_t field_hash = 0;
    uint32_t func_hash = 0;
    array_meta_t *array_meta;
    array_meta2_t *array_meta2;
    func_meta_t *func_meta;
    uint16_t rank_indexes[MAX_RANKS];
    uint16_t obj_indexes[MAX_RANKS];
    uint16_t field_indexes[MAX_RANKS];

    #ifdef FX4_COMPILER
    uint8_t max_stack_depth = 0;
    #endif

    uint8_t *instructions = (uint8_t *)array_get_data(vm->instruction_list);
    int32_t *stack        = vm->stack;
    int32_t *globals      = (int32_t *)array_get_data(vm->globals_list);
    int32_t *func_locals  = (int32_t *)array_get_data(vm->locals_list);
    int32_t *array        = (int32_t *)array_get_data(vm->array_list);
    int32_t *constants    = (int32_t *)array_get_data(vm->constants_list);

    jump_target_t *jump_targets = (jump_target_t *)array_get_data(vm->jump_targets);

    coroutine_state_t *coroutine = 0;
    int32_t *locals = 0;

    if(coroutine_index < 0){

        if(call_func_meta == 0){

            vm->pc = 0;
        }
        else{

            // running a specific function
            // this is a special case since we are faking a call

            func_meta = (func_meta_t *)&call_func_meta;

            // printf("%d %d\n", func_meta->label_index, func_meta->locals_index);

            // set return address to last instruction (which should always be HALT)
            vm->call_stack[vm->call_depth].return_addr  = array_get_count(vm->instruction_list) - 1;

            // set current locals
            vm->call_stack[vm->call_depth].locals = locals;

            // set call depth
            vm->call_depth++;

            // set new local index
            // locals = &func_locals[func_meta->locals_index];

            // set function PC:
            vm->pc = jump_targets[func_meta->label_index].instruction_offset;
        }
    
        locals = func_locals;
    }
    else if(coroutine_index >= cnt_of_array(vm->coroutines)){

        return VM4_STATUS_BAD_COROUTINE_INDEX;
    }
    else{ // run coroutine

        // check if a call_func was given, we cannot jump to a func and run 
        // a coroutine at the same time
        if( call_func_meta != 0 ){

            return VM4_STATUS_BAD_CF_META;
        }

        #ifdef FX4_COMPILER
        coroutine = (coroutine_state_t *)vm->coroutines[coroutine_index];
        #else
        coroutine = (coroutine_state_t *)mem2_vp_get_ptr(vm->coroutines[coroutine_index]);
        #endif

        if(coroutine == 0){

            return VM4_STATUS_BAD_COROUTINE_PTR;
        }

        int32_t *coroutine_locals = (int32_t *)(coroutine + 1);

        vm->pc = coroutine->current_pc;
        locals = coroutine_locals;
    }

    if(vm->sp > 0){

        debug_printf("*** VM started with non-empty stack! %d items ***\n", vm->sp);
    }

    int status = 0;

    #ifdef FX4_COMPILER
    #define DISPATCH cycles--; \
                     if( cycles == 0 ){ \
                        return VM4_STATUS_CYCLE_LIMIT; \
                    } \
                    print_vm_state(vm);\
                    opcode = instructions[vm->pc++]; \
                    if(vm->sp >= VM4_STACK_SIZE){ \
                        debug_printf("*** STACK OVER OR UNDERFLOW ***\n"); \
                        status = VM4_STATUS_STACK_OVER_UNDER; \
                    } \
                    else if(vm->sp > max_stack_depth){ \
                        max_stack_depth = vm->sp; \
                    } \
                    debug_printf("\n----------------------\n"); \
                    debug_printf("Cycle: %d\n", MAX_CYCLES - cycles); \
                    goto *opcode_table[opcode]
    #else
    // fast dispatch:
    #define DISPATCH cycles--; \
                     if( cycles == 0 ){ \
                        return VM4_STATUS_CYCLE_LIMIT; \
                    } \
                    opcode = instructions[vm->pc++]; \
                    goto *opcode_table[opcode]
    #endif

    DISPATCH;

opcode_push_const:
    arg1 = instructions[vm->pc++];
    stack[vm->sp++] = constants[arg1];

    #ifdef FX4_COMPILER
    if(hash_s_get_str(constants[arg1]) == NULL){

        debug_printf("PUSH_CONST: %d\n", constants[arg1]);
    }
    else{

        debug_printf("PUSH_CONST: %s (%d)\n", hash_s_get_str(constants[arg1]), constants[arg1]);
    }

    #else
    debug_printf("PUSH_CONST: %d\n", constants[arg1]);
    #endif

    DISPATCH;

opcode_push_const2:
    arg1 = instructions[vm->pc++];
    arg2 = instructions[vm->pc++];
    
    stack[vm->sp++] = constants[arg1];
    stack[vm->sp++] = constants[arg2];
    
    debug_printf("PUSH_CONST2: %d %d\n", constants[arg1], constants[arg2]);
    DISPATCH;


opcode_swap:
    debug_printf("SWAP\n");

    val = stack[vm->sp - 1];
    stack[vm->sp - 1] = stack[vm->sp - 2];
    stack[vm->sp - 2] = val;
    
    DISPATCH;

opcode_discard:
    if(vm->sp > 0){

        vm->sp--;    
    }
    
    debug_printf("DISCARD\n");
    DISPATCH;

opcode_store_global:
    arg1 = stack[--vm->sp];
    arg2 = stack[--vm->sp];

    globals[arg2] = arg1;

    debug_printf("STORE_GLOBAL: [%d] = %d\n", arg2, arg1);
    DISPATCH;

opcode_load_global:
    arg1 = instructions[vm->pc++];

    stack[vm->sp++] = globals[arg1];

    debug_printf("LOAD_GLOBAL: %d\n", arg1);
    DISPATCH;

opcode_store_local:
    arg1 = stack[--vm->sp];
    arg2 = stack[--vm->sp];

    locals[arg2] = arg1;

    debug_printf("STORE_LOCAL: [%d] = %d\n", arg2, arg1);
    DISPATCH;

opcode_load_local:
    arg1 = instructions[vm->pc++];

    stack[vm->sp++] = locals[arg1];

    debug_printf("LOAD_LOCAL: %d\n", arg1);
    DISPATCH;

opcode_store_array:
    val = stack[--vm->sp];
    index = stack[--vm->sp];
    rank = stack[--vm->sp];

    debug_printf("STORE_ARRAY: rank: %d val %d index %d\n", rank, val, index);

    for(int i = 0; i < rank; i++){

        arg1 = stack[--vm->sp];
        rank_indexes[i] = arg1;
        debug_printf("%d ", arg1);
    }

    array_meta  = (array_meta_t *)&globals[index];
    array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
    debug_printf("\tstride: ");
    for(int i = 0; i < rank; i++){

        debug_printf("%d ", array_meta2->strides[i]);
    }
    debug_printf("\n");
    debug_printf("\tmeta->array_index: %d\n", array_meta->array_index);

    debug_printf("\toffset: ");
    offset = 0;

    for(int i = 0; i < rank; i++){

        offset += (array_meta2->strides[i] * rank_indexes[i]);

        debug_printf("%d ", offset);
    }
    debug_printf("\n");

    offset %= array_meta->size;

    index2 = array_meta->array_index + (sizeof(array_meta2_t) / sizeof(uint32_t)) + offset;

    array[index2] = val;

   DISPATCH;

opcode_load_array:
    index = stack[--vm->sp];
    rank = stack[--vm->sp];

    for(int i = 0; i < rank; i++){

        rank_indexes[i] = stack[--vm->sp];
    }

    debug_printf("LOAD_ARRAY: rank: %d index: %d\n", rank, index);

    for(int i = 0; i < rank; i++){

        debug_printf("%d ", rank_indexes[i]);
    }

    array_meta = (array_meta_t *)&globals[index];
    array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
    debug_printf("\n\tstride: ");
    for(int i = 0; i < rank; i++){

        debug_printf("%d ", array_meta2->strides[i]);
    }
    debug_printf("\n");
    debug_printf("\tmeta->array_index: %d\n", array_meta->array_index);

    debug_printf("\toffset: ");
    offset = 0;

    for(int i = 0; i < rank; i++){

        offset += (array_meta2->strides[i] * rank_indexes[i]);

        debug_printf("%d ", offset);
    }
    debug_printf("\n");

    offset %= array_meta->size;

    index2 = offset + array_meta->array_index + (sizeof(array_meta2_t) / sizeof(uint32_t));

    val = array[index2];
    debug_printf("val: %d\n", val);

    stack[vm->sp++] = val;

    DISPATCH;

opcode_store_vector:
    val = stack[--vm->sp];
    index = stack[--vm->sp];

    debug_printf("STORE_VECTOR: val %d index %d\n", val, index);

    array_meta  = (array_meta_t *)&globals[index];
    // array_meta2 = (array_meta2_t *)&array[array_meta->array_index];

    ptr = &array[array_meta->array_index + (sizeof(array_meta2_t) / sizeof(uint32_t))];

    for(int i = 0; i < array_meta->size; i++){

        *ptr++ = val;
    }

   DISPATCH;

opcode_move_vector:
    size = instructions[vm->pc++];
    index = stack[vm->sp - 1 - size];

    debug_printf("MOVE_VECTOR: size %d index %d\n", size, index);

    array_meta  = (array_meta_t *)&globals[index];

    if(size > array_meta->size){

        size = array_meta->size;
    }

    ptr = &array[array_meta->array_index + (sizeof(array_meta2_t) / sizeof(uint32_t))] + size - 1;

    for(int i = 0; i < size; i++){

        *ptr-- = stack[--vm->sp];
    }

    --vm->sp; // pop index, final value

    DISPATCH;

opcode_move_obj:
    arg1 = stack[--vm->sp];
    arg2 = stack[--vm->sp];

    debug_printf("MOVE_OBJ: %s = %s\n", hash_s_get_str(arg2), hash_s_get_str(arg1));

    if(syslib_move_obj(arg2, arg1) < 0){

        status = VM4_STATUS_ERR_MOVE_OBJ;
        goto end;
    }

    DISPATCH;

opcode_load_ref:
    
    // arg1 = stack[--vm->sp];
    // debug_printf("LOAD_REF: %d\n", arg1);
    debug_printf("LOAD_REF: %d\n", stack[vm->sp - 1]);

    DISPATCH;

opcode_array_size:
    index = stack[--vm->sp]; // handle index in globals
    rank = stack[--vm->sp]; // rank

    debug_printf("ARRAY_SIZE\n");

    do_array_size(vm, index, rank);

    // must re-initialize array pointer
    // since array size may allocate
    array = (int32_t *)array_get_data(vm->array_list);

    DISPATCH;

opcode_obj_size:
    obj_hash = stack[--vm->sp];
    name_hash = stack[--vm->sp];
    
    debug_printf("OBJ_SIZE: %s -> %s\n", hash_s_get_str(obj_hash), hash_s_get_str(name_hash));

    if(syslib_create_obj(obj_hash, name_hash, vm->vm_id) < 0){

    	status = VM4_STATUS_ERR_OBJ_SIZE;

    	goto end;
    }

    DISPATCH;

opcode_publish:
    
    name_hash = stack[--vm->sp];
    index = instructions[vm->pc++];
    type = instructions[vm->pc++];

    // if array:
    if(type & TYPE_ARRAY){

        type &= ~TYPE_ARRAY;

        array_meta = (array_meta_t *)&globals[index];

        count = array_meta->size;
        index = array_meta->array_index;
    }
    else{

        count = 1;
    }

    #ifndef FX4_COMPILER
    // convert FX4 type to catbus type
    if(type == TYPE_NUMBER){

        type = CATBUS_TYPE_INT32;
    }
    else if(type == TYPE_FIXED16){

        type = CATBUS_TYPE_FIXED16;
    }
    else{

        debug_printf("Unhandled type %d for published var: %s\n", type, hash_s_get_str(name_hash));

        return VM4_STATUS_ERR_PUBLISH;
    }

    vm4_v_add_published_var(index, name_hash, type, count, 0, vm->vm_id);
    #endif

    debug_printf("PUBLISH: %s addr: %d type: %d count: %d\n", hash_s_get_str(name_hash), index, type, count);

    DISPATCH;

opcode_add:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    val = arg1 + arg2;
    stack[vm->sp++] = val;
    debug_printf("ADD: %d + %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_sub:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 - arg2;
    stack[vm->sp++] = val;
    debug_printf("SUB: %d - %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_mul:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 * arg2;
    stack[vm->sp++] = val;
    debug_printf("MUL: %d * %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_mul_f16:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = ((int64_t)arg1 * (int64_t)arg2) / 65536;
    stack[vm->sp++] = val;
    debug_printf("MUL_F16: %d * %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_div_f16:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    if(arg2 == 0){

        val = 0;
    }
    else{

        val = (((int64_t)arg1 * 65536) / (int64_t)arg2);
    }

    stack[vm->sp++] = val;
    debug_printf("DIV_F16: %d / %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_div:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    if(arg2 == 0){

        val = 0;
    }
    else{
        val = arg1 / arg2;    
    }
    
    stack[vm->sp++] = val;
    debug_printf("DIV: %d / %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_mod:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    if(arg2 == 0){

        val = 0;
    }
    else{
        
        val = arg1 % arg2;
    }

    stack[vm->sp++] = val;
    debug_printf("MOD: %d %% %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_compeq:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 == arg2;
    stack[vm->sp++] = val;
    debug_printf("COMPEQ: %d == %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_compneq:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 != arg2;
    stack[vm->sp++] = val;
    debug_printf("COMPNEQ: %d != %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_complt:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 < arg2;
    stack[vm->sp++] = val;
    debug_printf("COMPLT: %d < %d -> %d\n", arg1, arg2, val);
    DISPATCH;

 opcode_complte:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 <= arg2;
    stack[vm->sp++] = val;
    debug_printf("COMPLTE: %d <= %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_compgt:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 > arg2;
    stack[vm->sp++] = val;
    debug_printf("COMPGT: %d > %d -> %d\n", arg1, arg2, val);
    DISPATCH;

 opcode_compgte:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 >= arg2;
    stack[vm->sp++] = val;
    debug_printf("COMPGTE: %d >= %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_logical_and:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 && arg2;
    stack[vm->sp++] = val;
    debug_printf("AND: %d && %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_logical_or:
    arg2 = stack[--vm->sp];
    arg1 = stack[--vm->sp];

    val = arg1 || arg2;
    stack[vm->sp++] = val;
    debug_printf("OR: %d || %d -> %d\n", arg1, arg2, val);
    DISPATCH;

opcode_conv0_i32_to_f16:

    // convert top of stack
    arg1 = stack[vm->sp - 1];

    val = arg1 << 16;

    stack[vm->sp - 1] = val;

    debug_printf("CONV0: i32 to f16: %d -> %d\n", arg1, val);

    DISPATCH;

opcode_conv1_i32_to_f16:

    // convert top of stack -1 (which is actually at -2 since TOS is at -1)
    arg1 = stack[vm->sp - 2];

    val = arg1 << 16;

    stack[vm->sp - 2] = val;

    debug_printf("CONV1: i32 to f16: %d -> %d\n", arg1, val);


    DISPATCH;

opcode_conv0_f16_to_i32:

    // convert top of stack
    arg1 = stack[vm->sp - 1];

    val = arg1 >> 16;

    stack[vm->sp - 1] = val;

    debug_printf("CONV0: f16 to i32: %d -> %d\n", arg1, val);

    DISPATCH;

opcode_conv1_f16_to_i32:

    // convert top of stack -1 (which is actually at -2 since TOS is at -1)
    arg1 = stack[vm->sp - 2];

    val = arg1 >> 16;

    stack[vm->sp - 2] = val;

    debug_printf("CONV1: f16 to i32: %d -> %d\n", arg1, val);


    DISPATCH;

opcode_assign_op_array:
    op = instructions[vm->pc++];
    val = stack[--vm->sp];
    index = stack[--vm->sp];
    rank = stack[--vm->sp];

    array_meta = (array_meta_t *)&globals[index];
    array_meta2 = (array_meta2_t *)&array[array_meta->array_index];

    for(int i = 0; i < rank; i++){

        rank_indexes[i] = stack[--vm->sp];
    }

    offset = array_meta->array_index + sizeof(array_meta2_t) / sizeof(uint32_t);
    for(int i = 0; i < rank; i++){

        offset += (array_meta2->strides[i] * rank_indexes[i]);
    }

    debug_printf("ASSIGN_OP: array[%d] -> %d\n", offset, val);

    if(op == ASSIGN_OP_ADD){

        array[offset] += val;
    }
    else if(op == ASSIGN_OP_SUB){

        array[offset] -= val;
    }
    else{

        status = VM4_STATUS_ERR_ASSIGN_OP_ARRAY;
        goto end;
    }

    DISPATCH;

opcode_assign_op_vector:
    op = instructions[vm->pc++];
    val = stack[--vm->sp]; // right side
    index = stack[--vm->sp]; // left side

    array_meta  = (array_meta_t *)&globals[index];

    ptr = &array[array_meta->array_index + (sizeof(array_meta2_t) / sizeof(uint32_t))];

    debug_printf("ASSIGN_OP (vector): array[%d] -> %d\n", index, val);

    if(op == ASSIGN_OP_ADD){

        for(int i = 0; i < array_meta->size; i++){

            *ptr++ += val;
        }
    }
    else if(op == ASSIGN_OP_SUB){

        for(int i = 0; i < array_meta->size; i++){

            *ptr++ -= val;
        }
    }
    else{

        status = VM4_STATUS_ERR_ASSIGN_OP_VECTOR;
        goto end;
    }

    DISPATCH;

opcode_add_assign:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    globals[arg1] += arg2;

    debug_printf("ADD_ASSIGN: globals[%d] += %d\n", arg1, arg2);
    DISPATCH;

opcode_add_assign_local:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    locals[arg1] += arg2;

    debug_printf("ADD_ASSIGN: locals[%d] += %d\n", arg1, arg2);
    DISPATCH;

opcode_sub_assign:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    globals[arg1] -= arg2;

    debug_printf("SUB_ASSIGN: globals[%d] -= %d\n", arg1, arg2);
    DISPATCH;

opcode_sub_assign_local:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    locals[arg1] -= arg2;

    debug_printf("SUB_ASSIGN: locals[%d] -= %d\n", arg1, arg2);
    DISPATCH;

opcode_mul_assign:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    globals[arg1] *= arg2;

    debug_printf("MUL_ASSIGN: globals[%d] *= %d\n", arg1, arg2);
    DISPATCH;

opcode_mul_assign_local:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    locals[arg1] *= arg2;

    debug_printf("MUL_ASSIGN: locals[%d] *= %d\n", arg1, arg2);
    DISPATCH;

opcode_div_assign:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    globals[arg1] /= arg2;

    debug_printf("DIV_ASSIGN: globals[%d] /= %d\n", arg1, arg2);
    DISPATCH;

opcode_div_assign_local:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    locals[arg1] /= arg2;

    debug_printf("DIV_ASSIGN: locals[%d] /= %d\n", arg1, arg2);
    DISPATCH;

opcode_mod_assign:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    globals[arg1] %= arg2;

    debug_printf("MOD_ASSIGN: globals[%d] %%= %d\n", arg1, arg2);
    DISPATCH;

opcode_mod_assign_local:
    arg2 = stack[--vm->sp]; // right side
    arg1 = stack[--vm->sp]; // left side

    locals[arg1] %= arg2;

    debug_printf("MOD_ASSIGN: locals[%d] %%= %d\n", arg1, arg2);
    DISPATCH;

opcode_call_native:
    arg_count = instructions[vm->pc++];
    func_hash = stack[--vm->sp];

    for(int i = 0; i < arg_count; i++){

        call_args[(arg_count - 1) - i] = stack[--vm->sp];
    }

    debug_printf("CALL NATIVE: %s\n", hash_s_get_str(func_hash));

    if(syslib_call_func(vm, func_hash, arg_count, call_args, &vm->return_value) < 0){

        debug_printf("Error\n");
    }

    DISPATCH;

opcode_call:
    debug_printf("CALL\n");

    arg_count = instructions[vm->pc++];
    index = instructions[vm->pc++];
    offset = instructions[vm->pc++];

    debug_printf("offset %d args %d index: %d\n", offset, arg_count, index);

    // set up call stack
    if(vm->call_depth >= MAX_CALL_DEPTH){

        debug_printf("Call depth exceeded!\n");
        return VM4_STATUS_ERR_CALL_DEPTH;
    }

    // save current state
    vm->call_stack[vm->call_depth].return_addr  = vm->pc;
    vm->call_stack[vm->call_depth].locals = locals;

    // set new local index
    locals = &func_locals[index];

    // pop args from stack and store in function locals
    for(int i = 0; i < arg_count; i++){

        // reverse order of args
        locals[(arg_count - 1) - i] = stack[--vm->sp];
    }

    debug_printf("call to %d return to: %d\n", jump_targets[offset].instruction_offset, vm->call_stack[vm->call_depth].return_addr);

    // execute jump to function
    vm->call_depth++;
    vm->pc = jump_targets[offset].instruction_offset;

    DISPATCH;

opcode_icall:
    debug_printf("ICALL\n");

    arg_count = instructions[vm->pc++];
    index = instructions[vm->pc++]; // global var index

    // load value from global var
    func_meta = (func_meta_t *)&globals[index];
    offset = func_meta->label_index;

    debug_printf("offset %d args %d index: %d\n", offset, arg_count, index);

    // set up call stack
    if(vm->call_depth >= MAX_CALL_DEPTH){

        debug_printf("Call depth exceeded!\n");
        return VM4_STATUS_ERR_ICALL_DEPTH;
    }

    vm->call_stack[vm->call_depth].return_addr = vm->pc;
    vm->call_stack[vm->call_depth].locals = locals;

    // set new local index
    locals = &func_locals[index];

        // pop args from stack and store in function locals
    for(int i = 0; i < arg_count; i++){

        locals[(arg_count - 1) - i] = stack[--vm->sp];
    }

    debug_printf("icall to %d return to: %d\n", jump_targets[offset].instruction_offset, vm->call_stack[vm->call_depth].return_addr);

    // execute jump to function
    vm->call_depth++;
    vm->pc = jump_targets[offset].instruction_offset;

    DISPATCH;

opcode_return:
    debug_printf("RETURN\n");

    vm->call_depth--;
    vm->return_value = stack[--vm->sp];

    debug_printf("return to: %d\n", vm->call_stack[vm->call_depth].return_addr);

    // execute jump to return address
    // vm->locals_index = vm->call_stack[vm->call_depth].locals_index;
    locals = vm->call_stack[vm->call_depth].locals;
    vm->pc = vm->call_stack[vm->call_depth].return_addr;

    DISPATCH;

opcode_return_void:
    debug_printf("RETURN_VOID\n");

    vm->call_depth--;

    debug_printf("return to: %d\n", vm->call_stack[vm->call_depth].return_addr);

    // execute jump to return address
    // vm->locals_index = vm->call_stack[vm->call_depth].locals_index;
    locals = vm->call_stack[vm->call_depth].locals;
    vm->pc = vm->call_stack[vm->call_depth].return_addr;

    DISPATCH;

opcode_load_return:
    debug_printf("LOAD_RETURN\n");

    stack[vm->sp++] = vm->return_value;

    DISPATCH;

opcode_func:
    debug_printf("FUNC\n");

    debug_printf("Should not reach here!\n");
    return VM4_STATUS_ERR_FUNC;

    DISPATCH;

opcode_end_func:
    debug_printf("END_FUNC\n");

    debug_printf("Should not reach here!\n");
    return VM4_STATUS_ERR_FUNC;

    DISPATCH;


opcode_coro_init:
    debug_printf("CORO_INIT\n");

    arg_count = instructions[vm->pc++];
    size = instructions[vm->pc++];
    index = instructions[vm->pc++];

    debug_printf("args %d size: %d index: %d\n", arg_count, size, index);

    uint16_t jump_target = jump_targets[index].instruction_offset;

    debug_printf("\ttarget: %d\n", jump_target);

    coroutine = NULL;

    // search for free slot
    int slot = 0;
    for(; slot < cnt_of_array(vm->coroutines); slot++){

        // free slot:
        if(vm->coroutines[slot] <= 0){

            vm->coroutines[slot] = _coroutine_alloc(size);

            if(vm->coroutines[slot] <= 0){

                return VM4_STATUS_COROUTINE_FAIL;
            }

            #ifdef FX4_COMPILER
            coroutine = (coroutine_state_t *)vm->coroutines[slot];
            #else
            coroutine = (coroutine_state_t *)mem2_vp_get_ptr(vm->coroutines[slot]);
            #endif

            coroutine->current_pc = jump_target;
            coroutine->locals_count = size;
            coroutine->tick = vm->current_tick;

            break;
        }
    }

    if(slot >= cnt_of_array(vm->coroutines)){

        debug_printf("No more space for coroutines!\n");

        return VM4_STATUS_MAX_COROUTINES;
    }


    int32_t *coroutine_locals = (int32_t *)(coroutine + 1);

    // // pop args from stack and store in function locals
    for(int i = 0; i < arg_count; i++){

        // reverse order of args
        coroutine_locals[(arg_count - 1) - i] = stack[--vm->sp];
    }

    DISPATCH;

opcode_coro_exit:
    debug_printf("CORO_EXIT\n");\

    _coroutine_free(vm->coroutines[coroutine_index]);
    vm->coroutines[coroutine_index] = 0;

    // coroutine is terminating
    status = VM4_STATUS_COROUTINE_EXITED;

    goto end;

opcode_coro_start:
    debug_printf("CORO_START\n");

    return -1;

    DISPATCH;

opcode_coro_stop:
    debug_printf("CORO_STOP\n");

    return -1;

    DISPATCH;

opcode_yield:
    debug_printf("YIELD\n");

    // save current vm->pc
    coroutine->current_pc = vm->pc;

    // set current tick
    coroutine->tick = vm->current_tick;

    status = VM4_STATUS_COROUTINE_YIELDED;

    if(vm->sp != 0){

        status = VM4_STATUS_ERR_YIELD;
    }

    goto end;

opcode_delay:
    val = stack[--vm->sp];

    debug_printf("DELAY: %d\n", val);

    // save current vm->pc
    coroutine->current_pc = vm->pc;

    // update timer tick
    coroutine->tick = vm->current_tick + val;

    status = VM4_STATUS_COROUTINE_DELAYED;

    if(vm->sp != 0){

        debug_printf("SP: %d\n", vm->sp);

        status = VM4_STATUS_ERR_DELAY;
    }

    goto end;

opcode_rate:
    val = stack[--vm->sp];

    debug_printf("RATE: %d\n", val);

    // save current pc
    coroutine->current_pc = vm->pc;

    // update timer tick
    coroutine->tick += val;

    status = VM4_STATUS_COROUTINE_DELAYED;

    if(vm->sp != 0){

        status = VM4_STATUS_ERR_RATE;
    }

    goto end;

opcode_jump:
    debug_printf("JUMP\n");

    offset = instructions[vm->pc++];

    debug_printf("jump to %d\n", jump_targets[offset].instruction_offset);

    vm->pc = jump_targets[offset].instruction_offset;

    DISPATCH;

opcode_jumpz:
    debug_printf("JUMPZ\n");

    arg1 = stack[--vm->sp];
    offset = instructions[vm->pc++];

    if(arg1 == 0){

        debug_printf("jump to %d\n", jump_targets[offset].instruction_offset);

        vm->pc = jump_targets[offset].instruction_offset;
    }
    else{

        debug_printf("no jump\n");
    }

    DISPATCH;

opcode_jumpnz:
    debug_printf("JUMPNZ\n");

    arg1 = stack[--vm->sp];
    offset = instructions[vm->pc++];

    if(arg1 != 0){

        debug_printf("jump to %d\n", jump_targets[offset].instruction_offset);

        vm->pc = jump_targets[offset].instruction_offset;
    }
    else{

        debug_printf("no jump\n");
    }

    DISPATCH;

opcode_loop:
    debug_printf("LOOP\n");

    arg1 = stack[--vm->sp]; // iterator address
    offset = instructions[vm->pc++];

    // increment iterator
    locals[arg1]++;

    debug_printf("loop to %d\n", jump_targets[offset].instruction_offset);

    vm->pc = jump_targets[offset].instruction_offset;

    DISPATCH;

opcode_zero_local:
    debug_printf("ZERO_LOCAL\n");

    arg1 = stack[--vm->sp]; // var address

    // set var to zero
    locals[arg1] = 0;

    DISPATCH;


opcode_load_obj:
    debug_printf("LOAD_OBJ\n");

    field_hash = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    // type = instructions[vm->pc++];
    type = 0;

    debug_printf("\t%s.%s\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash));

    if(syslib_load_obj(obj_hash, field_hash, type, &val) < 0){

        status = VM4_STATUS_ERR_LOAD_OBJ;
        goto end;
    }

    stack[vm->sp++] = val;

    DISPATCH;


opcode_store_obj:
    debug_printf("STORE_OBJ\n");

    val = stack[--vm->sp];
    field_hash = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    type = instructions[vm->pc++];

    debug_printf("\t%d -> %s.%s\n", val, hash_s_get_str(obj_hash), hash_s_get_str(field_hash));

    if(syslib_store_obj(obj_hash, field_hash, type, val) < 0){

        status = VM4_STATUS_ERR_STORE_OBJ;
        goto end;
    }

    DISPATCH;


opcode_load_obj_n:
    debug_printf("LOAD_OBJ_N\n");

    obj_rank = instructions[vm->pc++];
    field_rank = instructions[vm->pc++];

    field_hash = stack[--vm->sp];
    
    for(int i = 0; i < field_rank; i++){

        field_indexes[i] = stack[--vm->sp];
    }

    obj_hash = stack[--vm->sp];

    for(int i = 0; i < obj_rank; i++){

        obj_indexes[i] = stack[--vm->sp];
    }

    val = 0;

    debug_printf("\t%s:%d.%s:%d -> %d\n", hash_s_get_str(obj_hash), obj_rank, hash_s_get_str(field_hash), field_rank, val);

    if(syslib_load_obj_n(obj_hash, obj_rank, obj_indexes, field_hash, field_rank, field_indexes, &val) < 0){

        status = VM4_STATUS_ERR_LOAD_OBJ_N;
        goto end;
    }

    stack[vm->sp++] = val;

    DISPATCH;

opcode_store_obj_n:
    debug_printf("STORE_OBJ_N\n");

    obj_rank = instructions[vm->pc++];
    field_rank = instructions[vm->pc++];
    type = instructions[vm->pc++];

    val = stack[--vm->sp];
    
    field_hash = stack[--vm->sp];

    for(int i = 0; i < field_rank; i++){

        field_indexes[i] = stack[--vm->sp];
    }

    obj_hash = stack[--vm->sp];

    for(int i = 0; i < obj_rank; i++){

        obj_indexes[i] = stack[--vm->sp];
    }

    debug_printf("\t%d -> %s:%d.%s:%d\n", val, hash_s_get_str(obj_hash), obj_rank, hash_s_get_str(field_hash), field_rank);

    if(syslib_store_obj_n(obj_hash, obj_rank, obj_indexes, field_hash, field_rank, field_indexes, type, val) < 0){

        status = VM4_STATUS_ERR_STORE_OBJ_N;
        goto end;
    }

    DISPATCH;


opcode_vstore_hue:
    debug_printf("VSTORE_HUE\n");

    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    pixelarray_vstore_hue(obj_hash, val);

    DISPATCH;

opcode_vstore_sat:
    debug_printf("VSTORE_SAT\n");

    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    pixelarray_vstore_sat(obj_hash, val);

    DISPATCH;

opcode_vstore_val:
    debug_printf("VSTORE_VAL\n");

    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    pixelarray_vstore_val(obj_hash, val);

    DISPATCH;

opcode_vstore_hsfade:
    debug_printf("VSTORE_HSFADE\n");

    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    pixelarray_vstore_hsfade(obj_hash, val);

    DISPATCH;

opcode_vstore_vfade:
    debug_printf("VSTORE_VFADE\n");

    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];

    pixelarray_vstore_vfade(obj_hash, val);

    DISPATCH;

opcode_pstore_hue:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_HUE %d[%d] = %d\n", obj_hash, index, val);

    pixelarray_pstore_hue(obj_hash, index, val);

    DISPATCH;

opcode_pstore_sat:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_SAT %d[%d] = %d\n", obj_hash, index, val);

    pixelarray_pstore_sat(obj_hash, index, val);

    DISPATCH;

opcode_pstore_val:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index    = stack[--vm->sp];
    
    debug_printf("PSTORE_VAL %d[%d] = %d\n", obj_hash, index, val);

    pixelarray_pstore_val(obj_hash, index, val);

    DISPATCH;

opcode_pstore_hsfade:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index    = stack[--vm->sp];
    
    debug_printf("PSTORE_HSFADE %d[%d] = %d\n", obj_hash, index, val);

    pixelarray_pstore_hsfade(obj_hash, index, val);

    DISPATCH;

opcode_pstore_vfade:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index    = stack[--vm->sp];
    
    debug_printf("PSTORE_VFADE %d[%d] = %d\n", obj_hash, index, val);

    pixelarray_pstore_vfade(obj_hash, index, val);

    DISPATCH;

opcode_pstore_hue2:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index2   = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_HUE2 %d[%d, %d] = %d\n", obj_hash, index, index2, val);

    pixelarray_pstore_hue2(obj_hash, index, index2, val);

    DISPATCH;

opcode_pstore_sat2:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index2   = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_SAT2 %d[%d, %d] = %d\n", obj_hash, index, index2, val);

    pixelarray_pstore_sat2(obj_hash, index, index2, val);

    DISPATCH;

opcode_pstore_val2:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index2   = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_VAL2 %d[%d, %d] = %d\n", obj_hash, index, index2, val);

    pixelarray_pstore_val2(obj_hash, index, index2, val);

    DISPATCH;

opcode_pstore_hsfade2:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index2   = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_HSFADE2 %d[%d, %d] = %d\n", obj_hash, index, index2, val);

    pixelarray_pstore_hsfade2(obj_hash, index, index2, val);

    DISPATCH;

opcode_pstore_vfade2:
    
    val      = stack[--vm->sp];
    obj_hash = stack[--vm->sp];
    index2   = stack[--vm->sp];
    index    = stack[--vm->sp];

    debug_printf("PSTORE_VFADE2 %d[%d, %d] = %d\n", obj_hash, index, index2, val);

    pixelarray_pstore_vfade2(obj_hash, index, index2, val);

    DISPATCH;

opcode_assign_op_obj:
    op          = instructions[vm->pc++];
    val         = stack[--vm->sp]; // right side
    field_hash  = stack[--vm->sp]; // field
    obj_hash    = stack[--vm->sp]; // object

   	if(syslib_op_obj(obj_hash, field_hash, op, val) < 0){

        status = VM4_STATUS_ERR_ASSIGN_OP_OBJ;
        goto end;
    }

    debug_printf("ASSIGN_OP (obj): %s.%s -> %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);
    DISPATCH;

opcode_pixel_vector_op_hue:
    debug_printf("PIXEL_VECTOR_OP_HUE\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];

    pixelarray_vector_op_hue(obj_hash, op, val);

    DISPATCH;

opcode_pixel_vector_op_sat:
    debug_printf("PIXEL_VECTOR_OP_SAT\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];

    pixelarray_vector_op_sat(obj_hash, op, val);

    DISPATCH;

opcode_pixel_vector_op_val:
    debug_printf("PIXEL_VECTOR_OP_VAL\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];

    pixelarray_vector_op_val(obj_hash, op, val);

    DISPATCH;

opcode_pixel_op_hue:
    debug_printf("PIXEL_OP_HUE\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];
    obj_indexes[0] = stack[--vm->sp];

    pixelarray_pixel_op_hue(obj_hash, op, obj_indexes[0], val);

    DISPATCH;

opcode_pixel_op_sat:
    debug_printf("PIXEL_OP_SAT\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];
    obj_indexes[0] = stack[--vm->sp];

    pixelarray_pixel_op_sat(obj_hash, op, obj_indexes[0], val);

    DISPATCH;

opcode_pixel_op_val:
    debug_printf("PIXEL_OP_VAL\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];
    obj_indexes[0] = stack[--vm->sp];

    pixelarray_pixel_op_val(obj_hash, op, obj_indexes[0], val);

    DISPATCH;


opcode_pixel_op_hue2:
    debug_printf("PIXEL_OP_HUE2\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];
    obj_indexes[1] = stack[--vm->sp];
    obj_indexes[0] = stack[--vm->sp];

    pixelarray_pixel_op_hue2(obj_hash, op, obj_indexes[0], obj_indexes[1], val);

    DISPATCH;

opcode_pixel_op_sat2:
    debug_printf("PIXEL_OP_SAT2\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];
    obj_indexes[1] = stack[--vm->sp];
    obj_indexes[0] = stack[--vm->sp];

    pixelarray_pixel_op_sat2(obj_hash, op, obj_indexes[0], obj_indexes[1], val);

    DISPATCH;

opcode_pixel_op_val2:
    debug_printf("PIXEL_OP_VAL2\n");

    op          = instructions[vm->pc++];
    val         = stack[--vm->sp];
    obj_hash    = stack[--vm->sp];
    obj_indexes[1] = stack[--vm->sp];
    obj_indexes[0] = stack[--vm->sp];

    pixelarray_pixel_op_val2(obj_hash, op, obj_indexes[0], obj_indexes[1], val);

    DISPATCH;

opcode_pixel_vector_load:
    debug_printf("PIXEL_VECTOR_LOAD\n");

    op          = instructions[vm->pc++];
    obj_hash    = stack[--vm->sp];

    pixelarray_vector_load(obj_hash, op, &val);

    stack[vm->sp++] = val;

    DISPATCH;

opcode_pixel_load:

    debug_printf("PIXEL_LOAD\n");

    op              = instructions[vm->pc++];
    obj_hash        = stack[--vm->sp];
    obj_indexes[0]  = stack[--vm->sp];

    pixelarray_load(obj_hash, op, obj_indexes[0], -1, &val);

    stack[vm->sp++] = val;

    DISPATCH;

opcode_pixel_load2:

    debug_printf("PIXEL_LOAD2\n");

    op              = instructions[vm->pc++];
    obj_hash        = stack[--vm->sp];
    obj_indexes[1]  = stack[--vm->sp];
    obj_indexes[0]  = stack[--vm->sp];

    pixelarray_load(obj_hash, op, obj_indexes[0], obj_indexes[1], &val);

    stack[vm->sp++] = val;

    DISPATCH;


opcode_assign_op_obj_n:
    op = instructions[vm->pc++];
    val = stack[--vm->sp]; // right side
    field_hash = stack[--vm->sp]; // field
    obj_hash = stack[--vm->sp]; // object

    obj_rank = instructions[vm->pc++];
    field_rank = instructions[vm->pc++];

    for(int i = 0; i < obj_rank; i++){

        obj_indexes[i] = stack[--vm->sp];
    }

    for(int i = 0; i < field_rank; i++){

        field_indexes[i] = stack[--vm->sp];
    }

	if(syslib_op_obj_n(obj_hash, obj_rank, obj_indexes, field_hash, field_rank, field_indexes, op, val) < 0){

        status = VM4_STATUS_ERR_ASSIGN_OP_OBJ_N;
        goto end;
    }

    debug_printf("ASSIGN_OP_N (obj): %s.%s -> %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);
    DISPATCH;

opcode_print:
    arg1 = stack[--vm->sp];

    debug_printf("PRINT: %d\n", arg1);

    #ifndef FX4_COMPILER
    log_v_debug_P(PSTR("VM PRINT: %d"), arg1);
    #endif

    DISPATCH;

opcode_assert:
    debug_printf("ASSERT\n");

    arg1 = stack[--vm->sp];
    
    if(arg1 == 0){

        debug_printf("^^^ Assertion failed! ^^^\n");

        #ifndef FX4_COMPILER
        log_v_error_P(PSTR("VM assertion failed!"));
        #endif

        status = VM4_STATUS_ASSERTION;
        goto end;
    }
    else{

        debug_printf("OK\n");
    }

    DISPATCH;

opcode_halt:
    debug_printf("HALT\n");

    vm->return_value = stack[0];
    
    #ifndef FX4_COMPILER
    // log_v_debug_P(PSTR("VM halt: %d"), vm->return_value);
    #endif

    status = VM4_STATUS_HALT;
    goto end;

opcode_trap:
    debug_printf("VM TRAP: %x\n", opcode);

    #ifndef FX4_COMPILER
    log_v_debug_P(PSTR("VM trap: 0x%02x PC: %d"), opcode, vm->pc);
    #endif

    status = VM4_STATUS_TRAP;
    goto end;

end:
    debug_printf("!!! Finish VM: %d returns: %d\n", status, vm->return_value);

    #ifdef FX4_COMPILER
    debug_printf("!!! Max stack depth: %d\n", max_stack_depth);
    #endif

    vm->pc = vm->pc;

    if(vm->sp >= VM4_STACK_SIZE){

        debug_printf("*** STACK OVER OR UNDERFLOW ***\n");
        status = VM4_STATUS_STACK_OVER_UNDER;
    }
    else if(vm->sp > 0){

        debug_printf("*** VM returned with non-empty stack! %d items ***\n", vm->sp);
        status = VM4_STATUS_STACK_NOT_EMPTY;
    }

    #ifndef FX4_COMPILER
    // log_v_debug_P(PSTR("VM end: %d"), status);
    #endif

    vm->cycle_count = MAX_CYCLES - cycles;

    // pixelarray_print();

    return status;
}

int vm_run_for_time(vm_t *vm, uint32_t increment, uint64_t stop_tick){

	debug_printf("Run for %ld ticks, increment %d\n", stop_tick, increment);

    uint64_t tick = 0;

    while(tick < stop_tick){

    	debug_printf("TICK: %ld\n", tick);

    	if(vm_run_tick(vm, tick) < 0){

    		return -1;
    	}

    	tick += increment;
    }

	return 0;
}


int vm_run_tick(vm_t *vm, uint64_t current_tick){

	vm->current_tick = current_tick;

    uint8_t coroutine_count = 0;
    uint8_t coroutine_ready = 0;

    for(int i = 0; i < cnt_of_array(vm->coroutines); i++){

        int status = vm_run_coroutine_tick(vm, i);

        if(status < 0){

            return status;
        }
        else if((status == VM4_STATUS_COROUTINE_EXITED) ||
                (status == VM4_STATUS_NO_COROUTINE)){

        }
        else if(status == VM4_STATUS_COROUTINE_NOT_READY){

            coroutine_count++;
        }
        else{

            coroutine_ready++;
            coroutine_count++;
        }
    }

    if(coroutine_count == 0){

        return VM4_STATUS_NO_COROUTINE;
    }

    if(coroutine_ready == 0){

        return VM4_STATUS_NO_READY_COROUTINE;
    }

	return VM4_STATUS_OK;
}

int vm_run_coroutine_tick(vm_t *vm, uint8_t index){

	// run all coroutine until they are >= current tick
	if(vm->coroutines[index] <= 0){

        return VM4_STATUS_NO_COROUTINE;
    }

    int status = VM4_STATUS_COROUTINE_NOT_READY;

    #ifdef FX4_COMPILER
    coroutine_state_t *coroutine = (coroutine_state_t *)vm->coroutines[index];
    #else
    coroutine_state_t *coroutine = (coroutine_state_t *)mem2_vp_get_ptr(vm->coroutines[index]);
    #endif

    // coroutine tick specifies next run time
    // if current tick is past coroutine, then it
    // needs to fun.
    // if current tick is before coroutine, coroutine
    // is still waiting
    if(vm->current_tick >= coroutine->tick){

    	// run!
    	status = vm_run_instructions(vm, index, 0);

    	if(status == VM4_STATUS_COROUTINE_EXITED){

            // coroutine exiting

            // status = 0;
        }
        else if(status == VM4_STATUS_COROUTINE_YIELDED){

            // status = 0;
        }
        else if(status == VM4_STATUS_COROUTINE_DELAYED){

            // status = 0;
        }
    }

    return status;
}

int vm_run_coroutines(vm_t *vm){

    for(int i = 0; i < cnt_of_array(vm->coroutines); i++){

        if(vm_run_coroutine(vm, i) < 0){

            return VM4_STATUS_ERROR;
        }
    }

    return 0;
}

int vm_run_coroutine(vm_t *vm, uint8_t index){

    // debug_printf("\n\nRun coroutine: current pc: %d\n", coroutine->current_pc);
    // // debug_printf("\n\nRun coroutine: start: %d current pc: %d\n", coroutine->start_addr, coroutine->current_pc);

    if(vm->coroutines[index] <= 0){

        return VM4_STATUS_NO_COROUTINE;
    }

    int status = -1;

    do{

        status = vm_run_instructions(vm, index, 0);

        if(status == VM4_STATUS_COROUTINE_EXITED){

            // coroutine exiting

            status = 0;
        }
        else if(status == VM4_STATUS_COROUTINE_YIELDED){

            // status = 0;
        }
        else if(status == VM4_STATUS_COROUTINE_DELAYED){

            // status = 0;
        }

    } while((status == VM4_STATUS_COROUTINE_YIELDED) ||
            (status == VM4_STATUS_COROUTINE_DELAYED));

    return status;
}


void vm_init(vm_t *vm){

    #ifdef FX4_COMPILER
    pixelarray_init();
    #endif

    memset(vm, 0, sizeof(vm_t));

    vm->instruction_list = array_alloc(sizeof(uint8_t), 1);
    vm->globals_list     = array_alloc(sizeof(int32_t), 1);
    vm->locals_list      = array_alloc(sizeof(int32_t), 1);
    vm->array_list       = array_alloc(sizeof(int32_t), 1);
    vm->object_list      = array_alloc(sizeof(int32_t), 1);
    vm->constants_list   = array_alloc(sizeof(int32_t), 1);
    vm->jump_targets     = array_alloc(sizeof(jump_target_t), 1);

    // init stack to 0s
    memset(vm->stack, 0, sizeof(vm->stack));
}

static void deinit_coroutines(vm_t *vm){

    for(int i = 0; i < cnt_of_array(vm->coroutines); i++){

        if(vm->coroutines[i] > 0){

            _coroutine_free(vm->coroutines[i]);

            vm->coroutines[i] = -1;
        }
    }
}

void vm_deinit(vm_t *vm){

    pixelarray_destroy(vm->vm_id);

    #ifdef FX4_COMPILER
    pixelarray_deinit();
    #endif

    if(vm->instruction_list > 0){

        array_free(vm->instruction_list);    
    }

    if(vm->globals_list > 0){

        array_free(vm->globals_list);    
    }

    if(vm->locals_list > 0){

        array_free(vm->locals_list);    
    }

    if(vm->array_list > 0){

        array_free(vm->array_list);    
    }

    if(vm->object_list > 0){

        array_free(vm->object_list);    
    }

    if(vm->constants_list > 0){

        array_free(vm->constants_list);    
    }

    if(vm->jump_targets > 0){

        array_free(vm->jump_targets);    
    }

    deinit_coroutines(vm);
}

int vm_get_state_size(vm_t *vm){

    return array_get_used_space(vm->instruction_list) +
           array_get_used_space(vm->globals_list) +
           array_get_used_space(vm->locals_list) +
           array_get_used_space(vm->array_list) +
           array_get_used_space(vm->object_list) +
           array_get_used_space(vm->constants_list) +
           array_get_used_space(vm->jump_targets) +
           sizeof(vm_t);
}


static const uint8_t instruction_sizes[] = {
    1,  // 0x00: OPCODE_TRAP
    2,  // 0x01: OPCODE_PUSH_CONST
    1,  // 0x02: OPCODE_ADD
    1,  // 0x03: OPCODE_SUB
    1,  // 0x04: OPCODE_MUL
    1,  // 0x05: OPCODE_DIV
    1,  // 0x06: OPCODE_PRINT
    1,  // 0x07: OPCODE_HALT
    1,  // 0x08: OPCODE_STORE_GLOBAL
    2,  // 0x09: OPCODE_LOAD_GLOBAL
    1,  // 0x0a: OPCODE_ARRAY_SIZE
    1,  // 0x0b: OPCODE_ARRAY_INDEX
    1,  // 0x0c: OPCODE_STORE_ARRAY
    1,  // 0x0d: OPCODE_LOAD_ARRAY
    2,  // 0x0e: OPCODE_CALL_NATIVE
    4,  // 0x0f: OPCODE_CALL
    1,  // 0x10: OPCODE_RETURN
    1,  // 0x11: OPCODE_FUNC
    1,  // 0x12: OPCODE_END_FUNC
    2,  // 0x13: OPCODE_JUMP
    2,  // 0x14: OPCODE_JUMPZ
    1,  // 0x15: OPCODE_DISCARD
    1,  // 0x16: OPCODE_LOAD_RETURN
    1,  // 0x17: OPCODE_RETURN_VOID
    1,  // 0x18: OPCODE_LOAD_OBJ
    2,  // 0x19: OPCODE_STORE_OBJ
    3,  // 0x1a: OPCODE_LOAD_OBJ_N
    4,  // 0x1b: OPCODE_STORE_OBJ_N
    1,  // 0x1c: OPCODE_COMPLT
    2,  // 0x1d: OPCODE_JUMPNZ
    1,  // 0x1e: OPCODE_CONV0_I32_TO_F16
    1,  // 0x1f: OPCODE_CONV1_I32_TO_F16
    1,  // 0x20: OPCODE_CONV0_F16_TO_I32
    1,  // 0x21: OPCODE_CONV1_F16_TO_I32
    1,  // 0x22: OPCODE_COMPLTE
    2,  // 0x23: OPCODE_LOOP
    1,  // 0x24: OPCODE_ZERO
    1,  // 0x25: OPCODE_COMPGT
    1,  // 0x26: OPCODE_COMPGTE
    3,  // 0x27: OPCODE_ICALL
    1,  // 0x28: OPCODE_OBJ_SIZE
    2,  // 0x29: OPCODE_LOAD_LOCAL
    1,  // 0x2a: OPCODE_STORE_LOCAL
    4,  // 0x2b: OPCODE_CORO_INIT
    1,  // 0x2c: OPCODE_CORO_START
    1,  // 0x2d: OPCODE_CORO_STOP
    1,  // 0x2e: OPCODE_ZERO_LOCAL
    1,  // 0x2f: OPCODE_COMPEQ
    1,  // 0x30: OPCODE_COMPNEQ
    1,  // 0x31: OPCODE_ASSERT
    1,  // 0x32: OPCODE_RESUME
    1,  // 0x33: OPCODE_CORO_EXIT
    1,  // 0x34: OPCODE_YIELD
    1,  // 0x35: OPCODE_DELAY
    1,  // 0x36: OPCODE_ADD_ASSIGN
    1,  // 0x37: OPCODE_MOD
    1,  // 0x38: OPCODE_ADD_ASSIGN_LOCAL
    1,  // 0x39: OPCODE_SUB_ASSIGN
    1,  // 0x3a: OPCODE_SUB_ASSIGN_LOCAL
    1,  // 0x3b: OPCODE_STORE_VECTOR
    2,  // 0x3c: OPCODE_ASSIGN_OP_ARRAY
    2,  // 0x3d: OPCODE_ASSIGN_OP_VECTOR
    1,  // 0x3e: OPCODE_MUL_F16
    1,  // 0x3f: OPCODE_VSTORE_HUE
    1,  // 0x40: OPCODE_PSTORE_HUE
    2,  // 0x41: OPCODE_ASSIGN_OP_OBJ
    4,  // 0x42: OPCODE_ASSIGN_OP_OBJ_N
    1,  // 0x43: OPCODE_RATE
    1,  // 0x44: OPCODE_DIV_F16
    1,  // 0x45: OPCODE_PSTORE_SAT
    1,  // 0x46: OPCODE_PSTORE_VAL
    1,  // 0x47: OPCODE_PSTORE_HSFADE
    1,  // 0x48: OPCODE_PSTORE_VFADE
    1,  // 0x49: OPCODE_VSTORE_SAT
    1,  // 0x4A: OPCODE_VSTORE_VAL
    1,  // 0x4B: OPCODE_VSTORE_HSFADE
    1,  // 0x4C: OPCODE_VSTORE_VFADE
    2,  // 0x4D: OPCODE_PIXEL_VECTOR_OP_HUE
    2,  // 0x4E: OPCODE_PIXEL_OP_HUE
    3,  // 0x4F: OPCODE_PUSH_CONST2
    2,  // 0x50: OPCODE_PIXEL_VECTOR_LOAD
    2,  // 0x51: OPCODE_PIXEL_LOAD
    2,  // 0x52: OPCODE_LOAD_ACCUM
    1,  // 0x53: OPCODE_SWAP
    2,  // 0x54: OPCODE_MOVE_VECTOR
    1,  // 0x55: OPCODE_MOVE_OBJ
    1,  // 0x56: OPCODE_LOAD_REF
    1,  // 0x57: OPCODE_PSTORE_HUE2
    1,  // 0x58: OPCODE_PSTORE_SAT2
    1,  // 0x59: OPCODE_PSTORE_VAL2
    1,  // 0x5A: OPCODE_PSTORE_HSFADE2
    1,  // 0x5B: OPCODE_PSTORE_VFADE2
    2,  // 0x5C: OPCODE_PIXEL_LOAD2
    2,  // 0x5D: OPCODE_PIXEL_VECTOR_OP_SAT
    2,  // 0x5E: OPCODE_PIXEL_OP_SAT
    2,  // 0x5F: OPCODE_PIXEL_VECTOR_OP_VAL
    2,  // 0x60: OPCODE_PIXEL_OP_VAL
    2,  // 0x61: OPCODE_PIXEL_OP_HUE2
    2,  // 0x62: OPCODE_PIXEL_OP_SAT2
    2,  // 0x63: OPCODE_PIXEL_OP_VAL2
    3,  // 0x64: OPCODE_PUBLISH
    1,  // 0x65: OPCODE_MUL_ASSIGN
    1,  // 0x66: OPCODE_MUL_ASSIGN_LOCAL
    1,  // 0x67: OPCODE_DIV_ASSIGN
    1,  // 0x68: OPCODE_DIV_ASSIGN_LOCAL
    1,  // 0x69: OPCODE_MOD_ASSIGN
    1,  // 0x6A: OPCODE_MOD_ASSIGN_LOCAL
    1,  // 0x6B: OPCODE_LOGICAL_AND
    1,  // 0x6C: OPCODE_LOGICAL_OR
};

int vm_get_instruction_size(uint8_t opcode){

    if(opcode >= cnt_of_array(instruction_sizes)){

       return -1;
    }

    return instruction_sizes[opcode];
}

void print_vm_state(vm_t *vm){

    #ifdef FX4_COMPILER

    // int32_t *stack        = (int32_t *)array_get_data(vm->stack_list);
    int32_t *stack              = vm->stack;
    int32_t *globals            = (int32_t *)array_get_data(vm->globals_list);
    int32_t *locals             = (int32_t *)array_get_data(vm->locals_list);
    int32_t *array              = (int32_t *)array_get_data(vm->array_list);
    int32_t *objects            = (int32_t *)array_get_data(vm->object_list);
    int32_t *constants          = (int32_t *)array_get_data(vm->constants_list);
    jump_target_t *jump_targets = (jump_target_t *)array_get_data(vm->jump_targets);

    debug_printf("VM State:\n");
    debug_printf("  PC: %d\n", vm->pc);
    debug_printf("  Call depth: %d\n", vm->call_depth);
    debug_printf("  SP: %d TOS -> [ ", vm->sp);
    for (uint32_t i = 0; i < vm->sp; i++) {
        debug_printf("%d ", stack[(vm->sp - 1) - i]);
    }
    debug_printf("]");
    debug_printf("\n");
    debug_printf("  Globals: ");
    for (int i = 0; i < array_get_count(vm->globals_list); i++){
        debug_printf("%d ", globals[i]);
    }
    debug_printf("\n");
    debug_printf("  Locals: ");
    for (int i = 0; i < array_get_count(vm->locals_list); i++){
        debug_printf("%d ", locals[i]);
    }
    debug_printf("\n");
    debug_printf("  Array: ");
    for (int i = 0; i < array_get_count(vm->array_list); i++){
        debug_printf("%d ", array[i]);
    }
    debug_printf("\n");
    debug_printf("  Objects: ");
    for (int i = 0; i < array_get_count(vm->object_list); i++){
        debug_printf("%d ", objects[i]);
    }
    debug_printf("\n");
    debug_printf("  Constants: ");
    for (int i = 0; i < array_get_count(vm->constants_list); i++) {
        debug_printf("%d ", constants[i]);
    }
    debug_printf("\n");
    debug_printf("  Jump targets: ");
    for (int i = 0; i < array_get_count(vm->jump_targets); i++) {
        debug_printf("%d ", jump_targets[i].instruction_offset);
    }
    debug_printf("\n");
    debug_printf("  Coroutines: \n");
    for (int i = 0; i < cnt_of_array(vm->coroutines); i++) {
        coroutine_state_t *coroutine = (coroutine_state_t *)vm->coroutines[i];

        if(coroutine == NULL){

            continue;
        }

        debug_printf("\tPC: %d tick: %lu locals: %d [", coroutine->current_pc, coroutine->tick, coroutine->locals_count);

        int32_t *coroutine_locals = (int32_t *)(coroutine + 1);
        for(int j = 0; j < coroutine->locals_count; j++){

            debug_printf("%d ", coroutine_locals[j]);
        }
        debug_printf("]");
    }
    debug_printf("\n");
    debug_printf("  Return: %d\n", vm->return_value);

    #endif
}

char* vm_get_opcode_str(uint8_t opcode){
    switch(opcode) {
        case OPCODE_TRAP:               return "TRAP";
        case OPCODE_PUSH_CONST:         return "PUSH_CONST";
        case OPCODE_ADD:                return "ADD";
        case OPCODE_SUB:                return "SUB";
        case OPCODE_MUL:                return "MUL";
        case OPCODE_DIV:                return "DIV";
        case OPCODE_PRINT:              return "PRINT";
        case OPCODE_HALT:               return "HALT";
        case OPCODE_STORE_GLOBAL:       return "STORE_GLOBAL";
        case OPCODE_LOAD_GLOBAL:        return "LOAD_GLOBAL";
        case OPCODE_ARRAY_SIZE:         return "ARRAY_SIZE";
        case OPCODE_STORE_ARRAY:        return "STORE_ARRAY";
        case OPCODE_LOAD_ARRAY:         return "LOAD_ARRAY";
        case OPCODE_CALL_NATIVE:        return "CALL_NATIVE";
        case OPCODE_CALL:               return "CALL";
        case OPCODE_RETURN:             return "RETURN";
        case OPCODE_FUNC:               return "FUNC";
        case OPCODE_END_FUNC:           return "END_FUNC";
        case OPCODE_JUMP:               return "JUMP";
        case OPCODE_JUMP_Z:             return "JUMP_Z";
        case OPCODE_DISCARD:            return "DISCARD";
        case OPCODE_LOAD_RETURN:        return "LOAD_RETURN";
        case OPCODE_RETURN_VOID:        return "RETURN_VOID";
        case OPCODE_LOAD_OBJ:           return "LOAD_OBJ";
        case OPCODE_STORE_OBJ:          return "STORE_OBJ";
        case OPCODE_LOAD_OBJ_N:         return "LOAD_OBJ_N";
        case OPCODE_STORE_OBJ_N:        return "STORE_OBJ_N";
        case OPCODE_COMPLT:             return "COMPLT";
        case OPCODE_JUMP_NZ:            return "JUMP_NZ";
        case OPCODE_CONV0_I32_TO_F16:   return "CONV0_I32_TO_F16";
        case OPCODE_CONV1_I32_TO_F16:   return "CONV1_I32_TO_F16";
        case OPCODE_CONV0_F16_TO_I32:   return "CONV0_F16_TO_I32";
        case OPCODE_CONV1_F16_TO_I32:   return "CONV1_F16_TO_I32";
        case OPCODE_COMPLTE:            return "COMPLTE";
        case OPCODE_LOOP:               return "LOOP";
        case OPCODE_COMPGT:             return "COMPGT";
        case OPCODE_COMPGTE:            return "COMPGTE";
        case OPCODE_ICALL:              return "ICALL";
        case OPCODE_OBJ_SIZE:           return "OBJ_SIZE";
        case OPCODE_LOAD_LOCAL:         return "LOAD_LOCAL";
        case OPCODE_STORE_LOCAL:        return "STORE_LOCAL";
        case OPCODE_CORO_INIT:          return "CORO_INIT";
        case OPCODE_CORO_START:         return "CORO_START";
        case OPCODE_CORO_STOP:          return "CORO_STOP";
        case OPCODE_ZERO_LOCAL:         return "ZERO_LOCAL";
        case OPCODE_COMPEQ:             return "COMPEQ";
        case OPCODE_COMPNEQ:            return "COMPNEQ";
        case OPCODE_ASSERT:             return "ASSERT";
        // case OPCODE_RESUME:             return "RESUME";
        case OPCODE_CORO_EXIT:          return "CORO_EXIT";
        case OPCODE_YIELD:              return "YIELD";
        case OPCODE_DELAY:              return "DELAY";
        case OPCODE_ADD_ASSIGN:         return "ADD_ASSIGN";
        case OPCODE_MOD:                return "MOD";
        case OPCODE_ADD_ASSIGN_LOCAL:   return "ADD_ASSIGN_LOCAL";
        case OPCODE_SUB_ASSIGN:         return "SUB_ASSIGN";
        case OPCODE_SUB_ASSIGN_LOCAL:   return "SUB_ASSIGN_LOCAL";
        case OPCODE_STORE_VECTOR:       return "STORE_VECTOR";
        case OPCODE_ASSIGN_OP_ARRAY:    return "ASSIGN_OP_ARRAY";
        case OPCODE_ASSIGN_OP_VECTOR:   return "ASSIGN_OP_VECTOR";
        case OPCODE_MUL_F16:            return "MUL_F16";
        case OPCODE_VSTORE_HUE:         return "VSTORE_HUE";
        case OPCODE_PSTORE_HUE:         return "PSTORE_HUE";
        case OPCODE_ASSIGN_OP_OBJ:      return "ASSIGN_OP_OBJ";
        case OPCODE_ASSIGN_OP_OBJ_N:    return "ASSIGN_OP_OBJ_N";
        case OPCODE_RATE:               return "RATE";
        case OPCODE_DIV_F16:            return "DIV_F16";
        case OPCODE_PSTORE_SAT:         return "PSTORE_SAT";
        case OPCODE_PSTORE_VAL:         return "PSTORE_VAL";
        case OPCODE_PSTORE_HSFADE:      return "PSTORE_HSFADE";
        case OPCODE_PSTORE_VFADE:       return "PSTORE_VFADE";
        case OPCODE_VSTORE_SAT:         return "VSTORE_SAT";
        case OPCODE_VSTORE_VAL:         return "VSTORE_VAL";
        case OPCODE_VSTORE_HSFADE:      return "VSTORE_HSFADE";
        case OPCODE_VSTORE_VFADE:       return "VSTORE_VFADE"; 
        case OPCODE_PIXEL_VECTOR_OP_HUE:return "PIXEL_VECTOR_OP_HUE";    
        case OPCODE_PIXEL_OP_HUE:       return "PIXEL_OP_HUE";
        case OPCODE_PUSH_CONST2:        return "PUSH_CONST2";
        case OPCODE_PIXEL_VECTOR_LOAD:  return "PIXEL_VECTOR_LOAD";
        case OPCODE_PIXEL_LOAD:         return "PIXEL_LOAD";
        // case OPCODE_LOAD_ACCUM:         return "LOAD_ACCUM";
        case OPCODE_SWAP:               return "SWAP";
        case OPCODE_MOVE_VECTOR:        return "MOVE_VECTOR";
        case OPCODE_MOVE_OBJ:           return "MOVE_OBJ";
        case OPCODE_LOAD_REF:           return "LOAD_REF";
        case OPCODE_PSTORE_HUE2:        return "PSTORE_HUE2";
        case OPCODE_PSTORE_SAT2:        return "PSTORE_SAT2";
        case OPCODE_PSTORE_VAL2:        return "PSTORE_VAL2";
        case OPCODE_PSTORE_HSFADE2:     return "PSTORE_HSFADE2";
        case OPCODE_PSTORE_VFADE2:      return "PSTORE_VFADE2";
        case OPCODE_PIXEL_LOAD2:        return "PIXEL_LOAD2";
        case OPCODE_PIXEL_VECTOR_OP_SAT:return "PIXEL_VECTOR_OP_SAT";    
        case OPCODE_PIXEL_OP_SAT:       return "PIXEL_OP_SAT";
        case OPCODE_PIXEL_VECTOR_OP_VAL:return "PIXEL_VECTOR_OP_VAL";    
        case OPCODE_PIXEL_OP_VAL:       return "PIXEL_OP_VAL";
        case OPCODE_PIXEL_OP_HUE2:      return "PIXEL_OP_HUE2";
        case OPCODE_PIXEL_OP_SAT2:      return "PIXEL_OP_SAT2";
        case OPCODE_PIXEL_OP_VAL2:      return "PIXEL_OP_VAL2";
        case OPCODE_PUBLISH:            return "PUBLISH";
        case OPCODE_MUL_ASSIGN:         return "MUL_ASSIGN";
        case OPCODE_MUL_ASSIGN_LOCAL:   return "MUL_ASSIGN_LOCAL";
        case OPCODE_DIV_ASSIGN:         return "DIV_ASSIGN";
        case OPCODE_DIV_ASSIGN_LOCAL:   return "DIV_ASSIGN_LOCAL";
        case OPCODE_MOD_ASSIGN:         return "MOD_ASSIGN";
        case OPCODE_MOD_ASSIGN_LOCAL:   return "MOD_ASSIGN_LOCAL";
        case OPCODE_LOGICAL_AND:        return "LOGICAL_AND";
        case OPCODE_LOGICAL_OR:         return "LOGICAL_OR";

        default:                        return "UNKNOWN_OPCODE";
    }
}


void vm_print_listing(
    uint8_t *instructions,
    uint16_t count,
    int32_t *constants,
    uint16_t const_count,
    jump_target_t *jump_targets,
    uint16_t jump_count){

    debug_printf("%d bytes\n", count);

    int index = 0;

    while(index < count){

        debug_printf("%3d: ", index);

        char *s = NULL;

        switch(instructions[index]){
            case OPCODE_PUSH_CONST:
                s = hash_s_get_str((uint32_t)constants[instructions[index + 1]]);

                if(s == NULL){

                    debug_printf("%s: %d\n", vm_get_opcode_str(instructions[index]), constants[instructions[index + 1]]);    
                }
                else{
                    debug_printf("%s: %s\n", vm_get_opcode_str(instructions[index]), s);
                }
                
                break;

            // case OPCODE_LOAD_ACCUM:
                // debug_printf("%s: %d\n", vm_get_opcode_str(instructions[index]), constants[instructions[index + 1]]);
                // break;

            case OPCODE_CALL:
                debug_printf("CALL -> %d\n", jump_targets[instructions[index + 3]].instruction_offset);
                break;

            case OPCODE_ICALL:
                debug_printf("ICALL -> %d\n", jump_targets[instructions[index + 2]].instruction_offset);
                break;

            case OPCODE_JUMP:
                debug_printf("JUMP -> %d\n", jump_targets[instructions[index + 1]].instruction_offset);

                break;

            case OPCODE_JUMP_Z:
                debug_printf("JUMPZ -> %d\n", jump_targets[instructions[index + 1]].instruction_offset);

                break;
            case OPCODE_JUMP_NZ:
                debug_printf("JUMPNZ -> %d\n", jump_targets[instructions[index + 1]].instruction_offset);
                break;

            case OPCODE_LOOP:
                debug_printf("LOOP -> %d\n", jump_targets[instructions[index + 1]].instruction_offset);
                break;

            case OPCODE_PUSH_CONST2:
                debug_printf("PUSH_CONST2: %d %d\n", constants[instructions[index + 1]], constants[instructions[index + 2]]);
                break;

            default:
                debug_printf("%s\n", vm_get_opcode_str(instructions[index]));
                break;
        }

        // index++;
        int instruction_size = vm_get_instruction_size(instructions[index]);

        if(instruction_size < 0){

            printf("Invalid instruction size!\n");

            return;
        }

        index += instruction_size;
    }

}

void print_vm_listing(vm_t *vm){

    uint8_t *instructions = (uint8_t *)array_get_data(vm->instruction_list);
    int32_t *constants    = (int32_t *)array_get_data(vm->constants_list);
    jump_target_t *jump_targets = (jump_target_t *)array_get_data(vm->jump_targets);

    vm_print_listing(
        instructions,
        array_get_count(vm->instruction_list),
        constants,
        array_get_count(vm->constants_list),
        jump_targets,
        array_get_count(vm->jump_targets)
    );
}

int vm_get_serialized_size(vm_t *vm){

    int instructions_size   = array_get_data_space(vm->instruction_list);
    int globals_size        = array_get_data_space(vm->globals_list);
    int locals_size         = array_get_data_space(vm->locals_list);
    int array_size          = array_get_data_space(vm->array_list);
    int object_size         = array_get_data_space(vm->object_list);
    int constants_size      = array_get_data_space(vm->constants_list);
    int jump_targets_size   = array_get_data_space(vm->jump_targets);

    int size = sizeof(vm_stream_header_t) +
        instructions_size +
        globals_size +
        locals_size +
        array_size +
        object_size +
        constants_size +
        jump_targets_size;

    return size;
}


int vm_get_coroutine_count(vm_t *vm){

    int count = 0;

    for(int i = 0; i < MAX_COROUTINES; i++){

        if(vm->coroutines[i] > 0){

            count++;
        }
    }

    return count;
}

int vm_serialize(vm_t *vm, char *fname){

    #ifdef FX4_COMPILER
    FILE* fp = fopen(fname, "w");

    if(fp == NULL){

        return VM4_STATUS_FILE_NOT_FOUND;
    }
    #else
    file_t f = fs_f_open(fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND);

    if(f <= 0){

        return VM4_STATUS_FILE_NOT_FOUND;
    }
    #endif

    int status = VM4_STATUS_OK;

    vm_stream_header_t header = {
        BYTECODE_MAGIC,
        BYTECODE_VERSION,
        0,
        array_get_data_space(vm->instruction_list),
        array_get_data_space(vm->globals_list),
        array_get_data_space(vm->locals_list),
        array_get_data_space(vm->array_list),
        array_get_data_space(vm->object_list),
        array_get_data_space(vm->constants_list),
        array_get_data_space(vm->jump_targets),
        #ifdef FX4_COMPILER
        hash_get_count(),
        #else
        0, // OS version is omitting strings for now
        #endif
        vm_get_coroutine_count(vm),
        pixelarray_count(),
        vm->rng_seed,
        vm->frame_number,
        vm->current_tick,
    };

    if(header.instruction_len == 0){

        status = VM4_STATUS_BAD_INS_LEN;
        goto done;
    }

    #ifdef FX4_COMPILER
    fwrite(&header, 1, sizeof(header), fp);
    fwrite(array_get_data(vm->instruction_list),    1, array_get_data_space(vm->instruction_list),  fp);
    fwrite(array_get_data(vm->globals_list),        1, array_get_data_space(vm->globals_list),      fp);
    fwrite(array_get_data(vm->locals_list),         1, array_get_data_space(vm->locals_list),       fp);
    fwrite(array_get_data(vm->array_list),          1, array_get_data_space(vm->array_list),        fp);
    fwrite(array_get_data(vm->object_list),         1, array_get_data_space(vm->object_list),       fp);
    fwrite(array_get_data(vm->constants_list),      1, array_get_data_space(vm->constants_list),    fp);
    fwrite(array_get_data(vm->jump_targets),        1, array_get_data_space(vm->jump_targets),      fp);
    #else
    fs_i16_write(f, (uint8_t *)&header, sizeof(header));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->instruction_list),    array_get_data_space(vm->instruction_list));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->globals_list),        array_get_data_space(vm->globals_list));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->locals_list),         array_get_data_space(vm->locals_list));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->array_list),          array_get_data_space(vm->array_list));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->object_list),         array_get_data_space(vm->object_list));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->constants_list),      array_get_data_space(vm->constants_list));
    fs_i16_write(f, (uint8_t *)array_get_data(vm->jump_targets),        array_get_data_space(vm->jump_targets));
    #endif

    #ifdef FX4_COMPILER
    // write strings:
    for(int i = 0; i < hash_get_count(); i++){

        char *s = hash_get_string_at_index(i);

        if(s == NULL){

            status = VM4_STATUS_STRING_NOT_FOUND;
            goto done;
        }

        uint32_t hash = hash_u32_string(s);

        fwrite(&hash, sizeof(hash), 1, fp);

        uint16_t len = strlen(s) + 1; // account for null terminator
        fwrite(&len, sizeof(len), 1, fp);
        fwrite(s, len - 1, 1, fp); // account for null terminator

        // write null terminator
        char z = '\0';
        fwrite(&z, 1, 1, fp);
    }
    #else

    // OS omits strings

    #endif

    // write coroutines:
    for(int i = 0; i < MAX_COROUTINES; i++){

        if(vm->coroutines[i] <= 0){

            continue;
        }

        #ifdef FX4_COMPILER
        coroutine_state_t *coroutine = (coroutine_state_t *)vm->coroutines[i];
        #else
        coroutine_state_t *coroutine = (coroutine_state_t *)mem2_vp_get_ptr(vm->coroutines[i]);
        #endif

        int32_t *coroutine_locals = (int32_t *)(coroutine + 1);

        coroutine_state_serialized_t serialized = {
            coroutine->current_pc,
            i,
            coroutine->locals_count,
            coroutine->tick,
        };

        #ifdef FX4_COMPILER
        fwrite(&serialized, 1, sizeof(serialized), fp);            
        fwrite(coroutine_locals, coroutine->locals_count, sizeof(int32_t), fp);
        #else
        fs_i16_write(f, (uint8_t *)&serialized, sizeof(serialized));
        fs_i16_write(f, (uint8_t *)coroutine_locals, coroutine->locals_count * sizeof(int32_t));
        #endif
    }

    // write pixelarrays
    pixel_array_t *pixarray = pixelarray_get_ptr();

    for(int i = 0; i < header.pixelarray_len; i++){

        #ifdef FX4_COMPILER
        fwrite(pixarray, 1, sizeof(pixel_array_t), fp);            
        #else
        fs_i16_write(f, (uint8_t *)pixarray, sizeof(pixel_array_t));
        #endif

        pixarray++;
    }

done:
    #ifdef FX4_COMPILER
    fclose(fp);
    #else
    fs_f_close(f);
    #endif

    return status;
}

int vm_deserialize(vm_t *vm, char *fname){

    int status = 0;

    if(vm->instruction_list != 0){

        debug_printf("VM state must be set to 0s to deserialize!\n");

        status = VM4_STATUS_NOT_INITIALIZED;
        goto error;
    }

    #ifdef FX4_COMPILER
    FILE* fp = fopen(fname, "r");

    if(fp == NULL){

        return VM4_STATUS_FILE_NOT_FOUND;
    }
    #else
    file_t f = fs_f_open(fname, FS_MODE_READ_ONLY);

    if(f <= 0){

        status = VM4_STATUS_FILE_NOT_FOUND;
        goto error;
    }
    #endif

    // remember VM ID
    int32_t vm_id = vm->vm_id;

    // reset VM state
    memset(vm, 0, sizeof(vm_t));

    // restore VM ID
    vm->vm_id = vm_id;

    // init stack to 0s
    memset(vm->stack, 0, sizeof(vm->stack));

    #ifndef FX4_COMPILER
    uint32_t hash = hash_u32_start();
    #endif


    vm_stream_header_t header = {0};
    #ifdef FX4_COMPILER
    fread(&header, 1, sizeof(header), fp);
    #else
    fs_i16_read(f, (uint8_t *)&header, sizeof(header));
    hash = hash_u32_partial(hash, (uint8_t *)&header, sizeof(header));
    #endif

    if(header.magic != BYTECODE_MAGIC){

        return VM4_STATUS_BAD_MAGIC;
    }

    if(header.version != BYTECODE_VERSION){

        return VM4_STATUS_WRONG_VERSION;
    }

    vm->instruction_list = array_alloc(sizeof(uint8_t), header.instruction_len);
    if(vm->instruction_list <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    if(header.instruction_len == 0){

        status = VM4_STATUS_BAD_INS_LEN;
        goto error;        
    }

    debug_printf("ins: %d\n", header.instruction_len);

    for(uint32_t i = 0; i < header.instruction_len; i++){

        uint8_t byte = 0;
        #ifdef FX4_COMPILER
        fread(&byte, 1, sizeof(byte), fp);
        #else
        fs_i16_read(f, (uint8_t *)&byte, sizeof(byte));
        hash = hash_u32_partial(hash, &byte, sizeof(byte));
        #endif

        array_add(&vm->instruction_list, &byte);
    }

    vm->globals_list = array_alloc(sizeof(int32_t), header.globals_len / sizeof(int32_t));
    if(vm->globals_list <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    for(uint32_t i = 0; i < header.globals_len / sizeof(int32_t); i++){

        int32_t data = 0;
        #ifdef FX4_COMPILER
        fread(&data, 1, sizeof(data), fp);
        #else
        fs_i16_read(f, (uint8_t *)&data, sizeof(data));
        hash = hash_u32_partial(hash, (uint8_t *)&data, sizeof(data));
        #endif
        array_add(&vm->globals_list, &data);
    }

    vm->locals_list = array_alloc(sizeof(int32_t), header.locals_len / sizeof(int32_t));
    if(vm->locals_list <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    for(uint32_t i = 0; i < header.locals_len / sizeof(int32_t); i++){

        int32_t data = 0;
        #ifdef FX4_COMPILER
        fread(&data, 1, sizeof(data), fp);
        #else
        fs_i16_read(f, (uint8_t *)&data, sizeof(data));
        hash = hash_u32_partial(hash, (uint8_t *)&data, sizeof(data));
        #endif

        array_add(&vm->locals_list, &data);
    }

    vm->array_list = array_alloc(sizeof(int32_t), header.array_len / sizeof(int32_t));
    if(vm->array_list <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    for(uint32_t i = 0; i < header.array_len / sizeof(int32_t); i++){

        int32_t data = 0;
        #ifdef FX4_COMPILER
        fread(&data, 1, sizeof(data), fp);
        #else
        fs_i16_read(f, (uint8_t *)&data, sizeof(data));
        hash = hash_u32_partial(hash, (uint8_t *)&data, sizeof(data));
        #endif

        array_add(&vm->array_list, &data);
    }

    vm->object_list = array_alloc(sizeof(int32_t), header.object_len / sizeof(int32_t));
    if(vm->object_list <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    for(uint32_t i = 0; i < header.object_len / sizeof(int32_t); i++){

        int32_t data = 0;
        #ifdef FX4_COMPILER
        fread(&data, 1, sizeof(data), fp);
        #else
        fs_i16_read(f, (uint8_t *)&data, sizeof(data));
        hash = hash_u32_partial(hash, (uint8_t *)&data, sizeof(data));
        #endif

        array_add(&vm->object_list, &data);
    }

    vm->constants_list = array_alloc(sizeof(int32_t), header.constants_len / sizeof(int32_t));
    if(vm->constants_list <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    for(uint32_t i = 0; i < header.constants_len / sizeof(int32_t); i++){

        int32_t data = 0;
        #ifdef FX4_COMPILER
        fread(&data, 1, sizeof(data), fp);
        #else
        fs_i16_read(f, (uint8_t *)&data, sizeof(data));
        hash = hash_u32_partial(hash, (uint8_t *)&data, sizeof(data));
        #endif

        array_add(&vm->constants_list, &data);
    }

    vm->jump_targets = array_alloc(sizeof(jump_target_t), header.jump_targets_len / sizeof(jump_target_t));
    if(vm->jump_targets <= 0){

        status = VM4_STATUS_ARRAY_ALLOC_FAIL;
        goto error;
    }

    for(uint32_t i = 0; i < header.jump_targets_len / sizeof(jump_target_t); i++){

        jump_target_t data = {0};
        #ifdef FX4_COMPILER
        fread(&data, 1, sizeof(data), fp);
        #else
        fs_i16_read(f, (uint8_t *)&data, sizeof(data));
        hash = hash_u32_partial(hash, (uint8_t *)&data, sizeof(data));
        #endif

        array_add(&vm->jump_targets, &data);
    }

    #ifdef FX4_COMPILER
    debug_printf("strings count %d\n", header.strings_len);
    #else
    // log_v_debug_P(PSTR("strings count %d\n"),header.strings_len);
    #endif


    // record byte stream offset of strings section
    #ifdef FX4_COMPILER
    vm->strings_offset = ftell(fp);
    #else
    vm->strings_offset = fs_i32_tell(f);
    #endif

    for(uint32_t i = 0; i < header.strings_len; i++){

        uint32_t hash = 0;
        uint16_t len = 0;

        #ifdef FX4_COMPILER
        fread(&hash, 1, sizeof(hash), fp);
        #else
        fs_i16_read(f, (uint8_t *)&hash, sizeof(hash));
        hash = hash_u32_partial(hash, (uint8_t *)&hash, sizeof(hash));
        #endif

        #ifdef FX4_COMPILER
        fread(&len, 1, sizeof(len), fp);
        #else
        fs_i16_read(f, (uint8_t *)&len, sizeof(len));
        hash = hash_u32_partial(hash, (uint8_t *)&len, sizeof(len));
        #endif

        if(hash == 0){

            continue;
        }

        else if(len == 0){

            continue;
        }

        char str[64] = {0};

        if(len >= sizeof(str)){

            len = sizeof(str) - 1;

            #ifdef FX4_COMPILER
            debug_printf("String overflow %d\n", len);
            #else
            log_v_warn_P(PSTR("String overflow %d\n"),len);
            #endif
        }

        #ifdef FX4_COMPILER
        fread(str, 1, len, fp);
        #else
        fs_i16_read(f, (uint8_t *)str, len);
        hash = hash_u32_partial(hash, (uint8_t *)str, len);
        #endif

        #ifdef FX4_COMPILER
        debug_printf("%8x %2d %s\n", hash, len, str);
        #else
        // log_v_debug_P(PSTR("%8x %2d %s\n"), hash, len, str);

        // add string to KVDB
        kvdb_v_set_name(str);

        #endif
    }



    // // run init code:
    // // coroutines and pixel arrays are starting at empty from here.
    // // we run init code (so we can get things like publish, etc)
    // // and after we will load coroutines and pixelarrays.

    // status = vm_run_instructions( vm, -1, 0 );

    // if( status < 0 ){

    //     #ifndef FX4_COMPILER
    //     log_v_error_P( PSTR("VM init failed: %d"), status );
    //     #endif

    //     goto error;
    // }


    // // check if we are loading coroutines:
    // if(header.coroutines_len > 0){

    //     // free any coroutines that were created, since we aren't
    //     // going to run them and will reload the serialized states next
    //     deinit_coroutines(vm);
    //     // note that we don't need to do this for pixel arrays because
    //     // we will overwrite state, but we don't destroy pixel arrays
    //     // during runtime, only on deinit.
    // }

    // now load serialized coroutines, if any
    for(uint32_t i = 0; i < header.coroutines_len; i++){

        coroutine_state_serialized_t serialized = {0};

        #ifdef FX4_COMPILER
        fread(&serialized, 1, sizeof(serialized), fp);
        #else
        fs_i16_read(f, (uint8_t *)&serialized, sizeof(serialized));
        hash = hash_u32_partial(hash, (uint8_t *)&serialized, sizeof(serialized));
        #endif

        if(serialized.index >= MAX_COROUTINES){

            return VM4_STATUS_MAX_COROUTINES;
        }

        coroutine_handle_t coroutine_h = _coroutine_alloc(serialized.locals_count);

        #ifdef FX4_COMPILER

        // deal with CWE-401 with GCC -fanalyzer 
        ASSERT(coroutine_h > 0);

        if(coroutine_h == 0){
        #else
        if(coroutine_h < 0){
        #endif

            return VM4_STATUS_ARRAY_ALLOC_FAIL;
        }

        #ifndef FX4_COMPILER
        log_v_debug_P(PSTR("Load coroutine at: %d"), serialized.index);
        #endif

        vm->coroutines[serialized.index] = coroutine_h;

        #ifdef FX4_COMPILER
        coroutine_state_t *coroutine = (coroutine_state_t *)coroutine_h;
        #else
        coroutine_state_t *coroutine = (coroutine_state_t *)mem2_vp_get_ptr(coroutine_h);
        #endif

        int32_t *coroutine_locals = (int32_t *)(coroutine + 1);

        coroutine->current_pc     = serialized.current_pc;
        coroutine->locals_count   = serialized.locals_count;
        coroutine->tick           = serialized.tick;

        for(uint32_t j = 0; j < serialized.locals_count; j++){

            #ifdef FX4_COMPILER
            fread(coroutine_locals, 1, sizeof(int32_t), fp);
            #else
            fs_i16_read(f, (uint8_t *)coroutine_locals, sizeof(int32_t));
            hash = hash_u32_partial(hash, (uint8_t *)coroutine_locals, sizeof(int32_t));
            #endif

            coroutine_locals++;
        }
    }

    #ifdef FX4_COMPILER
    pixelarray_init();
    #endif

    for(uint32_t i = 0; i < header.pixelarray_len; i++){

        pixel_array_t pixarray = {0};

        #ifdef FX4_COMPILER
        fread(&pixarray, 1, sizeof(pixel_array_t), fp);
        #else
        fs_i16_read(f, (uint8_t *)&pixarray, sizeof(pixel_array_t));
        hash = hash_u32_partial(hash, (uint8_t *)&pixarray, sizeof(pixel_array_t));
        #endif

        pixelarray_add(&pixarray, vm->vm_id);
    }


    #ifdef FX4_COMPILER
    fclose(fp);
    #else
    fs_f_close(f);
    #endif

    #ifdef FX4_COMPILER
    vm->rng_seed = 1234;
    #else

    if(header.rng_seed == 0){

        // init RNG seed to device ID
        uint64_t rng_seed;
        cfg_i8_get(CFG_PARAM_DEVICE_ID, &rng_seed);
        vm->rng_seed = rng_seed;
    }
    else{

        vm->rng_seed = header.rng_seed;
    }

    vm->frame_number = header.frame_number;
    vm->current_tick = header.current_tick;

    
    vm->file_hash = hash;
    #endif

    return VM4_STATUS_OK;

error:
    return status;
}

int vm_lookup_hash(uint32_t hash, char *s, int max_len){

    #ifdef FX4_COMPILER

    char *str = hash_s_get_str(hash);

    if(str == NULL){

        return -1;
    }

    int str_len = strlen(str);

    if(str_len < max_len){

        max_len = str_len;
    }

    strncpy(s, str, max_len);

    #else

    char name[CATBUS_STRING_LEN];

    if(max_len > CATBUS_STRING_LEN){

        max_len = CATBUS_STRING_LEN;
    }

    if(kv_i8_get_name(hash, name) < 0){

        return -1;
    }

    strncpy(s, name, max_len);

    #endif

    return 0;
}

int32_t* vm_get_global(vm_t *vm, uint16_t index){

    int32_t *globals      = (int32_t *)array_get_data(vm->globals_list);

    return &globals[index];
}

int32_t* vm_get_array(vm_t *vm, uint16_t index){

    int32_t *globals      = (int32_t *)array_get_data(vm->globals_list);
    int32_t *array        = (int32_t *)array_get_data(vm->array_list);
    
    array_meta_t *array_meta  = (array_meta_t *)&globals[index];

    return &array[array_meta->array_index + 1];   
}

#endif