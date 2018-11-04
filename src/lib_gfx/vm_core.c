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
#include "vm_core.h"
#include "trig.h"
#include "hash.h"
#include "vm_config.h"
#include "vm_lib.h"
#include "catbus_types.h"
#include "datetime.h"

#ifdef VM_ENABLE_KV
#include "keyvalue.h"
#include "kvdb.h"
#ifndef ESP8266
#include "catbus.h"
#endif
#endif

#ifdef ESP8266 // very slight speed boost using 32 bit numbers on Xtensa
static uint32_t cycles;
#else
static uint16_t cycles;
#endif

static int8_t _vm_i8_run_stream(
    uint8_t *stream,
    uint16_t func_addr,
    uint16_t pc_offset,
    vm_state_t *state,
    int32_t *data ){

#ifdef ESP8266
    static void *opcode_table[] = {
#else
    static void *const opcode_table[] PROGMEM = {
#endif

        &&opcode_trap,              // 0

        &&opcode_mov,	            // 1
        &&opcode_clr,	            // 2
        
        &&opcode_not,	            // 3
        
        &&opcode_compeq,            // 4
        &&opcode_compneq,	        // 5
        &&opcode_compgt,	        // 6
        &&opcode_compgte,	        // 7
        &&opcode_complt,	        // 8
        &&opcode_complte,	        // 9
        &&opcode_and,	            // 10
        &&opcode_or,	            // 11
        &&opcode_add,	            // 12
        &&opcode_sub,	            // 13
        &&opcode_mul,	            // 14
        &&opcode_div,	            // 15
        &&opcode_mod,	            // 16

        &&opcode_f16_compeq,        // 17
        &&opcode_f16_compneq,       // 18
        &&opcode_f16_compgt,        // 19
        &&opcode_f16_compgte,       // 20
        &&opcode_f16_complt,        // 21
        &&opcode_f16_complte,       // 22
        &&opcode_f16_and,           // 23
        &&opcode_f16_or,            // 24
        &&opcode_f16_add,           // 25
        &&opcode_f16_sub,           // 26
        &&opcode_f16_mul,           // 27
        &&opcode_f16_div,           // 28
        &&opcode_f16_mod,           // 29

        &&opcode_jmp,	            // 30
        &&opcode_jmp_if_z,	        // 31
        &&opcode_jmp_if_not_z,	    // 32
        &&opcode_jmp_if_l_pre_inc,  // 33

        &&opcode_ret,	            // 34
        &&opcode_call,	            // 35
        &&opcode_lcall,             // 36
        &&opcode_dbcall,            // 37

        &&opcode_index,             // 38
        &&opcode_load_indirect,     // 39
        &&opcode_store_indirect,    // 40

        &&opcode_assert,            // 41
        &&opcode_halt,              // 42

        &&opcode_vmov,              // 43
        &&opcode_vadd,              // 44
        &&opcode_vsub,              // 45
        &&opcode_vmul,              // 46
        &&opcode_vdiv,              // 47
        &&opcode_vmod,              // 48

        &&opcode_pmov,              // 49
        &&opcode_padd,              // 50
        &&opcode_psub,              // 51
        &&opcode_pmul,              // 52
        &&opcode_pdiv,              // 53
        &&opcode_pmod,              // 54

        &&opcode_pstore_hue,        // 55
        &&opcode_pstore_sat,        // 56
        &&opcode_pstore_val,        // 57
        &&opcode_pstore_vfade,      // 58
        &&opcode_pstore_hsfade,     // 59

        &&opcode_pload_hue,         // 60
        &&opcode_pload_sat,         // 61
        &&opcode_pload_val,         // 62
        &&opcode_pload_vfade,       // 63
        &&opcode_pload_hsfade,      // 64

        &&opcode_db_store,          // 65
        &&opcode_db_load,           // 66

        &&opcode_conv_i32_to_f16,   // 67
        &&opcode_conv_f16_to_i32,   // 68

        &&opcode_is_v_fading,	    // 69
        &&opcode_is_hs_fading,	    // 70

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

    uint8_t *code = stream + state->code_start;
    uint8_t *pc = code + func_addr + pc_offset;
    uint8_t opcode;

    uint16_t dest;
    uint16_t src;
    uint16_t op1;
    uint16_t op2;
    uint16_t call_target;
    uint16_t call_param;
    uint16_t call_arg;
    uint8_t call_param_len;
    uint16_t result;
    uint16_t base_addr;
    uint16_t index;
    uint16_t count;
    uint16_t stride;
    uint16_t temp;
    uint16_t len;
    uint8_t type;
    uint8_t array;
    uint8_t attr;
    catbus_hash_t32 hash;
    catbus_hash_t32 db_hash;
    uint16_t index_x;
    uint16_t index_y;

    #ifdef VM_ENABLE_GFX
    int32_t value_i32;
    #endif

    uint8_t *call_stack[VM_MAX_CALL_DEPTH];
    uint8_t call_depth = 0;

    // uint8_t opcode, dest, src, index_x, index_y, result, op1_addr, op2_addr, obj, attr, param_len, func_id;
    // int32_t op1, op2, index, base, ary_stride, ary_length, ary_addr;
    // bool yield;
    int32_t params[8];
    int32_t indexes[8];
    // uint16_t addr;


    #ifdef ESP8266
        #define DISPATCH cycles--; \
                         if( cycles == 0 ){ \
                            return VM_STATUS_ERR_MAX_CYCLES; \
                        } \
                        opcode = *pc++; \
                        goto *opcode_table[opcode]


        DISPATCH;

    #else
        #define DISPATCH goto dispatch
        dispatch:
            cycles--;

            if( cycles == 0 ){

                return VM_STATUS_ERR_MAX_CYCLES;
            }

            opcode = *pc++;

        #ifdef ESP8266
            goto *opcode_table[opcode];
        #else
            void *target = (void*)pgm_read_word( &opcode_table[opcode] );
            goto *target;
        #endif

    #endif


opcode_mov:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    data[dest] = data[src];

    DISPATCH;


opcode_clr:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    
    data[dest] = 0;

    DISPATCH;


opcode_not:    
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    if( data[src] == 0 ){

        data[dest] = 1;
    }
    else{
        
        data[dest] = 0;
    }

    DISPATCH;


opcode_compeq:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] == data[op2];

    DISPATCH;


opcode_compneq:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] != data[op2];

    DISPATCH;


opcode_compgt:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] > data[op2];

    DISPATCH;


