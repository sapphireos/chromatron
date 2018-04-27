// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>

#include <string.h>
#include "gfx_lib.h"
#include "random.h"
#include "crc.h"
#include "vm_core.h"
#include "trig.h"
#include "vm_config.h"

#ifdef VM_ENABLE_KV
#include "keyvalue.h"
#include "kvdb.h"
#ifndef ESP8266
#include "catbus.h"
#endif
#endif


static int32_t _vm_i32_sys_call( uint8_t func_id, int32_t *params, uint16_t param_len );

static uint16_t cycles;
static uint8_t call_depth;

static int8_t _vm_i8_run_stream(
    uint8_t *stream,
    uint16_t offset,
    vm_state_t *state,
    int32_t *data ){

#ifdef ESP8266
    static void *opcode_table[] = {
#else
    static void *const opcode_table[] PROGMEM = {
#endif

        &&opcode_mov,	            // 0
        &&opcode_clr,	            // 1
        &&opcode_compeq,	        // 2
        &&opcode_compneq,	        // 3
        &&opcode_compgt,	        // 4
        &&opcode_compgte,	        // 5
        &&opcode_complt,	        // 6
        &&opcode_complte,	        // 7
        &&opcode_and,	            // 8
        &&opcode_or,	            // 9
        &&opcode_add,	            // 10
        &&opcode_sub,	            // 11
        &&opcode_mul,	            // 12
        &&opcode_div,	            // 13
        &&opcode_mod,	            // 14
        &&opcode_jmp,	            // 15
        &&opcode_jmp_if_z,	        // 16
        &&opcode_jmp_if_not_z,	    // 17
        &&opcode_jmp_if_z_dec,	    // 18
        &&opcode_jmp_if_gte,	    // 19
        &&opcode_jmp_if_l_pre_inc,  // 20
        &&opcode_print,	            // 21
        &&opcode_ret,	            // 22
        &&opcode_call,	            // 23
        
        //&&opcode_lta,	            // 24
        //&&opcode_lfa,	            // 25
        //&&opcode_lfa2d,	        // 26
        //&&opcode_lta2d,	        // 27

        &&opcode_trap,              // 24 // index load
        &&opcode_trap,              // 25 // index store
        &&opcode_trap,              // 26
        &&opcode_trap,              // 27

        &&opcode_ltah,	            // 28
        &&opcode_ltas,	            // 29
        &&opcode_ltav,	            // 30
        &&opcode_lfah,	            // 31
        &&opcode_lfas,	            // 32
        &&opcode_lfav,	            // 33
        &&opcode_array_add,	        // 34
        &&opcode_array_sub,	        // 35
        &&opcode_array_mul,	        // 36
        &&opcode_array_div,	        // 37
        &&opcode_array_mod,	        // 38
        &&opcode_array_mov,	        // 39
        &&opcode_rand,	            // 40
        &&opcode_assert,	        // 41
        &&opcode_halt,	            // 42
        &&opcode_is_fading,	        // 43
        &&opcode_lib_call,	        // 44
        &&opcode_sys_call,	        // 45
        &&opcode_trap,	            // 46
        &&opcode_trap,	            // 47
        &&opcode_trap,	            // 48
        &&opcode_lfahsf,            // 49
        &&opcode_lfavf,	            // 50
        &&opcode_ltahsf,            // 51
        &&opcode_ltavf,	            // 52
        &&opcode_trap,	            // 53
        &&opcode_obj_load,	        // 54
        &&opcode_obj_store,	        // 55
        &&opcode_not,	            // 56
        &&opcode_db_load,	        // 57
        &&opcode_db_store,	        // 58
        &&opcode_db_idx_load,	    // 59
        &&opcode_db_idx_store,	    // 60
        &&opcode_db_len,	        // 61
        &&opcode_trap,	            // 62
        &&opcode_trap,	            // 63
        &&opcode_trap,	            // 64
        &&opcode_trap,	            // 65
        &&opcode_trap,	            // 66
        &&opcode_trap,	            // 67
        &&opcode_trap,	            // 68
        &&opcode_trap,	            // 69
        &&opcode_trap,	            // 70
        &&opcode_trap,	            // 71
        &&opcode_trap,	            // 72
        &&opcode_trap,	            // 73
        &&opcode_trap,	            // 74
        &&opcode_trap,	            // 75
        &&opcode_trap,	            // 76
        &&opcode_trap,	            // 77
        &&opcode_trap,	            // 78
        &&opcode_trap,	            // 79
        &&opcode_trap,	            // 80
        &&opcode_trap,	            // 81
        &&opcode_trap,	            // 82
        &&opcode_trap,	            // 83
        &&opcode_trap,	            // 84
        &&opcode_trap,	            // 85
        &&opcode_trap,	            // 86
        &&opcode_trap,	            // 87
        &&opcode_trap,	            // 88
        &&opcode_trap,	            // 89
        &&opcode_trap,	            // 90
        &&opcode_trap,	            // 91
        &&opcode_trap,	            // 92
        &&opcode_trap,	            // 93
        &&opcode_trap,	            // 94
        &&opcode_trap,	            // 95
        &&opcode_trap,	            // 96
        &&opcode_trap,	            // 97
        &&opcode_trap,	            // 98
        &&opcode_trap,	            // 99
        &&opcode_trap,	            // 100
        &&opcode_trap,	            // 101
        &&opcode_trap,	            // 102
        &&opcode_trap,	            // 103
        &&opcode_trap,	            // 104
        &&opcode_trap,	            // 105
        &&opcode_trap,	            // 106
        &&opcode_trap,	            // 107
        &&opcode_trap,	            // 108
        &&opcode_trap,	            // 109
        &&opcode_trap,	            // 110
        &&opcode_trap,	            // 111
        &&opcode_trap,	            // 112
        &&opcode_trap,	            // 113
        &&opcode_trap,	            // 114
        &&opcode_trap,	            // 115
        &&opcode_trap,	            // 116
        &&opcode_trap,	            // 117
        &&opcode_trap,	            // 118
        &&opcode_trap,	            // 119
        &&opcode_trap,	            // 120
        &&opcode_trap,	            // 121
        &&opcode_trap,	            // 122
        &&opcode_trap,	            // 123
        &&opcode_trap,	            // 124
        &&opcode_trap,	            // 125
        &&opcode_trap,	            // 126
        &&opcode_trap,	            // 127
        &&opcode_trap,	            // 128
        &&opcode_trap,	            // 129
        &&opcode_trap,	            // 130
        &&opcode_trap,	            // 131
        &&opcode_trap,	            // 132
        &&opcode_trap,	            // 133
        &&opcode_trap,	            // 134
        &&opcode_trap,	            // 135
        &&opcode_trap,	            // 136
        &&opcode_trap,	            // 137
        &&opcode_trap,	            // 138
        &&opcode_trap,	            // 139
        &&opcode_trap,	            // 140
        &&opcode_trap,	            // 141
        &&opcode_trap,	            // 142
        &&opcode_trap,	            // 143
        &&opcode_trap,	            // 144
        &&opcode_trap,	            // 145
        &&opcode_trap,	            // 146
        &&opcode_trap,	            // 147
        &&opcode_trap,	            // 148
        &&opcode_trap,	            // 149
        &&opcode_trap,	            // 150
        &&opcode_trap,	            // 151
        &&opcode_trap,	            // 152
        &&opcode_trap,	            // 153
        &&opcode_trap,	            // 154
        &&opcode_trap,	            // 155
        &&opcode_trap,	            // 156
        &&opcode_trap,	            // 157
        &&opcode_trap,	            // 158
        &&opcode_trap,	            // 159
        &&opcode_trap,	            // 160
        &&opcode_trap,	            // 161
        &&opcode_trap,	            // 162
        &&opcode_trap,	            // 163
        &&opcode_trap,	            // 164
        &&opcode_trap,	            // 165
        &&opcode_trap,	            // 166
        &&opcode_trap,	            // 167
        &&opcode_trap,	            // 168
        &&opcode_trap,	            // 169
        &&opcode_trap,	            // 170
        &&opcode_trap,	            // 171
        &&opcode_trap,	            // 172
        &&opcode_trap,	            // 173
        &&opcode_trap,	            // 174
        &&opcode_trap,	            // 175
        &&opcode_trap,	            // 176
        &&opcode_trap,	            // 177
        &&opcode_trap,	            // 178
        &&opcode_trap,	            // 179
        &&opcode_trap,	            // 180
        &&opcode_trap,	            // 181
        &&opcode_trap,	            // 182
        &&opcode_trap,	            // 183
        &&opcode_trap,	            // 184
        &&opcode_trap,	            // 185
        &&opcode_trap,	            // 186
        &&opcode_trap,	            // 187
        &&opcode_trap,	            // 188
        &&opcode_trap,	            // 189
        &&opcode_trap,	            // 190
        &&opcode_trap,	            // 191
        &&opcode_trap,	            // 192
        &&opcode_trap,	            // 193
        &&opcode_trap,	            // 194
        &&opcode_trap,	            // 195
        &&opcode_trap,	            // 196
        &&opcode_trap,	            // 197
        &&opcode_trap,	            // 198
        &&opcode_trap,	            // 199
        &&opcode_trap,	            // 200
        &&opcode_trap,	            // 201
        &&opcode_trap,	            // 202
        &&opcode_trap,	            // 203
        &&opcode_trap,	            // 204
        &&opcode_trap,	            // 205
        &&opcode_trap,	            // 206
        &&opcode_trap,	            // 207
        &&opcode_trap,	            // 208
        &&opcode_trap,	            // 209
        &&opcode_trap,	            // 210
        &&opcode_trap,	            // 211
        &&opcode_trap,	            // 212
        &&opcode_trap,	            // 213
        &&opcode_trap,	            // 214
        &&opcode_trap,	            // 215
        &&opcode_trap,	            // 216
        &&opcode_trap,	            // 217
        &&opcode_trap,	            // 218
        &&opcode_trap,	            // 219
        &&opcode_trap,	            // 220
        &&opcode_trap,	            // 221
        &&opcode_trap,	            // 222
        &&opcode_trap,	            // 223
        &&opcode_trap,	            // 224
        &&opcode_trap,	            // 225
        &&opcode_trap,	            // 226
        &&opcode_trap,	            // 227
        &&opcode_trap,	            // 228
        &&opcode_trap,	            // 229
        &&opcode_trap,	            // 230
        &&opcode_trap,	            // 231
        &&opcode_trap,	            // 232
        &&opcode_trap,	            // 233
        &&opcode_trap,	            // 234
        &&opcode_trap,	            // 235
        &&opcode_trap,	            // 236
        &&opcode_trap,	            // 237
        &&opcode_trap,	            // 238
        &&opcode_trap,	            // 239
        &&opcode_trap,	            // 240
        &&opcode_trap,	            // 241
        &&opcode_trap,	            // 242
        &&opcode_trap,	            // 243
        &&opcode_trap,	            // 244
        &&opcode_trap,	            // 245
        &&opcode_trap,	            // 246
        &&opcode_trap,	            // 247
        &&opcode_trap,	            // 248
        &&opcode_trap,	            // 249
        &&opcode_trap,	            // 250
        &&opcode_trap,	            // 251
        &&opcode_trap,	            // 252
        &&opcode_trap,	            // 253
        &&opcode_trap,	            // 254
        &&opcode_trap,	            // 255
    };

    uint8_t *code = (uint8_t *)( stream + state->code_start );
    uint8_t *pc = code + offset;
    uint8_t opcode, dest, src, index_x, index_y, result, op1_addr, op2_addr, obj, attr, param_len, func_id;
    int32_t op1, op2, index, size;
    //index_x32, index_y32, size_x32, size_y32
    int32_t params[8];
    uint16_t addr;
    catbus_hash_t32 hash;


    call_depth++;

dispatch:
    cycles++;

    if( cycles > VM_MAX_CYCLES ){

        call_depth--;
        return VM_STATUS_ERR_MAX_CYCLES;
    }

    opcode = *pc++;

#ifdef ESP8266
    goto *opcode_table[opcode];
#else
    void *target = (void*)pgm_read_word( &opcode_table[opcode] );
    goto *target;
#endif

opcode_mov:
    dest = *pc++;
    src  = *pc++;

    data[dest] = data[src];

    goto dispatch;


opcode_clr:

    dest = *pc++;

    data[dest] = 0;

    goto dispatch;


opcode_not:
    
    dest = *pc++;
    src = *pc++;

    if( data[src] == 0 ){

        data[dest] = 1;
    }
    else{
        
        data[dest] = 0;
    }

    goto dispatch;


opcode_compeq:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 == op2;

    goto dispatch;

opcode_compneq:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 != op2;

    goto dispatch;


opcode_compgt:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 > op2;

    goto dispatch;


opcode_compgte:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 >= op2;

    goto dispatch;


opcode_complt:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 < op2;

    goto dispatch;


opcode_complte:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 <= op2;

    goto dispatch;



opcode_and:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 && op2;

    goto dispatch;

opcode_or:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 || op2;

    goto dispatch;


opcode_add:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 + op2;

    goto dispatch;


opcode_sub:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 - op2;

    goto dispatch;


opcode_mul:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    data[result] = op1 * op2;

    goto dispatch;

opcode_div:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    if( op2 != 0 ){

        data[result] = op1 / op2;
    }
    else{

        data[result] = 0;        
    }

    goto dispatch;


opcode_mod:

    result = *pc++;
    op1  = data[*pc++];
    op2  = data[*pc++];

    if( op2 != 0 ){

        data[result] = op1 % op2;
    }
    else{

        data[result] = 0;        
    }

    goto dispatch;


opcode_jmp:

    addr = *pc++;
    addr += ( *pc ) << 8;

    pc = code + addr;

    goto dispatch;


opcode_jmp_if_z:

    op1_addr = *pc++;

    addr = *pc++;
    addr += ( *pc++ ) << 8;

    if( data[op1_addr] == 0 ){

        pc = code + addr;
    }

    goto dispatch;


opcode_jmp_if_not_z:

    op1_addr = *pc++;

    addr = *pc++;
    addr += ( *pc++ ) << 8;

    if( data[op1_addr] != 0 ){

        pc = code + addr;
    }

    goto dispatch;


opcode_jmp_if_z_dec:

    op1_addr = *pc++;

    addr = *pc++;
    addr += ( *pc++ ) << 8;

    if( data[op1_addr] == 0 ){

        pc = code + addr;
    }
    else{

        data[op1_addr]--;
    }

    goto dispatch;


opcode_jmp_if_gte:

    op1_addr = *pc++;
    op2_addr = *pc++;

    addr = *pc++;
    addr += ( *pc++ ) << 8;

    if( data[op1_addr] >= data[op2_addr] ){

        pc = code + addr;
    }

    goto dispatch;


opcode_jmp_if_l_pre_inc:

    op1_addr = *pc++;
    op2_addr = *pc++;

    data[op1_addr]++;

    if( data[op1_addr] < data[op2_addr] ){

        addr = *pc++;
        addr += ( *pc ) << 8;

        pc = code + addr;
    }
    else{

        pc += 2;
    }

    goto dispatch;


opcode_print:

    src = *pc++;
    op1  = data[src];

    // log_v_debug_P( PSTR("%d = %d"), src, op1 );

    goto dispatch;


opcode_ret:
    op1_addr = *pc++;
    data[RETURN_VAL_ADDR] = data[op1_addr];

    call_depth--;
    return VM_STATUS_OK;


opcode_call:
    addr = *pc++;
    addr += ( *pc++ ) << 8;

    // call function, by recursively calling into VM
    int8_t status = _vm_i8_run_stream( stream, addr, state, data );
    if( status < 0 ){

        call_depth--;
        return status;
    }

    goto dispatch;


// opcode_lta:

//     dest = *pc++;
//     src  = *pc++;
//     index = data[*pc++];
//     size = data[*pc++];

//     index %= size;

//     data[dest + index] = data[src];

//     goto dispatch;


// opcode_lfa:

//     dest = *pc++;
//     src  = *pc++;
//     index = data[*pc++];
//     size = data[*pc++];

//     index %= size;

//     data[dest] = data[src + index];

//     goto dispatch;


// opcode_lfa2d:

//     dest = *pc++;
//     src  = *pc++;
//     index_x32 = data[*pc++];
//     index_y32 = data[*pc++];
//     size_x32 = data[*pc++];
//     size_y32 = data[*pc++];

//     index_x32 %= size_x32;
//     index_y32 %= size_y32;

//     data[dest] = data[src + index_x32 + (index_y32 * size_x32)];

//     goto dispatch;

// opcode_lta2d:

//     dest = *pc++;
//     src  = *pc++;
//     index_x32 = data[*pc++];
//     index_y32 = data[*pc++];
//     size_x32 = data[*pc++];
//     size_y32 = data[*pc++];

//     index_x32 %= size_x32;
//     index_y32 %= size_y32;

//     data[dest + index_x32 + (index_y32 * size_x32)] = data[src];

//     goto dispatch;


opcode_ltah:

    src  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX

    op1 = data[src];
    // wraparound to 16 bit range.
    // this makes it easy to run a circular rainbow
    op1 %= 65536;

    gfx_v_set_hue( op1, data[index_x], data[index_y], obj );
    #endif

    goto dispatch;


opcode_ltas:

    src  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX

    op1 = data[src];
    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound
    if( op1 > 65535 ){

        op1 = 65535;
    }
    else if( op1 < 0 ){

        op1 = 0;
    }

    gfx_v_set_sat( op1, data[index_x], data[index_y], obj );

    #endif

    goto dispatch;


opcode_ltav:

    src  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX

    op1 = data[src];
    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound
    if( op1 > 65535 ){

        op1 = 65535;
    }
    else if( op1 < 0 ){

        op1 = 0;
    }

    gfx_v_set_val( op1, data[index_x], data[index_y], obj );

    #endif

    goto dispatch;

opcode_ltahsf:

    src  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX

    op1 = data[src];
    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound

    if( op1 > 65535 ){

        op1 = 65535;
    }
    else if( op1 < 0 ){

        op1 = 0;
    }

    gfx_v_set_hs_fade( op1, data[index_x], data[index_y], obj );
    #endif

    goto dispatch;

opcode_ltavf:

    src  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    
    op1 = data[src];

    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound

    if( op1 > 65535 ){

        op1 = 65535;
    }
    else if( op1 < 0 ){

        op1 = 0;
    }
    
    gfx_v_set_v_fade( op1, data[index_x], data[index_y], obj );
    #endif

    goto dispatch;

opcode_lfah:

    dest  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_hue( data[index_x], data[index_y], obj );
    #endif

    goto dispatch;


opcode_lfas:

    dest  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_sat( data[index_x], data[index_y], obj );
    #endif

    goto dispatch;


opcode_lfav:

    dest  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_val( data[index_x], data[index_y], obj );
    #endif

    goto dispatch;

opcode_lfahsf:

    dest  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_hs_fade( data[index_x], data[index_y], obj );
    #endif

    goto dispatch;

opcode_lfavf:

    dest  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_v_fade( data[index_x], data[index_y], obj );
    #endif

    goto dispatch;

opcode_array_add:
    
    obj = *pc++;
    dest = *pc++;
    attr = *pc++;
    op1 = data[*pc++];
    size = data[*pc++];

    if( obj == ARRAY_OBJ_TYPE ){

        for( uint16_t i = 0; i < size; i++ ){

            data[dest + i] += op1;
        }
    }
    else if( obj == PIX_OBJ_TYPE ){

        #ifdef VM_ENABLE_GFX
        gfx_v_array_add( dest, attr, op1 );
        #endif
    }

    goto dispatch;


opcode_array_sub:

    obj = *pc++;
    dest = *pc++;
    attr = *pc++;
    op1 = data[*pc++];
    size = data[*pc++];

    if( obj == ARRAY_OBJ_TYPE ){

        for( uint16_t i = 0; i < size; i++ ){

            data[dest + i] -= op1;
        }
    }
    else if( obj == PIX_OBJ_TYPE ){

        #ifdef VM_ENABLE_GFX
        gfx_v_array_sub( dest, attr, op1 );
        #endif
    }

    goto dispatch;


opcode_array_mul:

    obj = *pc++;
    dest = *pc++;
    attr = *pc++;
    op1 = data[*pc++];
    size = data[*pc++];

    if( obj == ARRAY_OBJ_TYPE ){

        for( uint16_t i = 0; i < size; i++ ){

            data[dest + i] *= op1;
        }
    }
    else if( obj == PIX_OBJ_TYPE ){

        #ifdef VM_ENABLE_GFX
        gfx_v_array_mul( dest, attr, op1 );
        #endif
    }

    goto dispatch;


opcode_array_div:

    obj = *pc++;
    dest = *pc++;
    attr = *pc++;
    op1 = data[*pc++];
    size = data[*pc++];

    if( obj == ARRAY_OBJ_TYPE ){

        for( uint16_t i = 0; i < size; i++ ){

            data[dest + i] /= op1;
        }
    }
    else if( obj == PIX_OBJ_TYPE ){

        #ifdef VM_ENABLE_GFX
        gfx_v_array_div( dest, attr, op1 );
        #endif
    }

    goto dispatch;


opcode_array_mod:

    obj = *pc++;
    dest = *pc++;
    attr = *pc++;
    op1 = data[*pc++];
    size = data[*pc++];

    if( obj == ARRAY_OBJ_TYPE ){

        for( uint16_t i = 0; i < size; i++ ){

            data[dest + i] %= op1;
        }
    }
    else if( obj == PIX_OBJ_TYPE ){

        #ifdef VM_ENABLE_GFX
        gfx_v_array_mod( dest, attr, op1 );
        #endif
    }
    goto dispatch;


opcode_array_mov:

    obj = *pc++;
    dest = *pc++;
    attr = *pc++;
    op1 = data[*pc++];
    size = data[*pc++];

    if( obj == ARRAY_OBJ_TYPE ){

        for( uint16_t i = 0; i < size; i++ ){

            data[dest + i] = op1;
        }
    }
    else if( obj == PIX_OBJ_TYPE ){

        #ifdef VM_ENABLE_GFX
        gfx_v_array_move( dest, attr, op1 );
        #endif
    }

    goto dispatch;


opcode_rand:

    dest = *pc++;
    op1_addr = *pc++;
    op2_addr = *pc++;

    uint16_t val;
    uint16_t diff = data[op2_addr] - data[op1_addr];

    // check for divide by 0!
    if( diff == 0 ){

        val = 0;
    }
    else{

        val = rnd_u16_get_int_with_seed( &state->rng_seed ) % diff;
    }

    data[dest] = val + data[op1_addr];

    goto dispatch;


opcode_lib_call:
    func_id     = *pc++;
    dest        = *pc++;
    param_len   = *pc++;    

    for( uint32_t i = 0; i < param_len; i++ ){

        params[i] = data[*pc];
        pc++;
    }

    data[dest] = _vm_i32_sys_call( func_id, params, param_len );

    goto dispatch;

opcode_sys_call:
    hash        =  (catbus_hash_t32)(*pc++) << 24;
    hash        |= (catbus_hash_t32)(*pc++) << 16;
    hash        |= (catbus_hash_t32)(*pc++) << 8;
    hash        |= (catbus_hash_t32)(*pc++) << 0;

    dest        = *pc++;
    param_len   = *pc++;    

    for( uint32_t i = 0; i < param_len; i++ ){

        params[i] = data[*pc];
        pc++;
    }

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_i32_lib_call( hash, params, param_len );
    #else   
    data[dest] = 0;
    #endif

    goto dispatch;


opcode_assert:
    op1 = data[*pc++];

    if( op1 == FALSE ){

        #ifndef VM_TARGET_ESP
        // log_v_debug_P( PSTR("VM assertion failed") );
        #endif
        call_depth--;
        return VM_STATUS_ASSERT;
    }

    goto dispatch;

opcode_halt:
    
    call_depth--;
    return VM_STATUS_HALT;

    goto dispatch;


opcode_is_fading:
    dest  = *pc++;
    index_x = *pc++;
    index_y  = *pc++;
    obj = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_is_fading( data[index_x], data[index_y], obj );
    #endif

    goto dispatch;

opcode_obj_load:
    obj         = *pc++;
    attr        = *pc++;
    op1_addr    = *pc++;
    dest        = *pc++;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_i32_get_obj_attr( obj, attr, op1_addr );
    #endif

    goto dispatch;

opcode_obj_store:
    obj         = *pc++;
    attr        = *pc++;
    op1_addr    = *pc++;
    dest        = *pc++;

    #ifdef VM_ENABLE_GFX

    // object store is a no-op for now

    #endif

    goto dispatch;

opcode_db_load:
    hash        =  (catbus_hash_t32)(*pc++) << 24;
    hash        |= (catbus_hash_t32)(*pc++) << 16;
    hash        |= (catbus_hash_t32)(*pc++) << 8;
    hash        |= (catbus_hash_t32)(*pc++) << 0;

    dest        = *pc++;

    #ifdef VM_ENABLE_KV
    #ifdef ESP8266
    if( kvdb_i8_get( hash, CATBUS_TYPE_INT32, &data[dest], sizeof(data[dest]) ) < 0 ){

        data[dest] = 0;        
    }
    #else
    if( catbus_i8_get( hash, CATBUS_TYPE_INT32, &data[dest] ) < 0 ){

        data[dest] = 0;        
    }
    #endif
    #endif

    goto dispatch;

opcode_db_store:
    hash        =  (catbus_hash_t32)(*pc++) << 24;
    hash        |= (catbus_hash_t32)(*pc++) << 16;
    hash        |= (catbus_hash_t32)(*pc++) << 8;
    hash        |= (catbus_hash_t32)(*pc++) << 0;

    op1_addr    = *pc++;

    #ifdef VM_ENABLE_KV
    #ifdef ESP8266
    kvdb_i8_set( hash, CATBUS_TYPE_INT32, &data[op1_addr], sizeof(data[op1_addr]) );
    kvdb_i8_publish( hash );
    #else
    catbus_i8_set( hash, CATBUS_TYPE_INT32, &data[op1_addr] );
    catbus_i8_publish( hash );
    #endif
    #endif

    goto dispatch;

opcode_db_idx_load:
    hash        =  (catbus_hash_t32)(*pc++) << 24;
    hash        |= (catbus_hash_t32)(*pc++) << 16;
    hash        |= (catbus_hash_t32)(*pc++) << 8;
    hash        |= (catbus_hash_t32)(*pc++) << 0;

    dest        = *pc++;
    index       = data[*pc++];

    #ifdef VM_ENABLE_KV
    #ifdef ESP8266
    if( kvdb_i8_array_get( hash, CATBUS_TYPE_INT32, index, &data[dest], sizeof(data[dest]) ) < 0 ){

        data[dest] = 0;        
    }
    #else
    if( catbus_i8_array_get( hash, CATBUS_TYPE_INT32, index, 1, &data[dest] ) < 0 ){

        data[dest] = 0;        
    }
    #endif
    #endif

    goto dispatch;

opcode_db_idx_store:
    hash        =  (catbus_hash_t32)(*pc++) << 24;
    hash        |= (catbus_hash_t32)(*pc++) << 16;
    hash        |= (catbus_hash_t32)(*pc++) << 8;
    hash        |= (catbus_hash_t32)(*pc++) << 0;

    op1_addr    = *pc++;
    index       = data[*pc++];

    #ifdef VM_ENABLE_KV
    #ifdef ESP8266
    kvdb_i8_array_set( hash, CATBUS_TYPE_INT32, index, &data[op1_addr], sizeof(data[op1_addr]) );
    kvdb_i8_publish( hash );
    #else
    catbus_i8_array_set( hash, CATBUS_TYPE_INT32, index, 1, &data[op1_addr] );
    catbus_i8_publish( hash );
    #endif
    #endif

    goto dispatch;

opcode_db_len:
    hash        =  (catbus_hash_t32)(*pc++) << 24;
    hash        |= (catbus_hash_t32)(*pc++) << 16;
    hash        |= (catbus_hash_t32)(*pc++) << 8;
    hash        |= (catbus_hash_t32)(*pc++) << 0;

    dest        = *pc++;

    #ifdef VM_ENABLE_KV
    catbus_meta_t meta;
    
    if( kv_i8_get_meta( hash, &meta ) < 0 ){

        data[dest] = 0;        
    }
    else{

        data[dest] = meta.count + 1;
    }
    #else
    data[dest] = 0;        
    #endif

    goto dispatch;

opcode_trap:
    call_depth--;
    return VM_STATUS_TRAP;
}


int8_t vm_i8_run(
    uint8_t *stream,
    uint16_t offset,
    vm_state_t *state ){

    cycles = 0;
    call_depth = 0;

    int32_t *data = (int32_t *)( stream + state->data_start );

    int8_t status = _vm_i8_run_stream( stream, offset, state, data );

    if( cycles > state->max_cycles ){

        state->max_cycles = cycles;        
    }

    return status;
}


int8_t vm_i8_run_init(
    uint8_t *stream,
    vm_state_t *state ){

    state->frame_number = 0;

    return vm_i8_run( stream, state->init_start, state );
}


int8_t vm_i8_run_loop(
    uint8_t *stream,
    vm_state_t *state ){

    cycles = 0;
    call_depth = 0;
    state->frame_number++;

    return vm_i8_run( stream, state->loop_start, state );
}


int32_t vm_i32_get_data( 
    uint8_t *stream,
    vm_state_t *state,
    uint8_t addr ){

    // bounds check
    if( addr >= state->data_count ){

        return 0;
    }

    int32_t *data_table = (int32_t *)( stream + state->data_start );

    return data_table[addr];
}

void vm_v_get_data_multi( 
    uint8_t *stream,
    vm_state_t *state,
    uint8_t addr, 
    uint16_t len,
    int32_t *dest ){

    // bounds check
    if( ( addr + len ) > state->data_count ){

        return;
    }

    int32_t *data_table = (int32_t *)( stream + state->data_start );

    while( len > 0 ){

        *dest = data_table[addr];

        dest++;
        addr++;
        len--;
    }
}

void vm_v_set_data( 
    uint8_t *stream,
    vm_state_t *state,
    uint8_t addr, 
    int32_t data ){

    // bounds check
    if( addr >= state->data_count ){

        return;
    }

    int32_t *data_table = (int32_t *)( stream + state->data_start );

    data_table[addr] = data;
}


int8_t vm_i8_load_program(
    uint8_t flags,
    uint8_t *stream,
    uint16_t len,
    vm_state_t *state ){

    memset( state, 0, sizeof(vm_state_t) );

    if( ( flags & VM_LOAD_FLAGS_CHECK_HEADER ) == 0 ){

        // verify crc
        if( crc_u16_block( stream, len ) != 0 ){

            return VM_STATUS_ERR_BAD_CRC;
        }
    }

    vm_program_header_t *prog_header = (vm_program_header_t *)stream;

    if( prog_header->file_magic != FILE_MAGIC ){

        return VM_STATUS_ERR_BAD_FILE_MAGIC;
    }

    // verify header
    if( prog_header->prog_magic != PROGRAM_MAGIC ){

        return VM_STATUS_ERR_BAD_PROG_MAGIC;
    }

    if( prog_header->isa_version != VM_ISA_VERSION ){

        return VM_STATUS_ERR_INVALID_ISA;
    }

    if( ( sizeof(vm_program_header_t) % 4 ) != 0 ){

        return VM_STATUS_HEADER_MISALIGN;
    }

    state->init_start = prog_header->init_start;
    state->loop_start = prog_header->loop_start;

    uint16_t obj_start = sizeof(vm_program_header_t);

    state->read_keys_count = prog_header->read_keys_len / sizeof(uint32_t);
    state->read_keys_start = obj_start;
    obj_start += prog_header->read_keys_len;

    if( ( state->read_keys_start % 4 ) != 0 ){

        return VM_STATUS_READ_KEYS_MISALIGN;
    }

    state->write_keys_count = prog_header->write_keys_len / sizeof(uint32_t);
    state->write_keys_start = obj_start;
    obj_start += prog_header->write_keys_len;

    if( ( state->write_keys_start % 4 ) != 0 ){

        return VM_STATUS_WRITE_KEYS_MISALIGN;
    }

    state->publish_count = prog_header->publish_len / sizeof(vm_publish_t);
    state->publish_start = obj_start;
    obj_start += prog_header->publish_len;

    if( ( state->publish_start % 4 ) != 0 ){

        return VM_STATUS_PUBLISH_VARS_MISALIGN;
    }

    state->pix_obj_count = prog_header->pix_obj_len / sizeof(gfx_pixel_array_t);
    state->pix_obj_start = obj_start;
    obj_start += prog_header->pix_obj_len;

    if( ( state->pix_obj_start % 4 ) != 0 ){

        return VM_STATUS_PIXEL_MISALIGN;
    }

    if( ( flags & VM_LOAD_FLAGS_CHECK_HEADER ) != 0 ){

        return VM_STATUS_OK;
    }

    state->code_start = obj_start;

    // Not assigning the variable this way - see note below on data_magic.
    // uint32_t code_magic = *(uint32_t *)( stream + *code_start );

    uint32_t code_magic = 0;
    memcpy( (uint8_t *)&code_magic, stream + state->code_start, sizeof(code_magic) );

    if( code_magic != CODE_MAGIC ){

        return VM_STATUS_ERR_BAD_CODE_MAGIC;
    }

    state->code_start += sizeof(uint32_t);

    state->data_start = state->code_start + prog_header->code_len;
    state->data_len = prog_header->data_len;
    state->data_count = state->data_len / DATA_LEN;

    // The Xtensa CPU in the ESP8266 will throw an alignment exception 9
    // here.
    // So, intead, we'll memcpy into the 32 bit var instead.
    // uint32_t data_magic = *(uint32_t *)( stream + *data_start );
    uint32_t data_magic = 0;
    memcpy( (uint8_t *)&data_magic, stream + state->data_start, sizeof(data_magic) );

    // we do the same above on code_magic, although the exception was not seen there.

    if( data_magic != DATA_MAGIC ){

        return VM_STATUS_ERR_BAD_DATA_MAGIC;
    }

    state->data_start += sizeof(uint32_t);

    // init RNG seed
    state->rng_seed = 1;

    state->frame_number = 0;

    return VM_STATUS_OK;
}


int8_t vm_i8_eval( uint8_t *stream, int32_t *data, int32_t *result ){

    // this is broken

    return -1;
    // uint64_t rng_seed = rnd_u64_get_seed();

    // cycles = 0;

    // int8_t status = _vm_i8_run_stream( stream, 0, state, data );

    // rnd_v_seed( rng_seed );

    // *result = data[RETURN_VAL_ADDR];

    // return status;
}


static int32_t _vm_i32_sys_call( uint8_t func_id, int32_t *params, uint16_t param_len ){

    return -1;    
}