opcode_compgte:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] >= data[op2];

    DISPATCH;


opcode_complt:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] < data[op2];

    DISPATCH;


opcode_complte:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] <= data[op2];

    DISPATCH;



opcode_and:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] && data[op2];

    DISPATCH;


opcode_or:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] || data[op2];

    DISPATCH;


opcode_add:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] + data[op2];

    DISPATCH;

    
opcode_sub:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] - data[op2];

    DISPATCH;


opcode_mul:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;
    data[dest] = data[op1] * data[op2];

    DISPATCH;


opcode_div:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    if( data[op2] != 0 ){

        data[dest] = data[op1] / data[op2];
    }
    else{

        data[dest] = 0;
    }

    DISPATCH;


opcode_mod:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    if( data[op2] != 0 ){

        data[dest] = data[op1] % data[op2];
    }
    else{

        data[dest] = 0;
    }

    DISPATCH;


opcode_f16_compeq:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] == data[op2];

    DISPATCH;


opcode_f16_compneq:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] != data[op2];

    DISPATCH;


opcode_f16_compgt:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] > data[op2];

    DISPATCH;


opcode_f16_compgte:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] >= data[op2];

    DISPATCH;


opcode_f16_complt:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] < data[op2];

    DISPATCH;


opcode_f16_complte:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] <= data[op2];

    DISPATCH;



opcode_f16_and:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] && data[op2];

    DISPATCH;


opcode_f16_or:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] || data[op2];

    DISPATCH;


opcode_f16_add:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] + data[op2];

    DISPATCH;

    
opcode_f16_sub:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = data[op1] - data[op2];

    DISPATCH;


opcode_f16_mul:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[dest] = ( (int64_t)data[op1] * (int64_t)data[op2] ) / 65536;

    DISPATCH;


opcode_f16_div:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    if( data[op2] != 0 ){

        data[dest] = ( (int64_t)data[op1] * 65536 ) / data[op2];
    }
    else{

        data[dest] = 0;
    }

    DISPATCH;


opcode_f16_mod:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    if( data[op2] != 0 ){

        data[dest] = data[op1] % data[op2];
    }
    else{

        data[dest] = 0;
    }

    DISPATCH;


opcode_jmp:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    
    pc = code + dest;

    DISPATCH;


opcode_jmp_if_z:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    if( data[src] == 0 ){

        pc = code + dest;
    }
    

    DISPATCH;


opcode_jmp_if_not_z:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    if( data[src] != 0 ){

        pc = code + dest;
    }
    

    DISPATCH;


opcode_jmp_if_l_pre_inc:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    op1 = *pc++;
    op1 += ( *pc++ ) << 8;
    op2 = *pc++;
    op2 += ( *pc++ ) << 8;

    data[op1]++;

    if( data[op1] < data[op2] ){

        pc = code + dest;
    }
    
    DISPATCH;


opcode_ret:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    
    // move return val to return register
    data[RETURN_VAL_ADDR] = data[dest];

    // check if call depth is 0
    // if so, we are exiting the VM
    if( call_depth == 0 ){

        return VM_STATUS_OK;
    }

    // pop PC from call stack
    call_depth--;

    if( call_depth > VM_MAX_CALL_DEPTH ){

        return VM_STATUS_CALL_DEPTH_EXCEEDED;
    }

    pc = call_stack[call_depth];
        
    DISPATCH;


opcode_call:
    
    call_target = *pc++;
    call_target += ( *pc++ ) << 8;

    call_param_len = *pc++;

    while( call_param_len > 0 ){
        call_param_len--;

        // decode
        call_param = *pc++;
        call_param += ( *pc++ ) << 8;
        call_arg = *pc++;
        call_arg += ( *pc++ ) << 8;

        // move param to arg
        data[call_arg] = data[call_param];
    }

    // set up return stack
    call_stack[call_depth] = pc;
    call_depth++;

    if( call_depth > VM_MAX_CALL_DEPTH ){

        return VM_STATUS_CALL_DEPTH_EXCEEDED;
    }

    // call by jumping to target
    pc = code + call_target;

    DISPATCH;


opcode_lcall:
    hash =  (catbus_hash_t32)(*pc++) << 24;
    hash |= (catbus_hash_t32)(*pc++) << 16;
    hash |= (catbus_hash_t32)(*pc++) << 8;
    hash |= (catbus_hash_t32)(*pc++) << 0;

    len = *pc++;
    
    for( uint32_t i = 0; i < len; i++ ){
        temp = *pc++;
        temp += ( *pc++ ) << 8;

        // params[i] = data[temp]; // by value
        params[i] = temp; // by reference
    }

    result = *pc++;
    result += ( *pc++ ) << 8;

    // initialize result to 0
    data[result] = 0;

    if( vm_lib_i8_libcall_built_in( hash, state, data, &data[result], params, len ) != 0 ){

        #ifdef VM_ENABLE_GFX
        // try gfx lib
        // load params by value
        for( uint32_t i = 0; i < len; i++ ){

            params[i] = data[params[i]];
        }

        data[result] = gfx_i32_lib_call( hash, params, len );
        #endif
    }
    else{

        // internal lib call completed successfully

        // check yield flag
        if( state->yield ){

            // check call depth, can only yield from top level functions running as a thread
            if( ( call_depth != 0 ) || ( state->current_thread < 0 ) ){

                return VM_STATUS_IMPROPER_YIELD;
            }

            // store PC offset
            state->threads[state->current_thread].pc_offset = pc - ( code + func_addr );

            // yield!
            return VM_STATUS_YIELDED;
        }
    }
    
    DISPATCH;


opcode_dbcall:
    hash =  (catbus_hash_t32)(*pc++) << 24;
    hash |= (catbus_hash_t32)(*pc++) << 16;
    hash |= (catbus_hash_t32)(*pc++) << 8;
    hash |= (catbus_hash_t32)(*pc++) << 0;

    db_hash =  (catbus_hash_t32)(*pc++) << 24;
    db_hash |= (catbus_hash_t32)(*pc++) << 16;
    db_hash |= (catbus_hash_t32)(*pc++) << 8;
    db_hash |= (catbus_hash_t32)(*pc++) << 0;

    len = *pc++;

    for( uint32_t i = 0; i < len; i++ ){
        temp = *pc++;
        temp += ( *pc++ ) << 8;

        params[i] = temp; // by reference
    }

    result = *pc++;
    result += ( *pc++ ) << 8;

    // initialize result to 0
    data[result] = 0;

    // call db func
    if( hash == __KV__len ){

        #ifdef VM_ENABLE_KV
        catbus_meta_t meta;
        
        if( kv_i8_get_meta( db_hash, &meta ) < 0 ){

            data[result] = 0;
        }
        else{

            data[result] = meta.count + 1;
        }
        #endif
    }    
    
    DISPATCH;


opcode_index:
    result = *pc++;
    result += ( *pc++ ) << 8;

    base_addr = *pc++;
    base_addr += ( *pc++ ) << 8;
    
    data[result] = base_addr;

    len = *pc++;

    while( len > 0 ){
        len--;

        // decode
        index = *pc++;
        index += ( *pc++ ) << 8;

        count = *pc++;
        count += ( *pc++ ) << 8;

        stride = *pc++;
        stride += ( *pc++ ) << 8;

        temp = data[index];

        if( count > 0 ){

            temp %= count;
            temp *= stride;
        }

        data[result] += temp;
    }

    DISPATCH;


opcode_load_indirect:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    temp = data[src];

    // bounds check
    if( temp >= state->data_count ){

        return VM_STATUS_INDEX_OUT_OF_BOUNDS;        
    }

    data[dest] = data[temp];
    
    DISPATCH;


opcode_store_indirect:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    temp = data[dest];

    // bounds check
    if( temp >= state->data_count ){

        return VM_STATUS_INDEX_OUT_OF_BOUNDS;        
    }

    data[temp] = data[src];

    DISPATCH;


opcode_assert:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    
    if( data[dest] == FALSE ){

        #ifndef VM_TARGET_ESP
        // log_v_debug_P( PSTR("VM assertion failed") );
        #endif
        return VM_STATUS_ASSERT;
    }

    DISPATCH;


opcode_halt:
    return VM_STATUS_HALT;

    DISPATCH;


opcode_vmov:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    len = *pc++;
    len += ( *pc++ ) << 8;
    type = *pc++;

    // deference pointer
    dest = data[dest];

    for( uint16_t i = 0; i < len; i++ ){

        data[dest + i] = data[src];
    }

    DISPATCH;


opcode_vadd:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    len = *pc++;
    len += ( *pc++ ) << 8;
    type = *pc++;

    // deference pointer
    dest = data[dest];
    
    for( uint16_t i = 0; i < len; i++ ){

        data[dest + i] += data[src];
    }

    DISPATCH;


opcode_vsub:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    len = *pc++;
    len += ( *pc++ ) << 8;
    type = *pc++;

    // deference pointer
    dest = data[dest];
    
    for( uint16_t i = 0; i < len; i++ ){

        data[dest + i] -= data[src];
    }

    DISPATCH;


opcode_vmul:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    len = *pc++;
    len += ( *pc++ ) << 8;
    type = *pc++;

    // deference pointer
    dest = data[dest];
        
    if( type == CATBUS_TYPE_FIXED16 ){

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] = ( (int64_t)data[dest + i] * data[src] ) / 65536;
        }
    }
    else{

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] *= data[src];
        }
    }

    DISPATCH;


opcode_vdiv:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    len = *pc++;
    len += ( *pc++ ) << 8;
    type = *pc++;

    // deference pointer
    dest = data[dest];

    // check for divide by zero
    if( data[src] == 0 ){

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] = 0;
        }
    }
    else if( type == CATBUS_TYPE_FIXED16 ){

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] = ( (int64_t)data[dest + i] * 65536 ) / data[src];
        }
    }
    else{

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] /= data[src];
        }
    }

    DISPATCH;


opcode_vmod:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;
    len = *pc++;
    len += ( *pc++ ) << 8;
    type = *pc++;

    // deference pointer
    dest = data[dest];

    // check for divide by zero
    if( data[src] == 0 ){

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] = 0;
        }
    }
    else{

        for( uint16_t i = 0; i < len; i++ ){

            data[dest + i] %= data[src];
        }
    }

    DISPATCH;


opcode_pmov:
    array = *pc++;
    attr = *pc++;
    
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    #ifdef VM_ENABLE_GFX
    gfx_v_array_move( array, attr, data[src] );
    #endif

    DISPATCH;


opcode_padd:
    array = *pc++;
    attr = *pc++;
    
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    #ifdef VM_ENABLE_GFX
    gfx_v_array_add( array, attr, data[src] );
    #endif
    
    DISPATCH;


opcode_psub:
    array = *pc++;
    attr = *pc++;
    
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    #ifdef VM_ENABLE_GFX
    gfx_v_array_sub( array, attr, data[src] );
    #endif

    
    DISPATCH;


opcode_pmul:
    array = *pc++;
    attr = *pc++;
    
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    #ifdef VM_ENABLE_GFX
    gfx_v_array_mul( array, attr, data[src] );
    #endif

    DISPATCH;


opcode_pdiv:
    array = *pc++;
    attr = *pc++;
    
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    #ifdef VM_ENABLE_GFX
    gfx_v_array_div( array, attr, data[src] );
    #endif

    DISPATCH;


opcode_pmod:
    array = *pc++;
    attr = *pc++;
    
    src = *pc++;
    src += ( *pc++ ) << 8;
    
    #ifdef VM_ENABLE_GFX
    gfx_v_array_mod( array, attr, data[src] );
    #endif

    DISPATCH;


opcode_pstore_hue:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    src = *pc++;
    src += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX

//     // wraparound to 16 bit range.
//     // this makes it easy to run a circular rainbow
//     op1 %= 65536;
    // ^^^^^^ I don't think we actually need to do this.
    // gfx will crunch from i32 to u16, which does the mod for free.

    gfx_v_set_hue( data[src], data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_pstore_sat:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    src = *pc++;
    src += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    // load source
    value_i32 = data[src];    

    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound
    if( value_i32 > 65535 ){

        value_i32 = 65535;
    }
    else if( value_i32 < 0 ){

        value_i32 = 0;
    }

    gfx_v_set_sat( value_i32, data[index_x], data[index_y], array );
    #endif

    
    DISPATCH;


opcode_pstore_val:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    src = *pc++;
    src += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    // load source
    value_i32 = data[src];    

    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound
    if( value_i32 > 65535 ){

        value_i32 = 65535;
    }
    else if( value_i32 < 0 ){

        value_i32 = 0;
    }

    gfx_v_set_val( value_i32, data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_pstore_vfade:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    src = *pc++;
    src += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    // load source
    value_i32 = data[src];    

    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound
    if( value_i32 > 65535 ){

        value_i32 = 65535;
    }
    else if( value_i32 < 0 ){

        value_i32 = 0;
    }

    gfx_v_set_v_fade( value_i32, data[index_x], data[index_y], array );
    #endif

    DISPATCH;


opcode_pstore_hsfade:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    src = *pc++;
    src += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    // load source
    value_i32 = data[src];    

    // clamp to our 16 bit range.
    // we will essentially saturate at 0 or 65535,
    // but will not wraparound
    if( value_i32 > 65535 ){

        value_i32 = 65535;
    }
    else if( value_i32 < 0 ){

        value_i32 = 0;
    }

    gfx_v_set_hs_fade( value_i32, data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_pload_hue:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_hue( data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_pload_sat:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_sat( data[index_x], data[index_y], array );
    #endif

    DISPATCH;


opcode_pload_val:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_val( data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_pload_vfade:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_v_fade( data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_pload_hsfade:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_hs_fade( data[index_x], data[index_y], array );
    #endif

    DISPATCH;


opcode_db_store:
    hash =  (catbus_hash_t32)(*pc++) << 24;
    hash |= (catbus_hash_t32)(*pc++) << 16;
    hash |= (catbus_hash_t32)(*pc++) << 8;
    hash |= (catbus_hash_t32)(*pc++) << 0;

    len = *pc++;

    indexes[0] = 0;

    for( uint32_t i = 0; i < len; i++ ){

        index = *pc++;
        index += ( *pc++ ) << 8;

        indexes[i] = data[index];
    }
    
    type = *pc++;

    src = *pc++;
    src += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_KV
    #ifdef ESP8266
    kvdb_i8_array_set( hash, type, indexes[0], &data[src], sizeof(data[src]) );
    #else
    catbus_i8_array_set( hash, type, indexes[0], 1, &data[src] );
    catbus_i8_publish( hash );
    #endif
    #endif
    
    DISPATCH;


opcode_db_load:
    hash =  (catbus_hash_t32)(*pc++) << 24;
    hash |= (catbus_hash_t32)(*pc++) << 16;
    hash |= (catbus_hash_t32)(*pc++) << 8;
    hash |= (catbus_hash_t32)(*pc++) << 0;

    len = *pc++;

    indexes[0] = 0;

    for( uint32_t i = 0; i < len; i++ ){

        index = *pc++;
        index += ( *pc++ ) << 8;

        indexes[i] = data[index];
    }
    
    type = *pc++;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_KV
    #ifdef ESP8266
    if( kvdb_i8_array_get( hash, type, indexes[0], &data[dest], sizeof(data[dest]) ) < 0 ){

        data[dest] = 0;        
    }
    #else
    if( catbus_i8_array_get( hash, type, indexes[0], 1, &data[dest] ) < 0 ){

        data[dest] = 0;        
    }
    #endif
    #endif
    
    DISPATCH;


opcode_conv_i32_to_f16:        
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    data[dest] = data[src] * 65536;

    DISPATCH;


opcode_conv_f16_to_i32:
    dest = *pc++;
    dest += ( *pc++ ) << 8;
    src = *pc++;
    src += ( *pc++ ) << 8;

    data[dest] = data[src] / 65536;
    
    DISPATCH;


opcode_is_v_fading:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_is_v_fading( data[index_x], data[index_y], array );
    #endif

    DISPATCH;


opcode_is_hs_fading:
    array = *pc++;

    index_x = *pc++;
    index_x += ( *pc++ ) << 8;
    
    index_y = *pc++;
    index_y += ( *pc++ ) << 8;

    dest = *pc++;
    dest += ( *pc++ ) << 8;

    #ifdef VM_ENABLE_GFX
    data[dest] = gfx_u16_get_is_hs_fading( data[index_x], data[index_y], array );
    #endif
    
    DISPATCH;


opcode_trap:
    return VM_STATUS_TRAP;
}



int8_t vm_i8_run(
    uint8_t *stream,
    uint16_t func_addr,
    uint16_t pc_offset,
    vm_state_t *state ){

    cycles = VM_MAX_CYCLES;

    int32_t *data = (int32_t *)( stream + state->data_start );

    // load published vars
    vm_publish_t *publish = (vm_publish_t *)&stream[state->publish_start];

    uint32_t count = state->publish_count;

    while( count > 0 ){

        kvdb_i8_get( publish->hash, publish->type, &data[publish->addr], sizeof(data[publish->addr]) );

        publish++;
        count--;
    }

    #ifdef VM_ENABLE_GFX

    // set pixel arrays
    gfx_v_init_pixel_arrays( (gfx_pixel_array_t *)&data[PIX_ARRAY_ADDR], state->pix_obj_count );

    #endif

    state->yield = FALSE;

    int8_t status = _vm_i8_run_stream( stream, func_addr, pc_offset, state, data );

    cycles = VM_MAX_CYCLES - cycles;

    if( cycles > state->max_cycles ){

        state->max_cycles = cycles;        
    }

    // store published vars back to DB
    publish = (vm_publish_t *)&stream[state->publish_start];

    count = state->publish_count;

    while( count > 0 ){

        kvdb_i8_set( publish->hash, publish->type, &data[publish->addr], sizeof(data[publish->addr]) );

        publish++;
        count--;
    }

    return status;
}


int8_t vm_i8_run_init(
    uint8_t *stream,
    vm_state_t *state ){

    state->frame_number = 0;

    return vm_i8_run( stream, state->init_start, 0, state );
}


int8_t vm_i8_run_loop(
    uint8_t *stream,
    vm_state_t *state ){

    state->frame_number++;

    return vm_i8_run( stream, state->loop_start, 0, state );
}

int8_t vm_i8_run_threads(
    uint8_t *stream,
    vm_state_t *state ){

    bool threads_running = FALSE;

    for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

        if( state->threads[i].func_addr == 0xffff ){

            continue;
        }

        // check thread delay
        if( state->threads[i].delay_ticks > 0 ){

            // decrement
            state->threads[i].delay_ticks--;

            continue;
        }

        state->current_thread = i;

        int8_t status = vm_i8_run( stream, state->threads[i].func_addr, state->threads[i].pc_offset, state );

        threads_running = TRUE;

        state->current_thread = -1;

        if( status == VM_STATUS_OK ){

            // thread returned, kill it
            state->threads[i].func_addr = 0xffff;
        }
        else if( status == VM_STATUS_YIELDED ){


        }
        else if( status == VM_STATUS_HALT ){

            return status;
        }
    }

    if( !threads_running ){

        return VM_STATUS_NO_THREADS;
    } 

    return VM_STATUS_OK;
}

int8_t vm_i8_run_cron(
    uint8_t *stream,
    vm_state_t *state,
    uint32_t ntp_seconds ){

    cron_t *cron = (cron_t *)&stream[state->cron_start];

    if( state->last_cron < ntp_seconds ){

        state->last_cron = ntp_seconds - 1;
    }
    else if( ( ntp_seconds - state->last_cron ) > 5 ){

        state->last_cron = ntp_seconds - 1;
    }
    
    while( state->last_cron < ntp_seconds ){

        state->last_cron++;

        datetime_t datetime;
        datetime_v_seconds_to_datetime( state->last_cron, &datetime );

        for( uint8_t i = 0; i < state->cron_count; i++ ){

            bool match = TRUE;

            if( ( cron->seconds >= 0 ) && ( cron->seconds != datetime.seconds ) ){

                match = FALSE;
            }

            if( ( cron->minutes >= 0 ) && ( cron->minutes != datetime.minutes ) ){

                match = FALSE;
            }

            if( ( cron->hours >= 0 ) && ( cron->hours != datetime.hours ) ){

                match = FALSE;
            }

            if( ( cron->day_of_month >= 0 ) && ( cron->day_of_month != datetime.day ) ){

                match = FALSE;
            }

            if( ( cron->day_of_week >= 0 ) && ( cron->day_of_week != datetime.weekday ) ){

                match = FALSE;
            }

            if( ( cron->month >= 0 ) && ( cron->month != datetime.month ) ){

                match = FALSE;
            }

            // the check for cron->run ensures the entry only runs one time
            // for each time specified.
            // if you set for instance hours = 15, but set nothing else,
            // then without the run flag the task would run every second
            // for the entire hour.

            if( match && !cron->run ){

                cron->run = TRUE;

                int8_t status = vm_i8_run( stream, cron->func_addr, 0, state );

                if( status == VM_STATUS_HALT ){

                    return status;
                }
            }

            if( !match ){

                cron->run = FALSE;
            }

            cron++;
        }
    }

    return VM_STATUS_OK;
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

    // reset thread state
    for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

        state->threads[i].func_addr = 0xffff;
    }

    state->current_thread = -1;
    state->tick_rate = 1; // set default tick rate

    if( ( flags & VM_LOAD_FLAGS_CHECK_HEADER ) == 0 ){

        // verify crc
        uint32_t check_len = len - sizeof(uint32_t);
        uint32_t hash;
        memcpy( &hash, stream + check_len, sizeof(hash) );

        if( hash_u32_data( stream, check_len ) != hash ){

            return VM_STATUS_ERR_BAD_HASH;
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
    
    state->program_name_hash = prog_header->program_name_hash;    

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

    state->link_count = prog_header->link_len / sizeof(link_t);
    state->link_start = obj_start;
    obj_start += prog_header->link_len;

    if( ( state->link_start % 4 ) != 0 ){

        return VM_STATUS_LINK_MISALIGN;
    }

    state->db_count = prog_header->db_len / sizeof(catbus_meta_t);
    state->db_start = obj_start;
    obj_start += prog_header->db_len;

    if( ( state->db_start % 4 ) != 0 ){

        return VM_STATUS_DB_MISALIGN;
    }

    state->cron_count = prog_header->cron_len / sizeof(cron_t);
    state->cron_start = obj_start;
    obj_start += prog_header->cron_len;

    if( ( state->cron_start % 4 ) != 0 ){

        return VM_STATUS_CRON_MISALIGN;
    }
    

    // if just checking the header, we're done at this point
    if( ( flags & VM_LOAD_FLAGS_CHECK_HEADER ) != 0 ){

        return VM_STATUS_OK;
    }


    // set up final items for VM execution

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


void vm_v_init_db(
    uint8_t *stream,
    vm_state_t *state,
    uint8_t tag ){

    // add published vars to DB
    uint32_t count = state->publish_count;
    vm_publish_t *publish = (vm_publish_t *)&stream[state->publish_start];

    while( count > 0 ){        

        kvdb_i8_add( publish->hash, CATBUS_TYPE_INT32, 1, 0, 0 );
        kvdb_v_set_tag( publish->hash, tag );

        publish++;
        count--;
    }
}


void vm_v_clear_db( uint8_t tag ){

    // delete existing entries
    kvdb_v_clear_tag( 0, tag );
}

