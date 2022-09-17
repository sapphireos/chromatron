// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
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
#include "cpu.h"
#include "gfx_lib.h"
#include "random.h"
#include "vm_core.h"
#include "trig.h"
#include "hash.h"
#include "target.h"
#include "vm_lib.h"
#include "catbus.h"
#include "datetime.h"
#include "logging.h"
#include "timers.h"
#include "fs.h"
#include "config.h"

#ifdef VM_ENABLE_KV
#include "keyvalue.h"
#include "kvdb.h"
#ifdef VM_ENABLE_CATBUS
#include "catbus.h"
#endif
#endif

#if defined(ESP8266) && defined(VM_OPTIMIZED_DECODE)
#error "VM_OPTIMIZED_DECODE does not work on ESP8266!"
#endif


// keys that we really don't want the VM be to be able to write to.
// generally, these are going to be things that would allow it to 
// brick hardware, mess up the wifi connection, or mess up the pixel 
// array.
// static const PROGMEM uint32_t restricted_keys[] = {
//     __KV__reboot,
//     __KV__wifi_enable_ap,
//     __KV__wifi_router,
//     __KV__pix_clock,
//     __KV__pix_count,
//     __KV__pix_mode,    
// };

static uint32_t cycles;

// #ifdef VM_OPTIMIZED_DECODE
// typedef struct __attribute__((packed)){
//     uint16_t dest;
//     uint16_t src;
// } decode2_t;

// typedef struct __attribute__((packed)){
//     uint16_t dest;
//     uint16_t op1;
//     uint16_t op2;
// } decode3_t;

// typedef struct __attribute__((packed)){
//     uint16_t dest;
//     uint16_t src;
//     uint16_t len;
//     uint8_t type;
// } decodev_t;

// typedef struct __attribute__((packed)){
//     uint8_t array;
//     uint8_t attr;
//     uint16_t src;
// } decodep_t;

// typedef struct __attribute__((packed)){
//     uint8_t array;
//     uint16_t index_x;
//     uint16_t index_y;
//     uint16_t src;
// } decodep2_t;

// typedef struct __attribute__((packed)){
//     uint8_t array;
//     uint16_t index_x;
//     uint16_t index_y;
//     uint16_t dest;
// } decodep3_t;
// #endif

typedef struct __attribute__((packed)){
    uint16_t pool;
    uint16_t addr;
} packed_reference_t;

typedef union{
    packed_reference_t ref;
    uint32_t n;
} reference_t;


#define POOL_GLOBAL             0
#define POOL_PIXEL_ARRAY        1
#define POOL_STRING_LITERALS    2
#define POOL_FUNCTIONS          3
#define POOL_LOCAL              4


#define REG_ZERO                0
#define REG_CALL_PARAMS         1


// opcode decoders:

#define DECODE_NOP pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t op1;
    uint16_t padding;
} opcode_1ac_t;
#define DECODE_1AC opcode_1ac = (opcode_1ac_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t op1;
    uint8_t padding;
} opcode_2ac_t;
#define DECODE_2AC opcode_2ac = (opcode_2ac_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t op1;
    uint8_t op2;
} opcode_3ac_t;
#define DECODE_3AC opcode_3ac = (opcode_3ac_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t op1;
    uint8_t op2;
    uint8_t op3;
} opcode_4ac_t;
#define DECODE_4AC opcode_4ac = (opcode_4ac_t *)pc; pc += 8;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t op1;
    uint8_t op2;
    uint8_t op3;
    uint8_t op4;
} opcode_5ac_t;
#define DECODE_5AC opcode_5ac = (opcode_5ac_t *)pc; pc += 8;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint16_t imm1;
} opcode_1i_t;
#define DECODE_1I opcode_1i = (opcode_1i_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint16_t imm1;
    uint8_t reg1;
} opcode_1i1r_t;
#define DECODE_1I1R opcode_1i1r = (opcode_1i1r_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint16_t imm1;
    uint16_t imm2;
    uint8_t reg1;
} opcode_2i1r_t;
#define DECODE_2I1R opcode_2i1r = (opcode_2i1r_t *)pc; pc += 8;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint16_t imm1;
    uint8_t reg1;
    uint8_t reg2;
} opcode_1i2r_t;
#define DECODE_1I2R opcode_1i2r = (opcode_1i2r_t *)pc; pc += 8;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t imm1;
    uint8_t reg1;
    uint8_t reg2;
} opcode_1i2rs_t;
#define DECODE_1I2RS opcode_1i2rs = (opcode_1i2rs_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint16_t imm1;
    uint8_t reg1;
    uint8_t reg2;
    uint8_t reg3;
} opcode_1i3r_t;
#define DECODE_1I3R opcode_1i3r = (opcode_1i3r_t *)pc; pc += 8;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint16_t imm1;
    uint8_t reg1;
    uint8_t reg2;
    uint8_t reg3;
    uint8_t reg4;
} opcode_1i4r_t;
#define DECODE_1I4R opcode_1i4r = (opcode_1i4r_t *)pc; pc += 8;

// typedef struct __attribute__((packed)){
//     uint8_t opcode;
//     uint8_t dest;
//     uint8_t ref;
// } opcode_lkp0_t;
// #define DECODE_LKP0 opcode_lkp0 = (opcode_lkp0_t *)pc; pc += 4;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t ref;
    uint8_t index1;
    uint8_t count1;
    uint8_t stride1;
} opcode_lkp1_t;
#define DECODE_LKP1 opcode_lkp1 = (opcode_lkp1_t *)pc; pc += 8;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t ref;
    uint8_t index1;
    uint8_t count1;
    uint8_t stride1;
    uint8_t index2;
    uint8_t count2;
    uint8_t stride2;
} opcode_lkp2_t;
#define DECODE_LKP2 opcode_lkp2 = (opcode_lkp2_t *)pc; pc += 12;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t dest;
    uint8_t ref;
    uint8_t index1;
    uint8_t count1;
    uint8_t stride1;
    uint8_t index2;
    uint8_t count2;
    uint8_t stride2;
    uint8_t index3;
    uint8_t count3;
    uint8_t stride3;
} opcode_lkp3_t;
#define DECODE_LKP3 opcode_lkp3 = (opcode_lkp3_t *)pc; pc += 12;

typedef struct __attribute__((packed)){
    uint8_t opcode;
    uint8_t target;
    uint8_t value;
    uint8_t type;
    uint16_t length;
} opcode_vector_t;
#define DECODE_VECTOR opcode_vector = (opcode_vector_t *)pc; pc += 8;



#define CALL_SETUP \
    /* set up return stack */ \
    call_stack[call_depth] = pc; \
    /* record frame size of this function */ \
    frame_stack[call_depth] = current_frame_size; \
    /* look up function */ \
    current_frame_size = func_table[index].frame_size;

#define CALL_SWITCH_CONTEXT \
    /* adjust local memory pointers: */ \
    local_memory += frame_stack[call_depth] / 4; \
    registers = local_memory; \
    if( ( (uint32_t)( local_memory - locals_start ) + current_frame_size ) > state->local_data_len ){ \
        return VM_STATUS_ERR_LOCAL_OUT_OF_BOUNDS; \
    } \
    call_depth++; \
    pools[N_STATIC_POOLS + call_depth] = local_memory; \
    if( call_depth > VM_MAX_CALL_DEPTH ){ \
        return VM_STATUS_CALL_DEPTH_EXCEEDED; \
    }

#define CALL_FINISH \
    /* call by jumping to target */ \
    pc = code + func_table[index].addr;


#ifdef VM_ENABLE_GFX
#define GFX_LIB_CALL(FUNC_HASH, PARAMS_LEN) state->return_val = gfx_i32_lib_call( FUNC_HASH, params, PARAMS_LEN );
#else
#define GFX_LIB_CALL(FUNC_HASH, PARAMS_LEN)
#endif

#define LIBCALL(FUNC_HASH, PARAMS_LEN) \
    if( vm_lib_i8_libcall_built_in( FUNC_HASH, state, &state->return_val, params, PARAMS_LEN ) != 0 ){ \
        /* try gfx lib */ \
        GFX_LIB_CALL(FUNC_HASH, PARAMS_LEN) \
    } \
    else{ \
        /* internal lib call completed successfully */ \
        /* check yield flag */ \
        if( state->yield > 0 ){ \
            /* check call depth, can only yield from top level functions running as a thread */ \
            if( ( call_depth != 0 ) || ( state->current_thread < 0 ) ){ \
                return VM_STATUS_IMPROPER_YIELD; \
            } \
            /* store PC offset */ \
            state->threads[state->current_thread].pc_offset = pc - ( code + func_addr ); \
            /* yield! */ \
            return VM_STATUS_YIELDED; \
        } \
    }

static int8_t _vm_i8_run_stream(
    uint8_t *stream,
    uint16_t func_addr,
    uint16_t pc_offset,
    vm_state_t *state ){

#if defined(ESP8266)
    static void *opcode_table[] = {
#else
    static void *const opcode_table[] PROGMEM = {
#endif

        &&opcode_trap,              // 0

        &&opcode_mov,               // 1
        &&opcode_ldi,               // 2
        &&opcode_ldc,               // 3
        &&opcode_ldm,               // 4
        &&opcode_trap,              // 5
        // &&opcode_ldg,               // 4
        // &&opcode_ldl,               // 5
        &&opcode_ref,               // 6
        &&opcode_ldgi,              // 7
        &&opcode_stm,               // 8
        &&opcode_trap,              // 9
        // &&opcode_stg,               // 8
        // &&opcode_stl,               // 9
        &&opcode_stgi,              // 10
        &&opcode_ldstr,             // 11
        &&opcode_nop,               // 12

        &&opcode_lddb,              // 13
        &&opcode_stdb,              // 14
        &&opcode_lddbi,             // 15
        &&opcode_stdbi,             // 16
        
        &&opcode_trap,              // 17
        &&opcode_trap,              // 18

        &&opcode_ret,               // 19
        &&opcode_jmp,               // 20
        &&opcode_jmpz,              // 21
        &&opcode_loop,              // 22
        &&opcode_load_ret_val,      // 23

        &&opcode_trap,              // 24
        &&opcode_trap,              // 25
        &&opcode_trap,              // 26
        &&opcode_trap,              // 27
        &&opcode_trap,              // 28
        &&opcode_trap,              // 29
        &&opcode_trap,              // 30
        &&opcode_trap,              // 31

        &&opcode_compeq,            // 32
        &&opcode_compneq,           // 33
        &&opcode_compgt,            // 34
        &&opcode_compgte,           // 35
        &&opcode_complt,            // 36
        &&opcode_complte,           // 37

        &&opcode_not,               // 38
        &&opcode_and,               // 39
        &&opcode_or,                // 40

        &&opcode_add,               // 41
        &&opcode_sub,               // 42
        &&opcode_mul,               // 43
        &&opcode_div,               // 44
        &&opcode_mod,               // 45
        &&opcode_mul_f16,           // 46
        &&opcode_div_f16,           // 47

        &&opcode_conv_i32_to_f16,   // 48
        &&opcode_conv_f16_to_i32,   // 49
        &&opcode_conv_gfx16_to_f16, // 50
        
        &&opcode_trap,              // 51
        &&opcode_trap,              // 52
        &&opcode_trap,              // 53
        &&opcode_trap,              // 54
        &&opcode_trap,              // 55

        &&opcode_plookup1,          // 56
        &&opcode_plookup2,          // 57
        &&opcode_pload_attr,        // 58
        &&opcode_pstore_attr,       // 59

        &&opcode_trap,              // 60
        &&opcode_trap,              // 61
        &&opcode_trap,              // 62
        &&opcode_trap,              // 63

        &&opcode_trap,              // 64 // lookup0 - not implemented
        &&opcode_lookup1,           // 65
        &&opcode_lookup2,           // 66
        &&opcode_lookup3,           // 67

        &&opcode_trap,              // 68
        &&opcode_trap,              // 69
        &&opcode_trap,              // 70
        &&opcode_dbcall,            // 71

        &&opcode_call0,             // 72
        &&opcode_call1,             // 73
        &&opcode_call2,             // 74
        &&opcode_call3,             // 75
        &&opcode_call4,             // 76
        &&opcode_trap,              // 77
        &&opcode_trap,              // 78
        &&opcode_trap,              // 79

        &&opcode_icall0,            // 80
        &&opcode_icall1,            // 81
        &&opcode_icall2,            // 82
        &&opcode_icall3,            // 83
        &&opcode_icall4,            // 84

        &&opcode_trap,              // 85
        &&opcode_trap,              // 86
        &&opcode_trap,              // 87

        &&opcode_lcall0,            // 88
        &&opcode_lcall1,            // 89
        &&opcode_lcall2,            // 90
        &&opcode_lcall3,            // 91
        &&opcode_lcall4,            // 92
        &&opcode_trap,              // 93
        &&opcode_trap,              // 94
        &&opcode_trap,              // 95

        &&opcode_pstore_hue,        // 96
        &&opcode_pstore_sat,        // 97
        &&opcode_pstore_val,        // 98
        &&opcode_pstore_hs_fade,    // 99
        &&opcode_pstore_v_fade,     // 100
        &&opcode_trap,              // 101
        &&opcode_trap,              // 102
        &&opcode_trap,              // 103
        
        &&opcode_vstore_hue,        // 104
        &&opcode_vstore_sat,        // 105
        &&opcode_vstore_val,        // 106
        &&opcode_vstore_hs_fade,    // 107
        &&opcode_vstore_v_fade,     // 108
        &&opcode_trap,              // 109
        &&opcode_trap,              // 110
        &&opcode_trap,              // 111

        &&opcode_pload_hue,         // 112
        &&opcode_pload_sat,         // 113
        &&opcode_pload_val,         // 114
        &&opcode_trap,              // 115
        &&opcode_trap,              // 116
        &&opcode_trap,              // 117
        &&opcode_trap,              // 118
        &&opcode_trap,              // 119
        &&opcode_trap,              // 120
        &&opcode_trap,              // 121
        &&opcode_trap,              // 122
        &&opcode_trap,              // 123
        &&opcode_trap,              // 124
        &&opcode_trap,              // 125
        &&opcode_trap,              // 126
        &&opcode_trap,              // 127

        &&opcode_padd_hue,          // 128
        &&opcode_padd_sat,          // 129
        &&opcode_padd_val,          // 130
        &&opcode_padd_hs_fade,      // 131
        &&opcode_padd_v_fade,       // 132
        &&opcode_trap,              // 133
        &&opcode_trap,              // 134
        &&opcode_trap,              // 135
        &&opcode_vadd_hue,          // 136
        &&opcode_vadd_sat,          // 137
        &&opcode_vadd_val,          // 138
        &&opcode_vadd_hs_fade,      // 139
        &&opcode_vadd_v_fade,       // 140
        &&opcode_trap,              // 141
        &&opcode_trap,              // 142
        &&opcode_trap,              // 143
        
        &&opcode_psub_hue,          // 144
        &&opcode_psub_sat,          // 145
        &&opcode_psub_val,          // 146
        &&opcode_psub_hs_fade,      // 147
        &&opcode_psub_v_fade,       // 148
        &&opcode_trap,              // 149
        &&opcode_trap,              // 150
        &&opcode_trap,              // 151
        &&opcode_vsub_hue,          // 152
        &&opcode_vsub_sat,          // 153
        &&opcode_vsub_val,          // 154
        &&opcode_vsub_hs_fade,      // 155
        &&opcode_vsub_v_fade,       // 156
        &&opcode_trap,              // 157
        &&opcode_trap,              // 158
        &&opcode_trap,              // 159

        &&opcode_pmul_hue,          // 160
        &&opcode_pmul_sat,          // 161
        &&opcode_pmul_val,          // 162
        &&opcode_pmul_hs_fade,      // 163
        &&opcode_pmul_v_fade,       // 164
        &&opcode_trap,              // 165
        &&opcode_trap,              // 166
        &&opcode_trap,              // 167
        &&opcode_vmul_hue,          // 168
        &&opcode_vmul_sat,          // 169
        &&opcode_vmul_val,          // 170
        &&opcode_vmul_hs_fade,      // 171
        &&opcode_vmul_v_fade,       // 172
        &&opcode_trap,              // 173
        &&opcode_trap,              // 174
        &&opcode_trap,              // 175
        
        &&opcode_pdiv_hue,          // 176
        &&opcode_pdiv_sat,          // 177
        &&opcode_pdiv_val,          // 178
        &&opcode_pdiv_hs_fade,      // 179
        &&opcode_pdiv_v_fade,       // 180
        &&opcode_trap,              // 181
        &&opcode_trap,              // 182
        &&opcode_trap,              // 183
        &&opcode_vdiv_hue,          // 184
        &&opcode_vdiv_sat,          // 185
        &&opcode_vdiv_val,          // 186
        &&opcode_vdiv_hs_fade,      // 187
        &&opcode_vdiv_v_fade,       // 188
        &&opcode_trap,              // 189
        &&opcode_trap,              // 190
        &&opcode_trap,              // 191
        
        &&opcode_pmod_hue,          // 192
        &&opcode_pmod_sat,          // 193
        &&opcode_pmod_val,          // 194
        &&opcode_pmod_hs_fade,      // 195
        &&opcode_pmod_v_fade,       // 196
        &&opcode_trap,              // 197
        &&opcode_trap,              // 198
        &&opcode_trap,              // 199
        &&opcode_vmod_hue,          // 200
        &&opcode_vmod_sat,          // 201
        &&opcode_vmod_val,          // 202
        &&opcode_vmod_hs_fade,      // 203
        &&opcode_vmod_v_fade,       // 204
        &&opcode_trap,              // 205
        &&opcode_trap,              // 206
        &&opcode_trap,              // 207

        &&opcode_trap,              // 208
        &&opcode_trap,              // 209
        &&opcode_trap,              // 210
        &&opcode_trap,              // 211
        &&opcode_trap,              // 212
        &&opcode_trap,              // 213
        &&opcode_trap,              // 214
        &&opcode_trap,              // 215
        &&opcode_trap,              // 216
        &&opcode_trap,              // 217
        &&opcode_trap,              // 218
        &&opcode_trap,              // 219
        &&opcode_trap,              // 220
        &&opcode_trap,              // 221
        &&opcode_trap,              // 222
        &&opcode_trap,              // 223

        &&opcode_vmov,              // 224
        &&opcode_vadd,              // 225
        &&opcode_vsub,              // 226
        &&opcode_vmul,              // 227
        &&opcode_vdiv,              // 228
        &&opcode_vmod,              // 229
        &&opcode_vmin,              // 230
        &&opcode_vmax,              // 231
        &&opcode_vavg,              // 232
        &&opcode_vsum,              // 233
        
        &&opcode_trap,              // 234
        &&opcode_trap,              // 235
        &&opcode_trap,              // 236
        &&opcode_trap,              // 237
        &&opcode_trap,              // 238
        &&opcode_trap,              // 239
        &&opcode_trap,              // 240
        &&opcode_trap,              // 241


        &&opcode_trap,              // 242
        &&opcode_trap,              // 243
        &&opcode_trap,              // 244
        &&opcode_trap,              // 245
        &&opcode_trap,              // 246
        &&opcode_trap,              // 247

        &&opcode_halt,              // 248
        &&opcode_assert,            // 249
        &&opcode_print,             // 250
        &&opcode_trap,              // 251
        &&opcode_trap,              // 252
        &&opcode_trap,              // 253
        &&opcode_trap,              // 254
        &&opcode_trap,              // 255
    };

// #ifdef OLD_VM    

//         &&opcode_trap,              // 0

//         &&opcode_mov,	            // 1
//         &&opcode_clr,	            // 2
        
//         &&opcode_not,	            // 3
        
//         &&opcode_compeq,            // 4
//         &&opcode_compneq,	        // 5
//         &&opcode_compgt,	        // 6
//         &&opcode_compgte,	        // 7
//         &&opcode_complt,	        // 8
//         &&opcode_complte,	        // 9
//         &&opcode_and,	            // 10
//         &&opcode_or,	            // 11
//         &&opcode_add,	            // 12
//         &&opcode_sub,	            // 13
//         &&opcode_mul,	            // 14
//         &&opcode_div,	            // 15
//         &&opcode_mod,	            // 16

//         &&opcode_f16_compeq,        // 17
//         &&opcode_f16_compneq,       // 18
//         &&opcode_f16_compgt,        // 19
//         &&opcode_f16_compgte,       // 20
//         &&opcode_f16_complt,        // 21
//         &&opcode_f16_complte,       // 22
//         &&opcode_f16_and,           // 23
//         &&opcode_f16_or,            // 24
//         &&opcode_f16_add,           // 25
//         &&opcode_f16_sub,           // 26
//         &&opcode_f16_mul,           // 27
//         &&opcode_f16_div,           // 28
//         &&opcode_f16_mod,           // 29

//         &&opcode_jmp,	            // 30
//         &&opcode_jmp_if_z,	        // 31
//         &&opcode_jmp_if_not_z,	    // 32
//         &&opcode_jmp_if_l_pre_inc,  // 33

//         &&opcode_ret,	            // 34
//         &&opcode_call,	            // 35
//         &&opcode_lcall,             // 36
//         &&opcode_dbcall,            // 37

//         &&opcode_index,             // 38
//         &&opcode_load_indirect,     // 39
//         &&opcode_store_indirect,    // 40

//         &&opcode_assert,            // 41
//         &&opcode_halt,              // 42

//         &&opcode_vmov,              // 43
//         &&opcode_vadd,              // 44
//         &&opcode_vsub,              // 45
//         &&opcode_vmul,              // 46
//         &&opcode_vdiv,              // 47
//         &&opcode_vmod,              // 48

//         &&opcode_pmov,              // 49
//         &&opcode_padd,              // 50
//         &&opcode_psub,              // 51
//         &&opcode_pmul,              // 52
//         &&opcode_pdiv,              // 53
//         &&opcode_pmod,              // 54

//         &&opcode_pstore_hue,        // 55
//         &&opcode_pstore_sat,        // 56
//         &&opcode_pstore_val,        // 57
//         &&opcode_pstore_hsfade,     // 58


//         &&opcode_pstore_vfade,      // 59

//         &&opcode_pload_hue,         // 60
//         &&opcode_pload_sat,         // 61
//         &&opcode_pload_val,         // 62
//         &&opcode_pload_hsfade,      // 63
//         &&opcode_pload_vfade,       // 64

//         &&opcode_db_store,          // 65
//         &&opcode_db_load,           // 66

//         &&opcode_conv_i32_to_f16,   // 67
//         &&opcode_conv_f16_to_i32,   // 68

//         &&opcode_is_v_fading,	    // 69
//         &&opcode_is_hs_fading,	    // 70

//         &&opcode_pstore_pval,	    // 71
//         &&opcode_pload_pval,	    // 72

//         &&opcode_trap,	            // 73
//         &&opcode_trap,	            // 74
//         &&opcode_trap,	            // 75
//         &&opcode_trap,	            // 76
//         &&opcode_trap,	            // 77
//         &&opcode_trap,	            // 78
//         &&opcode_trap,	            // 79
//         &&opcode_trap,	            // 80
//         &&opcode_trap,	            // 81
//         &&opcode_trap,	            // 82
//         &&opcode_trap,	            // 83
//         &&opcode_trap,	            // 84
//         &&opcode_trap,	            // 85
//         &&opcode_trap,	            // 86
//         &&opcode_trap,	            // 87
//         &&opcode_trap,	            // 88
//         &&opcode_trap,	            // 89
//         &&opcode_trap,	            // 90
//         &&opcode_trap,	            // 91
//         &&opcode_trap,	            // 92
//         &&opcode_trap,	            // 93
//         &&opcode_trap,	            // 94
//         &&opcode_trap,	            // 95
//         &&opcode_trap,	            // 96
//         &&opcode_trap,	            // 97
//         &&opcode_trap,	            // 98
//         &&opcode_trap,	            // 99
//         &&opcode_trap,	            // 100
//         &&opcode_trap,	            // 101
//         &&opcode_trap,	            // 102
//         &&opcode_trap,	            // 103
//         &&opcode_trap,	            // 104
//         &&opcode_trap,	            // 105
//         &&opcode_trap,	            // 106
//         &&opcode_trap,	            // 107
//         &&opcode_trap,	            // 108
//         &&opcode_trap,	            // 109
//         &&opcode_trap,	            // 110
//         &&opcode_trap,	            // 111
//         &&opcode_trap,	            // 112
//         &&opcode_trap,	            // 113
//         &&opcode_trap,	            // 114
//         &&opcode_trap,	            // 115
//         &&opcode_trap,	            // 116
//         &&opcode_trap,	            // 117
//         &&opcode_trap,	            // 118
//         &&opcode_trap,	            // 119
//         &&opcode_trap,	            // 120
//         &&opcode_trap,	            // 121
//         &&opcode_trap,	            // 122
//         &&opcode_trap,	            // 123
//         &&opcode_trap,	            // 124
//         &&opcode_trap,	            // 125
//         &&opcode_trap,	            // 126
//         &&opcode_trap,	            // 127
//         &&opcode_trap,	            // 128
//         &&opcode_trap,	            // 129
//         &&opcode_trap,	            // 130
//         &&opcode_trap,	            // 131
//         &&opcode_trap,	            // 132
//         &&opcode_trap,	            // 133
//         &&opcode_trap,	            // 134
//         &&opcode_trap,	            // 135
//         &&opcode_trap,	            // 136
//         &&opcode_trap,	            // 137
//         &&opcode_trap,	            // 138
//         &&opcode_trap,	            // 139
//         &&opcode_trap,	            // 140
//         &&opcode_trap,	            // 141
//         &&opcode_trap,	            // 142
//         &&opcode_trap,	            // 143
//         &&opcode_trap,	            // 144
//         &&opcode_trap,	            // 145
//         &&opcode_trap,	            // 146
//         &&opcode_trap,	            // 147
//         &&opcode_trap,	            // 148
//         &&opcode_trap,	            // 149
//         &&opcode_trap,	            // 150
//         &&opcode_trap,	            // 151
//         &&opcode_trap,	            // 152
//         &&opcode_trap,	            // 153
//         &&opcode_trap,	            // 154
//         &&opcode_trap,	            // 155
//         &&opcode_trap,	            // 156
//         &&opcode_trap,	            // 157
//         &&opcode_trap,	            // 158
//         &&opcode_trap,	            // 159
//         &&opcode_trap,	            // 160
//         &&opcode_trap,	            // 161
//         &&opcode_trap,	            // 162
//         &&opcode_trap,	            // 163
//         &&opcode_trap,	            // 164
//         &&opcode_trap,	            // 165
//         &&opcode_trap,	            // 166
//         &&opcode_trap,	            // 167
//         &&opcode_trap,	            // 168
//         &&opcode_trap,	            // 169
//         &&opcode_trap,	            // 170
//         &&opcode_trap,	            // 171
//         &&opcode_trap,	            // 172
//         &&opcode_trap,	            // 173
//         &&opcode_trap,	            // 174
//         &&opcode_trap,	            // 175
//         &&opcode_trap,	            // 176
//         &&opcode_trap,	            // 177
//         &&opcode_trap,	            // 178
//         &&opcode_trap,	            // 179
//         &&opcode_trap,	            // 180
//         &&opcode_trap,	            // 181
//         &&opcode_trap,	            // 182
//         &&opcode_trap,	            // 183
//         &&opcode_trap,	            // 184
//         &&opcode_trap,	            // 185
//         &&opcode_trap,	            // 186
//         &&opcode_trap,	            // 187
//         &&opcode_trap,	            // 188
//         &&opcode_trap,	            // 189
//         &&opcode_trap,	            // 190
//         &&opcode_trap,	            // 191
//         &&opcode_trap,	            // 192
//         &&opcode_trap,	            // 193
//         &&opcode_trap,	            // 194
//         &&opcode_trap,	            // 195
//         &&opcode_trap,	            // 196
//         &&opcode_trap,	            // 197
//         &&opcode_trap,	            // 198
//         &&opcode_trap,	            // 199
//         &&opcode_trap,	            // 200
//         &&opcode_trap,	            // 201
//         &&opcode_trap,	            // 202
//         &&opcode_trap,	            // 203
//         &&opcode_trap,	            // 204
//         &&opcode_trap,	            // 205
//         &&opcode_trap,	            // 206
//         &&opcode_trap,	            // 207
//         &&opcode_trap,	            // 208
//         &&opcode_trap,	            // 209
//         &&opcode_trap,	            // 210
//         &&opcode_trap,	            // 211
//         &&opcode_trap,	            // 212
//         &&opcode_trap,	            // 213
//         &&opcode_trap,	            // 214
//         &&opcode_trap,	            // 215
//         &&opcode_trap,	            // 216
//         &&opcode_trap,	            // 217
//         &&opcode_trap,	            // 218
//         &&opcode_trap,	            // 219
//         &&opcode_trap,	            // 220
//         &&opcode_trap,	            // 221
//         &&opcode_trap,	            // 222
//         &&opcode_trap,	            // 223
//         &&opcode_trap,	            // 224
//         &&opcode_trap,	            // 225
//         &&opcode_trap,	            // 226
//         &&opcode_trap,	            // 227
//         &&opcode_trap,	            // 228
//         &&opcode_trap,	            // 229
//         &&opcode_trap,	            // 230
//         &&opcode_trap,	            // 231
//         &&opcode_trap,	            // 232
//         &&opcode_trap,	            // 233
//         &&opcode_trap,	            // 234
//         &&opcode_trap,	            // 235
//         &&opcode_trap,	            // 236
//         &&opcode_trap,	            // 237
//         &&opcode_trap,	            // 238
//         &&opcode_trap,	            // 239
//         &&opcode_trap,	            // 240
//         &&opcode_trap,	            // 241
//         &&opcode_trap,	            // 242
//         &&opcode_trap,	            // 243
//         &&opcode_trap,	            // 244
//         &&opcode_trap,	            // 245
//         &&opcode_trap,	            // 246
//         &&opcode_trap,	            // 247
//         &&opcode_trap,	            // 248
//         &&opcode_trap,	            // 249
//         &&opcode_trap,	            // 250
//         &&opcode_trap,	            // 251
//         &&opcode_trap,	            // 252
//         &&opcode_trap,	            // 253
//         &&opcode_trap,	            // 254
//         &&opcode_trap,	            // 255
//     };
//     #endif

    uint8_t *code = stream + state->code_start;
    uint8_t *pc = code + func_addr + pc_offset;
    uint8_t opcode;
    function_info_t *func_table = (function_info_t *)( stream + state->func_info_start );
    int32_t *constant_pool = (int32_t *)( stream + state->pool_start );
    int32_t *locals_start = (int32_t *)( stream + state->local_data_start );
    int32_t *local_memory = locals_start;
    int32_t *registers = local_memory;
    int32_t *global_memory = (int32_t *)( stream + state->global_data_start );

    uint16_t func_count = state->func_info_len / sizeof(function_info_t);
    uint16_t current_frame_size = 0xffff;

    for( uint16_t i = 0; i < func_count; i++ ){

        if( func_table[i].addr == func_addr ){

            current_frame_size = func_table[i].frame_size;

            break;
        }
    }

    if( current_frame_size == 0xffff ){

        return VM_STATUS_ERR_FUNC_NOT_FOUND;
    }
    

    // uint16_t dest;
    // uint16_t src;
    // uint16_t call_target;
    // uint16_t call_param;
    // uint16_t call_arg;
    // uint8_t call_param_len;
    // uint16_t result;
    // uint16_t base_addr;
    // uint16_t index;
    // uint16_t count;
    // uint16_t stride;
    // uint16_t temp;
    // uint16_t len;
    // uint8_t type;
    // catbus_hash_t32 hash;
    // catbus_hash_t32 db_hash;
    // vm_string_t *string;
    // int32_t *db_ptr;
    // uint16_t db_ptr_len;
    // uint16_t string_addr;
    
    // #ifdef VM_ENABLE_GFX
    // int32_t value_i32;
    // gfx_palette_t *palette;
    // gfx_pixel_array_t *pix_array;
    // #endif

    // uint8_t *call_stack[VM_MAX_CALL_DEPTH];
    // uint8_t call_depth = 0;
    // int32_t params[8];
    // int32_t indexes[8];

    // #ifdef VM_OPTIMIZED_DECODE
    // decode3_t *decode3;
    // decode2_t *decode2;
    // decodev_t *decodev;
    // decodep_t *decodep;
    // decodep2_t *decodep2;
    // decodep3_t *decodep3;
    // #else
    // uint8_t attr;
    // uint16_t op1;
    // uint16_t op2;
    // uint16_t index_x;
    // uint16_t index_y;
    // uint8_t array;
    // #endif

    int32_t value;
    uint16_t index;
    uint16_t count;
    uint16_t stride;
    int32_t params[8];
    reference_t ref;

    void *ptr_void;
    uint16_t len;

    int32_t *ptr_i32;
    gfx_pixel_array_t *pix_array;

    opcode_1ac_t *opcode_1ac;
    opcode_2ac_t *opcode_2ac;
    opcode_3ac_t *opcode_3ac;
    opcode_4ac_t *opcode_4ac;
    opcode_5ac_t *opcode_5ac;
    opcode_1i_t *opcode_1i;
    opcode_1i1r_t *opcode_1i1r;
    opcode_2i1r_t *opcode_2i1r;
    opcode_1i2r_t *opcode_1i2r;
    opcode_1i2rs_t *opcode_1i2rs;
    opcode_1i3r_t *opcode_1i3r;
    opcode_1i4r_t *opcode_1i4r;
    // opcode_lkp0_t *opcode_lkp0;
    opcode_lkp1_t *opcode_lkp1;
    opcode_lkp2_t *opcode_lkp2;
    opcode_lkp3_t *opcode_lkp3;
    opcode_vector_t *opcode_vector;


    uint8_t call_depth = 0;
    uint8_t *call_stack[VM_MAX_CALL_DEPTH];
    uint16_t frame_stack[VM_MAX_CALL_DEPTH];

    /*
    Storage Pools:

    0 - global
    1 - pixel arrays
    2 - string literals
    3 - function table
    4 -> call depth: local pools on call stack
    */
    #define N_STATIC_POOLS ( 4 )
    int32_t *pools[VM_MAX_CALL_DEPTH + N_STATIC_POOLS];
    memset( pools, 0, sizeof(pools) );

    pools[POOL_GLOBAL]                  = global_memory;
    // pools[POOL_PIXEL_ARRAY]             = (int32_t *)pix_array;
    pools[POOL_PIXEL_ARRAY]             = (int32_t *)0;
    pools[POOL_STRING_LITERALS]         = (int32_t *)0; // not yet implemented
    pools[POOL_FUNCTIONS]               = (int32_t *)func_table;
    pools[N_STATIC_POOLS + call_depth]  = local_memory;

    // return -1;

    #define DISPATCH cycles--; \
                     if( cycles == 0 ){ \
                        return VM_STATUS_ERR_MAX_CYCLES; \
                    } \
                    opcode = *pc; \
                    goto *opcode_table[opcode]


    DISPATCH;

opcode_mov:
    DECODE_2AC;    
    
    registers[opcode_2ac->dest] = registers[opcode_2ac->op1];    

    DISPATCH;

opcode_ldi:
    DECODE_1I1R;    
    
    registers[opcode_1i1r->reg1] = opcode_1i1r->imm1;

    DISPATCH;

opcode_ldc:
    DECODE_1I1R;    
    
    registers[opcode_1i1r->reg1] = constant_pool[opcode_1i1r->imm1];

    DISPATCH;

opcode_ldm:
    DECODE_2AC;    

    ref.n = registers[opcode_2ac->op1];

    registers[opcode_2ac->dest] = *( pools[ref.ref.pool] + ref.ref.addr );
    
    // registers[opcode_2ac->dest] = global_memory[registers[opcode_2ac->op1]];    

    DISPATCH;

// opcode_ldg:
//     DECODE_2AC;    
    
//     registers[opcode_2ac->dest] = global_memory[registers[opcode_2ac->op1]];    

//     DISPATCH;

// opcode_ldl:
//     DECODE_2AC;    
    
//     registers[opcode_2ac->dest] = local_memory[registers[opcode_2ac->op1]];    

//     DISPATCH;

opcode_ldgi:
    DECODE_1I1R;    
    
    registers[opcode_1i1r->reg1] = global_memory[opcode_1i1r->imm1];    

    DISPATCH;

opcode_stm:
    DECODE_2AC;    

    ref.n = registers[opcode_2ac->dest];

    *( pools[ref.ref.pool] + ref.ref.addr ) = registers[opcode_2ac->op1];
    
    // global_memory[registers[opcode_2ac->op1]] = registers[opcode_2ac->dest];    

    DISPATCH;

// opcode_stg:
//     DECODE_2AC;    
    
//     global_memory[registers[opcode_2ac->op1]] = registers[opcode_2ac->dest];    

//     DISPATCH;

// opcode_stl:
//     DECODE_2AC;    
    
//     local_memory[registers[opcode_2ac->op1]] = registers[opcode_2ac->dest];    

//     DISPATCH;

opcode_stgi:
    DECODE_1I1R;    
    
    global_memory[opcode_1i1r->imm1] = registers[opcode_1i1r->reg1];    

    DISPATCH;


opcode_ref:
    DECODE_2I1R;    

    // imm1 = addr
    // imm2 = storage pool

    ref.ref.addr = opcode_2i1r->imm1;

    if( opcode_2i1r->imm2 == POOL_GLOBAL ){

        ref.ref.pool = POOL_GLOBAL;   
    }
    else if( opcode_2i1r->imm2 == POOL_PIXEL_ARRAY ){

        ref.ref.pool = POOL_PIXEL_ARRAY;   
    }
    else if( opcode_2i1r->imm2 == POOL_LOCAL ){

        ref.ref.pool = N_STATIC_POOLS + call_depth;   
    }
    else if( opcode_2i1r->imm2 == POOL_STRING_LITERALS ){

        ref.ref.pool = POOL_STRING_LITERALS;   
    }
    else if( opcode_2i1r->imm2 == POOL_FUNCTIONS ){

        ref.ref.pool = POOL_FUNCTIONS;   
    }
    else{

        return VM_STATUS_BAD_STORAGE_POOL;
    }
    
    registers[opcode_2i1r->reg1] = ref.n;    

    DISPATCH;    

// opcode_lookup0:
//     DECODE_LKP0;


//     // we maybe shoudln't need lookup0....

//     ref.n = registers[opcode_lkp0->ref];

//     registers[opcode_lkp0->dest] = ref.n;

//     DISPATCH;

opcode_lookup1:
    DECODE_LKP1;

    ref.n = registers[opcode_lkp1->ref];

    index = registers[opcode_lkp1->index1];
    count = registers[opcode_lkp1->count1];
    stride = registers[opcode_lkp1->stride1];

    if( count > 0 ){

        index %= count;
        index *= stride;
    }

    ref.ref.addr += index;

    registers[opcode_lkp1->dest] = ref.n;

    DISPATCH;

opcode_lookup2:
    DECODE_LKP2;

    ref.n = registers[opcode_lkp2->ref];

    index = registers[opcode_lkp2->index1];
    count = registers[opcode_lkp2->count1];
    stride = registers[opcode_lkp2->stride1];

    if( count > 0 ){

        index %= count;
        index *= stride;
    }

    ref.ref.addr += index;

    index = registers[opcode_lkp2->index2];
    count = registers[opcode_lkp2->count2];
    stride = registers[opcode_lkp2->stride2];

    if( count > 0 ){

        index %= count;
        index *= stride;
    }

    ref.ref.addr += index;

    registers[opcode_lkp2->dest] = ref.n;

    DISPATCH;

opcode_lookup3:
    DECODE_LKP3;

    ref.n = registers[opcode_lkp3->ref];

    index = registers[opcode_lkp3->index1];
    count = registers[opcode_lkp3->count1];
    stride = registers[opcode_lkp3->stride1];

    if( count > 0 ){

        index %= count;
        index *= stride;
    }

    ref.ref.addr += index;

    index = registers[opcode_lkp3->index2];
    count = registers[opcode_lkp3->count2];
    stride = registers[opcode_lkp3->stride2];

    if( count > 0 ){

        index %= count;
        index *= stride;
    }

    ref.ref.addr += index;

    index = registers[opcode_lkp3->index3];
    count = registers[opcode_lkp3->count3];
    stride = registers[opcode_lkp3->stride3];

    if( count > 0 ){

        index %= count;
        index *= stride;
    }

    ref.ref.addr += index;

    registers[opcode_lkp3->dest] = ref.n;

    DISPATCH;

opcode_dbcall:
    DECODE_2AC;    

    if( registers[opcode_2ac->dest] == __KV__len ){

        #ifdef VM_ENABLE_KV
        catbus_meta_t meta;
        
        if( kv_i8_get_catbus_meta( registers[opcode_2ac->op1], &meta ) < 0 ){

            state->return_val = 0;
        }
        else{

            state->return_val = meta.count + 1;
        }
        #endif
    }
    else{

        state->return_val = 0;
    }

    DISPATCH;

opcode_ldstr:
    DECODE_3AC;    
    
    // not implemented!

    DISPATCH;

opcode_nop:
    DECODE_NOP;    
    
    
    DISPATCH;


opcode_lddb:
    DECODE_1I2RS;    
    
#ifdef VM_ENABLE_KV
    #ifdef VM_ENABLE_CATBUS
    if( catbus_i8_array_get( 
        registers[opcode_1i2rs->reg2], 
        opcode_1i2rs->imm1, 
        0, 
        1, 
        &registers[opcode_1i2rs->reg1] ) < 0 ){

        registers[opcode_1i2rs->reg1] = 0;        
    }
    #else
    // if( kvdb_i8_array_get( registers[opcode_1i2rs->op2], opcode_1i2rs->op1, 0, &registers[opcode_1i2rs->dest], sizeof(registers[opcode_1is2r->dest]registers[opcode_1is2r->dest]) ) < 0 ){

    //     registers[opcode_1i2rs->reg1] = 0;        
    // }
    #endif
#endif

    DISPATCH;

opcode_lddbi:
    DECODE_1I3R;    
    
#ifdef VM_ENABLE_KV
    #ifdef VM_ENABLE_CATBUS
    if( catbus_i8_array_get( 
        registers[opcode_1i3r->reg2], 
        opcode_1i3r->imm1, 
        registers[opcode_1i3r->reg3], 
        1, 
        &registers[opcode_1i3r->reg1] ) < 0 ){

        registers[opcode_1i3r->reg1] = 0;        
    }
    #else
    
    #endif
#endif

    DISPATCH;

opcode_stdb:
    DECODE_1I2RS;    
    
    #ifdef VM_ENABLE_KV
    #ifdef VM_ENABLE_CATBUS

    if( type_b_is_string( opcode_1i2rs->imm1 ) ){

        ref.n = registers[opcode_1i2rs->reg2];
        ptr_void = pools[ref.ref.pool] + ref.ref.addr;   
        len = strlen(ptr_void);
    }
    else{

        ptr_void = &registers[opcode_1i2rs->reg2];
        len = sizeof(registers[opcode_1i2rs->reg2]);
    }

    if( catbus_i8_array_set( 
        registers[opcode_1i2rs->reg1], 
        opcode_1i2rs->imm1, 
        0, 
        1, 
        ptr_void, 
        len ) < 0 ){

        registers[opcode_1i2rs->reg2] = 0;        
    }
    #else
    // if( kvdb_i8_array_get( registers[opcode_1i2rs->op2], opcode_1i2rs->op1, 0, &registers[opcode_1i2rs->dest], sizeof(registers[opcode_1is2r->dest]registers[opcode_1is2r->dest]) ) < 0 ){

    //     registers[opcode_1i2rs->reg1] = 0;        
    // }
    #endif
#endif

    DISPATCH;

opcode_stdbi:
    DECODE_1I3R;    
    
#ifdef VM_ENABLE_KV
    #ifdef VM_ENABLE_CATBUS
    if( catbus_i8_array_set( 
        registers[opcode_1i3r->reg1], 
        opcode_1i3r->imm1, 
        registers[opcode_1i3r->reg3], 
        1, 
        &registers[opcode_1i3r->reg2], 
        sizeof(registers[opcode_1i3r->reg2]) ) < 0 ){

        registers[opcode_1i3r->reg2] = 0;        
    }
    #else
    
    #endif
#endif

    DISPATCH;

opcode_halt:
    return VM_STATUS_HALT;

    DISPATCH;

opcode_assert:
    DECODE_1I1R;    
    
    if( registers[opcode_1i1r->reg1] == FALSE ){

        log_v_warn_P( PSTR("VM Assertion at line: %d"), opcode_1i1r->imm1 );

        return VM_STATUS_ASSERT;        
    }

    DISPATCH;

opcode_print:
    DECODE_1AC;    

    log_v_info_P( PSTR("VM print: %d"), registers[opcode_1ac->op1] );
    
    DISPATCH;

opcode_ret:
    DECODE_1AC;    
    
    state->return_val = registers[opcode_1ac->op1];


    // check if call depth is 0
    // if so, we are exiting the VM
    if( call_depth == 0 ){

        return VM_STATUS_OK;
    }

    // pop PC from call stack
    call_depth--;

    // adjust local memory pointers:
    local_memory -= frame_stack[call_depth] / 4;
    registers = local_memory;

    current_frame_size = frame_stack[call_depth];

    pc = call_stack[call_depth];

    DISPATCH;

opcode_jmp:
    DECODE_1I;    
    
    pc = code + opcode_1i->imm1;

    DISPATCH;    

opcode_jmpz:
    DECODE_1I1R;    

    if( registers[opcode_1i1r->reg1] == 0 ){

        pc = code + opcode_1i1r->imm1;    
    }

    DISPATCH;    

opcode_loop:
    DECODE_1I3R;    

    // imm1: jump target
    // reg1: iterator in
    // reg2: iterator out
    // reg3: stop condition

    // increment iterator:
    // note that inputs and outputs differ!
    value = registers[opcode_1i3r->reg1] + 1;

    // compare against stop:
    if( value < registers[opcode_1i3r->reg3] ){

        pc = code + opcode_1i3r->imm1;
    }

    registers[opcode_1i3r->reg2] = value;
    
    DISPATCH;    

opcode_load_ret_val:
    DECODE_1AC;    
    
    registers[opcode_1ac->op1] = state->return_val;

    DISPATCH;

opcode_call0:
    DECODE_1I;

    index = opcode_1i->imm1;

    CALL_SETUP;

    CALL_SWITCH_CONTEXT;
    CALL_FINISH;

    DISPATCH;

opcode_call1:
    DECODE_1I1R;

    index = opcode_1i1r->imm1;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_1i1r->reg1];

    CALL_SWITCH_CONTEXT;

    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];

    CALL_FINISH;

    DISPATCH;

opcode_call2:
    DECODE_1I2R;

    index = opcode_1i2r->imm1;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_1i2r->reg1];
    params[1] = registers[opcode_1i2r->reg2];

    CALL_SWITCH_CONTEXT;

    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];
    registers[REG_CALL_PARAMS + 1] = params[1];

    CALL_FINISH;

    DISPATCH;

opcode_call3:
    DECODE_1I3R;

    index = opcode_1i3r->imm1;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_1i3r->reg1];
    params[1] = registers[opcode_1i3r->reg2];
    params[2] = registers[opcode_1i3r->reg3];

    CALL_SWITCH_CONTEXT;

    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];
    registers[REG_CALL_PARAMS + 1] = params[1];
    registers[REG_CALL_PARAMS + 2] = params[2];

    CALL_FINISH;

    DISPATCH;

opcode_call4:
    DECODE_1I4R;

    index = opcode_1i4r->imm1;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_1i4r->reg1];
    params[1] = registers[opcode_1i4r->reg2];
    params[2] = registers[opcode_1i4r->reg3];
    params[3] = registers[opcode_1i4r->reg4];

    CALL_SWITCH_CONTEXT;
    
    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];
    registers[REG_CALL_PARAMS + 1] = params[1];
    registers[REG_CALL_PARAMS + 2] = params[2];
    registers[REG_CALL_PARAMS + 3] = params[3];

    CALL_FINISH;

    DISPATCH;


opcode_icall0:
    DECODE_1AC;

    // look up function
    ref.n = registers[opcode_1ac->op1];
    if( ref.ref.pool != POOL_FUNCTIONS ){

        return VM_STATUS_INVALID_FUNC_REF;
    }
    index = ref.ref.addr;

    CALL_SETUP;

    CALL_SWITCH_CONTEXT;
    CALL_FINISH;

    DISPATCH;


opcode_icall1:
    DECODE_2AC;

    // look up function
    ref.n = registers[opcode_2ac->dest];
    if( ref.ref.pool != POOL_FUNCTIONS ){

        return VM_STATUS_INVALID_FUNC_REF;
    }
    index = ref.ref.addr;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_2ac->op1];

    CALL_SWITCH_CONTEXT;

    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];

    CALL_FINISH;

    DISPATCH;

opcode_icall2:
    DECODE_3AC;

    // look up function
    ref.n = registers[opcode_3ac->dest];
    if( ref.ref.pool != POOL_FUNCTIONS ){

        return VM_STATUS_INVALID_FUNC_REF;
    }
    index = ref.ref.addr;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_3ac->op1];
    params[1] = registers[opcode_3ac->op2];

    CALL_SWITCH_CONTEXT;

    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];
    registers[REG_CALL_PARAMS + 1] = params[1];    

    CALL_FINISH;

    DISPATCH;

opcode_icall3:
    DECODE_4AC;

    // look up function
    ref.n = registers[opcode_4ac->dest];
    if( ref.ref.pool != POOL_FUNCTIONS ){

        return VM_STATUS_INVALID_FUNC_REF;
    }
    index = ref.ref.addr;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_4ac->op1];
    params[1] = registers[opcode_4ac->op2];
    params[2] = registers[opcode_4ac->op3];

    CALL_SWITCH_CONTEXT;
    
    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];
    registers[REG_CALL_PARAMS + 1] = params[1];
    registers[REG_CALL_PARAMS + 2] = params[2];

    CALL_FINISH;

    DISPATCH;

opcode_icall4:
    DECODE_5AC;

    // look up function
    ref.n = registers[opcode_5ac->dest];
    if( ref.ref.pool != POOL_FUNCTIONS ){

        return VM_STATUS_INVALID_FUNC_REF;
    }
    index = ref.ref.addr;

    CALL_SETUP;

    // load params
    params[0] = registers[opcode_5ac->op1];
    params[1] = registers[opcode_5ac->op2];
    params[2] = registers[opcode_5ac->op3];
    params[3] = registers[opcode_5ac->op4];

    CALL_SWITCH_CONTEXT;

    // store params
    registers[REG_CALL_PARAMS + 0] = params[0];
    registers[REG_CALL_PARAMS + 1] = params[1];
    registers[REG_CALL_PARAMS + 2] = params[2];
    registers[REG_CALL_PARAMS + 3] = params[3];

    CALL_FINISH;

    DISPATCH;


opcode_lcall0:
    DECODE_1AC;

    LIBCALL( registers[opcode_1ac->op1], 0 );

    // if( vm_lib_i8_libcall_built_in( registers[opcode_1ac->op1], state, &state->return_val, params, 0 ) != 0 ){

    //     #ifdef VM_ENABLE_GFX
    //     // try gfx lib
        
    //     data[result] = gfx_i32_lib_call( hash, params, len );
    //     #endif
    // }
    // else{

    //     // internal lib call completed successfully

    //     // check yield flag
    //     if( state->yield > 0 ){

    //         // check call depth, can only yield from top level functions running as a thread
    //         if( ( call_depth != 0 ) || ( state->current_thread < 0 ) ){

    //             return VM_STATUS_IMPROPER_YIELD;
    //         }

    //         // store PC offset
    //         state->threads[state->current_thread].pc_offset = pc - ( code + func_addr );

    //         // yield!
    //         return VM_STATUS_YIELDED;
    //     }
    // }

    DISPATCH;

opcode_lcall1:
    DECODE_2AC;

    params[0] = registers[opcode_2ac->op1];
    LIBCALL( registers[opcode_2ac->dest], 1 );

    DISPATCH;

opcode_lcall2:
    DECODE_3AC;

    params[0] = registers[opcode_3ac->op1];
    params[1] = registers[opcode_3ac->op2];
    LIBCALL( registers[opcode_3ac->dest], 2 );

    DISPATCH;

opcode_lcall3:
    DECODE_4AC;

    params[0] = registers[opcode_4ac->op1];
    params[1] = registers[opcode_4ac->op2];
    params[2] = registers[opcode_4ac->op3];
    LIBCALL( registers[opcode_4ac->dest], 3 );

    DISPATCH;

opcode_lcall4:
    DECODE_5AC;

    params[0] = registers[opcode_5ac->op1];
    params[1] = registers[opcode_5ac->op2];
    params[2] = registers[opcode_5ac->op3];
    params[3] = registers[opcode_5ac->op4];
    LIBCALL( registers[opcode_5ac->dest], 4 );

    DISPATCH;

opcode_compeq:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] == registers[opcode_3ac->op2];

    DISPATCH;

opcode_compneq:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] != registers[opcode_3ac->op2];

    DISPATCH;

opcode_compgt:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] > registers[opcode_3ac->op2];

    DISPATCH;

opcode_compgte:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] >= registers[opcode_3ac->op2];

    DISPATCH;

opcode_complt:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] < registers[opcode_3ac->op2];

    DISPATCH;

opcode_complte:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] <= registers[opcode_3ac->op2];

    DISPATCH;

opcode_not:
    DECODE_2AC;  

    registers[opcode_2ac->dest] = !registers[opcode_2ac->op1];  
    
    DISPATCH;

opcode_and:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] && registers[opcode_3ac->op2];

    DISPATCH;

opcode_or:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] || registers[opcode_3ac->op2];

    DISPATCH;

opcode_add:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] + registers[opcode_3ac->op2];

    DISPATCH;

opcode_sub:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] - registers[opcode_3ac->op2];

    DISPATCH;

opcode_mul:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = registers[opcode_3ac->op1] * registers[opcode_3ac->op2];

    DISPATCH;

opcode_div:
    DECODE_3AC;    

    if( registers[opcode_3ac->op2] == 0 ){

        registers[opcode_3ac->dest] = 0;
    }
    else{

        registers[opcode_3ac->dest] = registers[opcode_3ac->op1] / registers[opcode_3ac->op2];
    }

    DISPATCH;

opcode_mod:
    DECODE_3AC;    

    if( registers[opcode_3ac->op2] == 0 ){

        registers[opcode_3ac->dest] = 0;
    }
    else{

        registers[opcode_3ac->dest] = registers[opcode_3ac->op1] % registers[opcode_3ac->op2];
    }

    DISPATCH;

opcode_mul_f16:
    DECODE_3AC;    

    registers[opcode_3ac->dest] = ( (int64_t)registers[opcode_3ac->op1] * (int64_t)registers[opcode_3ac->op2] ) / 65536;

    DISPATCH;

opcode_div_f16:
    DECODE_3AC;    

    if( registers[opcode_3ac->op2] == 0 ){

        registers[opcode_3ac->dest] = 0;
    }
    else{

        registers[opcode_3ac->dest] = ( (int64_t)registers[opcode_3ac->op1] * 65536 ) / registers[opcode_3ac->op2];    
    }
    
    DISPATCH;

opcode_conv_i32_to_f16:
    DECODE_2AC;    

    registers[opcode_2ac->dest] = registers[opcode_2ac->op1] * 65536;

    DISPATCH;

opcode_conv_f16_to_i32:
    DECODE_2AC;    

    registers[opcode_2ac->dest] = registers[opcode_2ac->op1] / 65536;

    DISPATCH;

opcode_conv_gfx16_to_f16:
    DECODE_2AC;    

    registers[opcode_2ac->dest] = registers[opcode_2ac->op1];

    // when converting *gfx16* to f16, we map the integer representation of 65535 to 65536.
    // this is because 65535 is our maximum value and 1.0 technically maps to 0.0 in f16, but
    // generally when we use 1.0 what we mean is the maximum value, not the lowest.
    if( registers[opcode_2ac->dest] == 65535 ){

        registers[opcode_2ac->dest] = 65536;
    }

    DISPATCH;

opcode_plookup1:
    DECODE_3AC;

    ref.n = registers[opcode_3ac->op1];

    registers[opcode_3ac->dest] = gfx_u16_calc_index( ref.ref.addr, registers[opcode_3ac->op2], 65535 );
    
    DISPATCH;

opcode_plookup2:
    DECODE_4AC;
    
    ref.n = registers[opcode_4ac->op1];

    registers[opcode_4ac->dest] = gfx_u16_calc_index( ref.ref.addr, registers[opcode_4ac->op2], registers[opcode_4ac->op3] );
    
    DISPATCH;

opcode_pload_attr:
    DECODE_1I2RS;

    ref.n = registers[opcode_1i2rs->reg1];

    if( gfx_i8_get_pixel_array( ref.ref.addr, &pix_array ) == 0 ){

        ptr_i32 = (int32_t *)pix_array;

        registers[opcode_1i2rs->reg2] = ptr_i32[opcode_1i2rs->imm1];
    }
    else{

        registers[opcode_1i2rs->reg2] = 0;
    }

    DISPATCH;

opcode_pstore_attr:
    DECODE_1I2RS;

    ref.n = registers[opcode_1i2rs->reg1];

    if( gfx_i8_get_pixel_array( ref.ref.addr, &pix_array ) == 0 ){

        ptr_i32 = (int32_t *)pix_array;

        ptr_i32[opcode_1i2rs->imm1] = registers[opcode_1i2rs->reg2];
    }

    DISPATCH;

opcode_pstore_hue:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    index = registers[opcode_2ac->dest];

    if( value == 65536 ){

        // this is a shortcut to allow assignment 1.0 to be maximum, instead
        // of rolling over to 0.0.
        value = 65535;
    }

    // wrap hue
    value %= 65536;

    gfx_v_set_hue_1d( value, index );

    DISPATCH;


opcode_pstore_sat:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    index = registers[opcode_2ac->dest];

    // clamp value
    if( value < 0 ){

        value = 0;
    }
    else if( value > 65535 ){

        value = 65535;
    }

    gfx_v_set_sat_1d( value, index );

    DISPATCH;

opcode_pstore_val:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    index = registers[opcode_2ac->dest];

    // clamp value
    if( value < 0 ){

        value = 0;
    }
    else if( value > 65535 ){

        value = 65535;
    }

    gfx_v_set_val_1d( value, index );

    DISPATCH;

opcode_pstore_hs_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    index = registers[opcode_2ac->dest];

    // clamp value
    if( value < 0 ){

        value = 0;
    }
    else if( value > 65535 ){

        value = 65535;
    }

    gfx_v_set_hs_fade_1d( value, index );

    DISPATCH;

opcode_pstore_v_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    index = registers[opcode_2ac->dest];

    // clamp value
    if( value < 0 ){

        value = 0;
    }
    else if( value > 65535 ){

        value = 65535;
    }

    gfx_v_set_v_fade_1d( value, index );

    DISPATCH;

opcode_vstore_hue:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    if( value == 65536 ){
        
        // this is a shortcut to allow assignment 1.0 to be maximum, instead
        // of rolling over to 0.0.
        value = 65535;
    }

    gfx_v_array_move( ref.ref.addr, PIX_ATTR_HUE, value );

    DISPATCH;

opcode_vstore_sat:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_move( ref.ref.addr, PIX_ATTR_SAT, value );

    DISPATCH;

opcode_vstore_val:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_move( ref.ref.addr, PIX_ATTR_VAL, value );

    DISPATCH;

opcode_vstore_hs_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_move( ref.ref.addr, PIX_ATTR_HS_FADE, value );

    DISPATCH;

opcode_vstore_v_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_move( ref.ref.addr, PIX_ATTR_V_FADE, value );

    DISPATCH;

opcode_pload_hue:
    DECODE_2AC;

    registers[opcode_2ac->dest] = gfx_u16_get_hue_1d( registers[opcode_2ac->op1] );

    DISPATCH;

opcode_pload_sat:
    DECODE_2AC;

    registers[opcode_2ac->dest] = gfx_u16_get_sat_1d( registers[opcode_2ac->op1] );

    DISPATCH;

opcode_pload_val:
    DECODE_2AC;

    registers[opcode_2ac->dest] = gfx_u16_get_val_1d( registers[opcode_2ac->op1] );

    DISPATCH;

opcode_padd_hue:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_hue_1d( index );

    value += registers[opcode_2ac->op1];

    gfx_v_set_hue_1d( value, index );

    DISPATCH;

opcode_padd_sat:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_sat_1d( index );

    value += registers[opcode_2ac->op1];

    gfx_v_set_sat_1d( value, index );

    DISPATCH;

opcode_padd_val:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_val_1d( index );

    value += registers[opcode_2ac->op1];

    gfx_v_set_val_1d( value, index );

    DISPATCH;

opcode_padd_hs_fade:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_hs_fade_1d( index );

    value += registers[opcode_2ac->op1];

    gfx_v_set_hs_fade_1d( value, index );

    DISPATCH;

opcode_padd_v_fade:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_v_fade_1d( index );

    value += registers[opcode_2ac->op1];

    gfx_v_set_v_fade_1d( value, index );

    DISPATCH;

opcode_vadd_hue:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_add( ref.ref.addr, PIX_ATTR_HUE, value );    

    DISPATCH;

opcode_vadd_sat:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_add( ref.ref.addr, PIX_ATTR_SAT, value );    

    DISPATCH;

opcode_vadd_val:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_add( ref.ref.addr, PIX_ATTR_VAL, value );    

    DISPATCH;

opcode_vadd_hs_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_add( ref.ref.addr, PIX_ATTR_HS_FADE, value );    

    DISPATCH;

opcode_vadd_v_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_add( ref.ref.addr, PIX_ATTR_V_FADE, value );    

    DISPATCH;

opcode_psub_hue:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_hue_1d( index );

    value -= registers[opcode_2ac->op1];

    gfx_v_set_hue_1d( value, index );

    DISPATCH;

opcode_psub_sat:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_sat_1d( index );

    value -= registers[opcode_2ac->op1];

    gfx_v_set_sat_1d( value, index );

    DISPATCH;

opcode_psub_val:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_val_1d( index );

    value -= registers[opcode_2ac->op1];

    gfx_v_set_val_1d( value, index );

    DISPATCH;

opcode_psub_hs_fade:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_hs_fade_1d( index );

    value -= registers[opcode_2ac->op1];

    gfx_v_set_hs_fade_1d( value, index );

    DISPATCH;

opcode_psub_v_fade:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_v_fade_1d( index );

    value -= registers[opcode_2ac->op1];

    gfx_v_set_v_fade_1d( value, index );

    DISPATCH;

opcode_vsub_hue:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_sub( ref.ref.addr, PIX_ATTR_HUE, value );    

    DISPATCH;

opcode_vsub_sat:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_sub( ref.ref.addr, PIX_ATTR_SAT, value );    

    DISPATCH;

opcode_vsub_val:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_sub( ref.ref.addr, PIX_ATTR_VAL, value );    

    DISPATCH;

opcode_vsub_hs_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_sub( ref.ref.addr, PIX_ATTR_HS_FADE, value );    

    DISPATCH;

opcode_vsub_v_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_sub( ref.ref.addr, PIX_ATTR_V_FADE, value );    

    DISPATCH;

opcode_pmul_hue:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_hue_1d( index );

    if( opcode_1i2rs->imm1 == CATBUS_TYPE_FIXED16 ){

        value = ( (int64_t)registers[opcode_1i2rs->reg2] * value ) / 65536;
    }
    else{

        value *= registers[opcode_1i2rs->reg2];    
    }

    gfx_v_set_hue_1d( value, index );

    DISPATCH;

opcode_pmul_sat:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_sat_1d( index );

    if( opcode_1i2rs->imm1 == CATBUS_TYPE_FIXED16 ){

        value = ( (int64_t)registers[opcode_1i2rs->reg2] * value ) / 65536;
    }
    else{

        value *= registers[opcode_1i2rs->reg2];    
    }

    gfx_v_set_sat_1d( value, index );

    DISPATCH;

opcode_pmul_val:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_val_1d( index );

    if( opcode_1i2rs->imm1 == CATBUS_TYPE_FIXED16 ){

        value = ( (int64_t)registers[opcode_1i2rs->reg2] * value ) / 65536;
    }
    else{

        value *= registers[opcode_1i2rs->reg2];    
    }

    gfx_v_set_val_1d( value, index );

    DISPATCH;

opcode_pmul_hs_fade:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_hs_fade_1d( index );

    value *= registers[opcode_1i2rs->reg2];

    gfx_v_set_hs_fade_1d( value, index );

    DISPATCH;

opcode_pmul_v_fade:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_v_fade_1d( index );

    value *= registers[opcode_1i2rs->reg2];

    gfx_v_set_v_fade_1d( value, index );

    DISPATCH;

opcode_vmul_hue:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_mul( ref.ref.addr, PIX_ATTR_HUE, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vmul_sat:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_mul( ref.ref.addr, PIX_ATTR_SAT, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vmul_val:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_mul( ref.ref.addr, PIX_ATTR_VAL, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vmul_hs_fade:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_mul( ref.ref.addr, PIX_ATTR_HS_FADE, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vmul_v_fade:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_mul( ref.ref.addr, PIX_ATTR_V_FADE, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_pdiv_hue:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_hue_1d( index );

    if( registers[opcode_1i2rs->reg2] == 0 ){

        value = 0;
    }
    else{

        if( opcode_1i2rs->imm1 == CATBUS_TYPE_FIXED16 ){

            value = ( (int64_t)value * 65536 ) / registers[opcode_1i2rs->reg2];
        }
        else{

            value /= registers[opcode_1i2rs->reg2];    
        }
    }

    gfx_v_set_hue_1d( value, index );

    DISPATCH;

opcode_pdiv_sat:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_sat_1d( index );

    if( registers[opcode_1i2rs->reg2] == 0 ){

        value = 0;
    }
    else{

        if( opcode_1i2rs->imm1 == CATBUS_TYPE_FIXED16 ){

            value = ( (int64_t)value * 65536 ) / registers[opcode_1i2rs->reg2];
        }
        else{

            value /= registers[opcode_1i2rs->reg2];    
        }
    }

    gfx_v_set_sat_1d( value, index );

    DISPATCH;

opcode_pdiv_val:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_val_1d( index );

    if( registers[opcode_1i2rs->reg2] == 0 ){

        value = 0;
    }
    else{

        if( opcode_1i2rs->imm1 == CATBUS_TYPE_FIXED16 ){

            value = ( (int64_t)value * 65536 ) / registers[opcode_1i2rs->reg2];
        }
        else{

            value /= registers[opcode_1i2rs->reg2];    
        }
    }

    gfx_v_set_val_1d( value, index );

    DISPATCH;

opcode_pdiv_hs_fade:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_hs_fade_1d( index );

    if( registers[opcode_1i2rs->reg2] == 0 ){

        value = 0;
    }
    else{

        value /= registers[opcode_1i2rs->reg2];    
    }
    

    gfx_v_set_hs_fade_1d( value, index );

    DISPATCH;

opcode_pdiv_v_fade:
    DECODE_1I2RS;

    index = registers[opcode_1i2rs->reg1];

    value = gfx_u16_get_v_fade_1d( index );

    if( registers[opcode_1i2rs->reg2] == 0 ){

        value = 0;
    }
    else{

        value /= registers[opcode_1i2rs->reg2];    
    }
    

    gfx_v_set_v_fade_1d( value, index );

    DISPATCH;

opcode_vdiv_hue:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_div( ref.ref.addr, PIX_ATTR_HUE, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vdiv_sat:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_div( ref.ref.addr, PIX_ATTR_SAT, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vdiv_val:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_div( ref.ref.addr, PIX_ATTR_VAL, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vdiv_hs_fade:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_div( ref.ref.addr, PIX_ATTR_HS_FADE, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_vdiv_v_fade:
    DECODE_1I2RS;

    value = registers[opcode_1i2rs->reg2];
    ref.n = registers[opcode_1i2rs->reg1];

    gfx_v_array_div( ref.ref.addr, PIX_ATTR_V_FADE, value, opcode_1i2rs->imm1 );    

    DISPATCH;

opcode_pmod_hue:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_hue_1d( index );

    value %= registers[opcode_2ac->op1];

    gfx_v_set_hue_1d( value, index );

    DISPATCH;

opcode_pmod_sat:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_sat_1d( index );

    value %= registers[opcode_2ac->op1];

    gfx_v_set_sat_1d( value, index );

    DISPATCH;

opcode_pmod_val:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_val_1d( index );

    value %= registers[opcode_2ac->op1];

    gfx_v_set_val_1d( value, index );

    DISPATCH;

opcode_pmod_hs_fade:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_hs_fade_1d( index );

    value %= registers[opcode_2ac->op1];

    gfx_v_set_hs_fade_1d( value, index );

    DISPATCH;

opcode_pmod_v_fade:
    DECODE_2AC;

    index = registers[opcode_2ac->dest];

    value = gfx_u16_get_v_fade_1d( index );

    value %= registers[opcode_2ac->op1];

    gfx_v_set_v_fade_1d( value, index );

    DISPATCH;

opcode_vmod_hue:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_mod( ref.ref.addr, PIX_ATTR_HUE, value );    

    DISPATCH;

opcode_vmod_sat:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_mod( ref.ref.addr, PIX_ATTR_SAT, value );    

    DISPATCH;

opcode_vmod_val:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_mod( ref.ref.addr, PIX_ATTR_VAL, value );    

    DISPATCH;

opcode_vmod_hs_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_mod( ref.ref.addr, PIX_ATTR_HS_FADE, value );    

    DISPATCH;

opcode_vmod_v_fade:
    DECODE_2AC;

    value = registers[opcode_2ac->op1];
    ref.n = registers[opcode_2ac->dest];

    gfx_v_array_mod( ref.ref.addr, PIX_ATTR_V_FADE, value );    

    DISPATCH;




opcode_vmov:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->target];
    value = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        ptr_i32[ref.ref.addr + i] = value;
    }

    DISPATCH;

opcode_vadd:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->target];
    value = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        ptr_i32[ref.ref.addr + i] += value;
    }

    DISPATCH;

opcode_vsub:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->target];
    value = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        ptr_i32[ref.ref.addr + i] -= value;
    }

    DISPATCH;

opcode_vmul:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->target];
    value = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    if( opcode_vector->type == CATBUS_TYPE_FIXED16 ){

        for( uint16_t i = 0; i < opcode_vector->length; i++ ){

            ptr_i32[ref.ref.addr + i] = ( (int64_t)ptr_i32[ref.ref.addr + i] * value ) / 65536;
        }    
    }
    else{

        for( uint16_t i = 0; i < opcode_vector->length; i++ ){

            ptr_i32[ref.ref.addr + i] *= value;
        }    
    }

    DISPATCH;

opcode_vdiv:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->target];
    value = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    if( opcode_vector->type == CATBUS_TYPE_FIXED16 ){

        for( uint16_t i = 0; i < opcode_vector->length; i++ ){

            ptr_i32[ref.ref.addr + i] = ( (int64_t)ptr_i32[ref.ref.addr + i] * 65536 ) / value;
        }
    }
    else{

        for( uint16_t i = 0; i < opcode_vector->length; i++ ){

            ptr_i32[ref.ref.addr + i] /= value;
        }
    }

    DISPATCH;

opcode_vmod:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->target];
    value = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        ptr_i32[ref.ref.addr + i] %= value;
    }

    DISPATCH;

opcode_vmin:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    value = INT32_MAX;
    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        if( ptr_i32[ref.ref.addr + i] < value ){

            value = ptr_i32[ref.ref.addr + i];
        }
    }

    registers[opcode_vector->target] = value;

    DISPATCH;

opcode_vmax:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    value = INT32_MIN;
    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        if( ptr_i32[ref.ref.addr + i] > value ){

            value = ptr_i32[ref.ref.addr + i];
        }
    }

    registers[opcode_vector->target] = value;

    DISPATCH;

opcode_vavg:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    value = 0;
    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        value += ptr_i32[ref.ref.addr + i];
    }

    registers[opcode_vector->target] = value / opcode_vector->length;

    DISPATCH;

opcode_vsum:
    DECODE_VECTOR;

    ref.n = registers[opcode_vector->value];
    ptr_i32 = pools[ref.ref.pool];

    value = 0;
    for( uint16_t i = 0; i < opcode_vector->length; i++ ){

        value += ptr_i32[ref.ref.addr + i];
    }

    registers[opcode_vector->target] = value;

    DISPATCH;



opcode_trap:
    
    log_v_critical_P( PSTR("VM TRAP: %x"), opcode );

    return VM_STATUS_TRAP;





// #ifdef OLD_VM    

// opcode_mov:
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     data[decode2->dest] = data[decode2->src];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     data[dest] = data[src];
// #endif
//     DISPATCH;


// opcode_clr:
// #ifdef VM_OPTIMIZED_DECODE
//     dest = *(uint16_t *)pc;
//     pc += sizeof(uint16_t);
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
// #endif    
//     data[dest] = 0;

//     DISPATCH;


// opcode_not:    
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     if( data[decode2->src] == 0 ){

//         data[decode2->dest] = 1;
//     }
//     else{
        
//         data[decode2->dest] = 0;
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     if( data[src] == 0 ){

//         data[dest] = 1;
//     }
//     else{
        
//         data[dest] = 0;
//     }
// #endif
//     DISPATCH;


// opcode_compeq:
// opcode_f16_compeq:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] == data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] == data[op2];
// #endif
//     DISPATCH;


// opcode_compneq:
// opcode_f16_compneq:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] != data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] != data[op2];
// #endif
//     DISPATCH;


// opcode_compgt:
// opcode_f16_compgt:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] > data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] > data[op2];
// #endif
//     DISPATCH;


// opcode_compgte:
// opcode_f16_compgte:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] >= data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] >= data[op2];
// #endif
//     DISPATCH;


// opcode_complt:
// opcode_f16_complt:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] < data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] < data[op2];
// #endif
//     DISPATCH;


// opcode_complte:
// opcode_f16_complte:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] <= data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] <= data[op2];
// #endif
//     DISPATCH;



// opcode_and:
// opcode_f16_and:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] && data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] && data[op2];
// #endif
//     DISPATCH;


// opcode_or:
// opcode_f16_or:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] || data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] || data[op2];
// #endif
//     DISPATCH;


// opcode_add:
// opcode_f16_add:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] + data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] + data[op2];
// #endif
//     DISPATCH;

    
// opcode_sub:
// opcode_f16_sub:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] - data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = data[op1] - data[op2];
// #endif
//     DISPATCH;


// opcode_mul:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = data[decode3->op1] * data[decode3->op2];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;
//     data[dest] = data[op1] * data[op2];
// #endif
//     DISPATCH;


// opcode_div:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     if( data[decode3->op2] != 0 ){

//         data[decode3->dest] = data[decode3->op1] / data[decode3->op2];
//     }
//     else{

//         data[decode3->dest] = 0;   
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     if( data[op2] != 0 ){

//         data[dest] = data[op1] / data[op2];
//     }
//     else{

//         data[dest] = 0;
//     }
// #endif
//     DISPATCH;


// opcode_mod:
// opcode_f16_mod:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     if( data[decode3->op2] != 0 ){

//         data[decode3->dest] = data[decode3->op1] % data[decode3->op2];
//     }
//     else{

//         data[decode3->dest] = 0;   
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     if( data[op2] != 0 ){

//         data[dest] = data[op1] % data[op2];
//     }
//     else{

//         data[dest] = 0;
//     }
// #endif
//     DISPATCH;


// opcode_f16_mul:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->dest] = ( (int64_t)data[decode3->op1] * (int64_t)data[decode3->op2] ) / 65536;
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[dest] = ( (int64_t)data[op1] * (int64_t)data[op2] ) / 65536;
// #endif
//     DISPATCH;


// opcode_f16_div:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     if( data[decode3->op2] != 0 ){

//         data[decode3->dest] = ( (int64_t)data[decode3->op1] * 65536 ) / data[decode3->op2];
//     }
//     else{

//         data[decode3->dest] = 0;
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     if( data[op2] != 0 ){

//         data[dest] = ( (int64_t)data[op1] * 65536 ) / data[op2];
//     }
//     else{

//         data[dest] = 0;
//     }
// #endif
//     DISPATCH;


// opcode_jmp:
// #ifdef VM_OPTIMIZED_DECODE
//     dest = *(uint16_t *)pc;
//     pc += sizeof(uint16_t);
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
// #endif    
//     pc = code + dest;

//     DISPATCH;


// opcode_jmp_if_z:
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     if( data[decode2->src] == 0 ){

//         pc = code + decode2->dest;
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     if( data[src] == 0 ){

//         pc = code + dest;
//     }
// #endif

//     DISPATCH;


// opcode_jmp_if_not_z:
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     if( data[decode2->src] != 0 ){

//         pc = code + decode2->dest;
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     if( data[src] != 0 ){

//         pc = code + dest;
//     }
// #endif    

//     DISPATCH;


// opcode_jmp_if_l_pre_inc:
// #ifdef VM_OPTIMIZED_DECODE
//     decode3 = (decode3_t *)pc;
//     pc += sizeof(decode3_t);

//     data[decode3->op1]++;

//     if( data[decode3->op1] < data[decode3->op2] ){

//         pc = code + decode3->dest;
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     op1 = *pc++;
//     op1 += ( *pc++ ) << 8;
//     op2 = *pc++;
//     op2 += ( *pc++ ) << 8;

//     data[op1]++;

//     if( data[op1] < data[op2] ){

//         pc = code + dest;
//     }
// #endif    
//     DISPATCH;


// opcode_ret:
// #ifdef VM_OPTIMIZED_DECODE
//     dest = *(uint16_t *)pc;
//     pc += sizeof(uint16_t);
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
// #endif

//     // move return val to return register
//     data[RETURN_VAL_ADDR] = data[dest];

//     // check if call depth is 0
//     // if so, we are exiting the VM
//     if( call_depth == 0 ){

//         return VM_STATUS_OK;
//     }

//     // pop PC from call stack
//     call_depth--;

//     if( call_depth > VM_MAX_CALL_DEPTH ){

//         return VM_STATUS_CALL_DEPTH_EXCEEDED;
//     }

//     pc = call_stack[call_depth];
        
//     DISPATCH;


// opcode_call:
    
//     call_target = *pc++;
//     call_target += ( *pc++ ) << 8;

//     call_param_len = *pc++;

//     while( call_param_len > 0 ){
//         call_param_len--;

//         // decode
//         call_param = *pc++;
//         call_param += ( *pc++ ) << 8;
//         call_arg = *pc++;
//         call_arg += ( *pc++ ) << 8;

//         // move param to arg
//         data[call_arg] = data[call_param];
//     }

//     // set up return stack
//     call_stack[call_depth] = pc;
//     call_depth++;

//     if( call_depth > VM_MAX_CALL_DEPTH ){

//         return VM_STATUS_CALL_DEPTH_EXCEEDED;
//     }

//     // call by jumping to target
//     pc = code + call_target;

//     DISPATCH;


// opcode_lcall:
//     hash =  (catbus_hash_t32)(*pc++) << 24;
//     hash |= (catbus_hash_t32)(*pc++) << 16;
//     hash |= (catbus_hash_t32)(*pc++) << 8;
//     hash |= (catbus_hash_t32)(*pc++) << 0;

//     len = *pc++;
    
//     for( uint32_t i = 0; i < len; i++ ){
//         temp = *pc++;
//         temp += ( *pc++ ) << 8;

//         // params[i] = data[temp]; // by value
//         params[i] = temp; // by reference
//     }

//     result = *pc++;
//     result += ( *pc++ ) << 8;

//     // initialize result to 0
//     data[result] = 0;

//     if( vm_lib_i8_libcall_built_in( hash, state, data, &data[result], params, len ) != 0 ){

//         #ifdef VM_ENABLE_GFX
//         // try gfx lib
//         // load params by value
//         for( uint32_t i = 0; i < len; i++ ){

//             params[i] = data[params[i]];
//         }

//         data[result] = gfx_i32_lib_call( hash, params, len );
//         #endif
//     }
//     else{

//         // internal lib call completed successfully

//         // check yield flag
//         if( state->yield > 0 ){

//             // check call depth, can only yield from top level functions running as a thread
//             if( ( call_depth != 0 ) || ( state->current_thread < 0 ) ){

//                 return VM_STATUS_IMPROPER_YIELD;
//             }

//             // store PC offset
//             state->threads[state->current_thread].pc_offset = pc - ( code + func_addr );

//             // yield!
//             return VM_STATUS_YIELDED;
//         }
//     }
    
//     DISPATCH;


// opcode_dbcall:
//     hash =  (catbus_hash_t32)(*pc++) << 24;
//     hash |= (catbus_hash_t32)(*pc++) << 16;
//     hash |= (catbus_hash_t32)(*pc++) << 8;
//     hash |= (catbus_hash_t32)(*pc++) << 0;

//     db_hash =  (catbus_hash_t32)(*pc++) << 24;
//     db_hash |= (catbus_hash_t32)(*pc++) << 16;
//     db_hash |= (catbus_hash_t32)(*pc++) << 8;
//     db_hash |= (catbus_hash_t32)(*pc++) << 0;

//     len = *pc++;

//     for( uint32_t i = 0; i < len; i++ ){
//         temp = *pc++;
//         temp += ( *pc++ ) << 8;

//         params[i] = temp; // by reference
//     }

//     result = *pc++;
//     result += ( *pc++ ) << 8;

//     // initialize result to 0
//     data[result] = 0;

//     // call db func
//     if( hash == __KV__len ){

//         #ifdef VM_ENABLE_KV
//         catbus_meta_t meta;
        
//         if( kv_i8_get_catbus_meta( db_hash, &meta ) < 0 ){

//             data[result] = 0;
//         }
//         else{

//             data[result] = meta.count + 1;
//         }
//         #endif
//     }    
    
//     DISPATCH;


// opcode_index:
//     result = *pc++;
//     result += ( *pc++ ) << 8;

//     base_addr = *pc++;
//     base_addr += ( *pc++ ) << 8;
    
//     data[result] = base_addr;

//     len = *pc++;

//     while( len > 0 ){
//         len--;

//         // decode
//         index = *pc++;
//         index += ( *pc++ ) << 8;

//         count = *pc++;
//         count += ( *pc++ ) << 8;

//         stride = *pc++;
//         stride += ( *pc++ ) << 8;

//         temp = data[index];

//         if( count > 0 ){

//             temp %= count;
//             temp *= stride;
//         }

//         data[result] += temp;
//     }

//     DISPATCH;


// opcode_load_indirect:
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     temp = data[decode2->src];

//     // bounds check
//     if( temp >= state->data_count ){

//         return VM_STATUS_INDEX_OUT_OF_BOUNDS;        
//     }

//     data[decode2->dest] = data[temp];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     temp = data[src];

//     // bounds check
//     if( temp >= state->data_count ){

//         return VM_STATUS_INDEX_OUT_OF_BOUNDS;        
//     }

//     data[dest] = data[temp];
// #endif    
//     DISPATCH;


// opcode_store_indirect:
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     temp = data[decode2->dest];

//     // bounds check
//     if( temp >= state->data_count ){

//         return VM_STATUS_INDEX_OUT_OF_BOUNDS;        
//     }

//     data[temp] = data[decode2->src];
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     temp = data[dest];

//     // bounds check
//     if( temp >= state->data_count ){

//         return VM_STATUS_INDEX_OUT_OF_BOUNDS;        
//     }

//     data[temp] = data[src];
// #endif
//     DISPATCH;


// opcode_assert:
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
    
//     if( data[dest] == FALSE ){

//         #ifndef VM_TARGET_ESP
//         // log_v_debug_P( PSTR("VM assertion failed") );
//         #endif
//         return VM_STATUS_ASSERT;
//     }

//     DISPATCH;


// opcode_halt:
//     return VM_STATUS_HALT;

//     DISPATCH;


// opcode_vmov:
// #ifdef VM_OPTIMIZED_DECODE
//     decodev = (decodev_t *)pc;
//     pc += sizeof(decodev_t);

//     // deference pointer
//     dest = data[decodev->dest];

//     for( uint16_t i = 0; i < decodev->len; i++ ){

//         data[dest + i] = data[decodev->src];
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
//     len = *pc++;
//     len += ( *pc++ ) << 8;
//     type = *pc++;

//     // deference pointer
//     dest = data[dest];

//     for( uint16_t i = 0; i < len; i++ ){

//         data[dest + i] = data[src];
//     }
// #endif
//     DISPATCH;


// opcode_vadd:
// #ifdef VM_OPTIMIZED_DECODE
//     decodev = (decodev_t *)pc;
//     pc += sizeof(decodev_t);

//     // deference pointer
//     dest = data[decodev->dest];

//     for( uint16_t i = 0; i < decodev->len; i++ ){

//         data[dest + i] += data[decodev->src];
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
//     len = *pc++;
//     len += ( *pc++ ) << 8;
//     type = *pc++;

//     // deference pointer
//     dest = data[dest];
    
//     for( uint16_t i = 0; i < len; i++ ){

//         data[dest + i] += data[src];
//     }
// #endif
//     DISPATCH;


// opcode_vsub:
// #ifdef VM_OPTIMIZED_DECODE
//     decodev = (decodev_t *)pc;
//     pc += sizeof(decodev_t);

//     // deference pointer
//     dest = data[decodev->dest];

//     for( uint16_t i = 0; i < decodev->len; i++ ){

//         data[dest + i] -= data[decodev->src];
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
//     len = *pc++;
//     len += ( *pc++ ) << 8;
//     type = *pc++;

//     // deference pointer
//     dest = data[dest];
    
//     for( uint16_t i = 0; i < len; i++ ){

//         data[dest + i] -= data[src];
//     }
// #endif
//     DISPATCH;


// opcode_vmul:
// #ifdef VM_OPTIMIZED_DECODE
//     decodev = (decodev_t *)pc;
//     pc += sizeof(decodev_t);

//     // deference pointer
//     dest = data[decodev->dest];

//     if( decodev->type == CATBUS_TYPE_FIXED16 ){

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] = ( (int64_t)data[dest + i] * data[decodev->src] ) / 65536;
//         }
//     }
//     else{

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] *= data[decodev->src];
//         }
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
//     len = *pc++;
//     len += ( *pc++ ) << 8;
//     type = *pc++;

//     // deference pointer
//     dest = data[dest];
        
//     if( type == CATBUS_TYPE_FIXED16 ){

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] = ( (int64_t)data[dest + i] * data[src] ) / 65536;
//         }
//     }
//     else{

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] *= data[src];
//         }
//     }
// #endif
//     DISPATCH;


// opcode_vdiv:
// #ifdef VM_OPTIMIZED_DECODE
//     decodev = (decodev_t *)pc;
//     pc += sizeof(decodev_t);

//     // deference pointer
//     dest = data[decodev->dest];

//     // check for divide by zero
//     if( data[decodev->src] == 0 ){

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] = 0;
//         }
//     }
//     else if( decodev->type == CATBUS_TYPE_FIXED16 ){

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] = ( (int64_t)data[dest + i] * 65536 ) / data[decodev->src];
//         }
//     }
//     else{

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] /= data[decodev->src];
//         }
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
//     len = *pc++;
//     len += ( *pc++ ) << 8;
//     type = *pc++;

//     // deference pointer
//     dest = data[dest];

//     // check for divide by zero
//     if( data[src] == 0 ){

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] = 0;
//         }
//     }
//     else if( type == CATBUS_TYPE_FIXED16 ){

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] = ( (int64_t)data[dest + i] * 65536 ) / data[src];
//         }
//     }
//     else{

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] /= data[src];
//         }
//     }
// #endif
//     DISPATCH;


// opcode_vmod:
// #ifdef VM_OPTIMIZED_DECODE
//     decodev = (decodev_t *)pc;
//     pc += sizeof(decodev_t);

//     // deference pointer
//     dest = data[decodev->dest];

//     // check for divide by zero
//     if( data[decodev->src] == 0 ){

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] = 0;
//         }
//     }
//     else{

//         for( uint16_t i = 0; i < decodev->len; i++ ){

//             data[dest + i] %= data[decodev->src];
//         }
//     }
// #else
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;
//     len = *pc++;
//     len += ( *pc++ ) << 8;
//     type = *pc++;

//     // deference pointer
//     dest = data[dest];

//     // check for divide by zero
//     if( data[src] == 0 ){

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] = 0;
//         }
//     }
//     else{

//         for( uint16_t i = 0; i < len; i++ ){

//             data[dest + i] %= data[src];
//         }
//     }
// #endif
//     DISPATCH;


// opcode_pmov:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep = (decodep_t *)pc;
//     pc += sizeof(decodep_t);

//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_move( decodep->array, decodep->attr, data[decodep->src] );
//     #endif
// #else
//     array = *pc++;
//     attr = *pc++;
    
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_move( array, attr, data[src] );
//     #endif
// #endif
//     DISPATCH;


// opcode_padd:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep = (decodep_t *)pc;
//     pc += sizeof(decodep_t);

//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_add( decodep->array, decodep->attr, data[decodep->src] );
//     #endif
// #else
//     array = *pc++;
//     attr = *pc++;
    
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_add( array, attr, data[src] );
//     #endif
// #endif    
//     DISPATCH;


// opcode_psub:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep = (decodep_t *)pc;
//     pc += sizeof(decodep_t);

//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_sub( decodep->array, decodep->attr, data[decodep->src] );
//     #endif
// #else
//     array = *pc++;
//     attr = *pc++;
    
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_sub( array, attr, data[src] );
//     #endif
// #endif   
//     DISPATCH;


// opcode_pmul:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep = (decodep_t *)pc;
//     pc += sizeof(decodep_t);

//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_mul( decodep->array, decodep->attr, data[decodep->src] );
//     #endif
// #else
//     array = *pc++;
//     attr = *pc++;
    
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_mul( array, attr, data[src] );
//     #endif
// #endif
//     DISPATCH;


// opcode_pdiv:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep = (decodep_t *)pc;
//     pc += sizeof(decodep_t);

//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_div( decodep->array, decodep->attr, data[decodep->src] );
//     #endif
// #else
//     array = *pc++;
//     attr = *pc++;
    
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_div( array, attr, data[src] );
//     #endif
// #endif
//     DISPATCH;


// opcode_pmod:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep = (decodep_t *)pc;
//     pc += sizeof(decodep_t);

//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_mod( decodep->array, decodep->attr, data[decodep->src] );
//     #endif
// #else
//     array = *pc++;
//     attr = *pc++;
    
//     src = *pc++;
//     src += ( *pc++ ) << 8;
    
//     #ifdef VM_ENABLE_GFX
//     gfx_v_array_mod( array, attr, data[src] );
//     #endif
// #endif
//     DISPATCH;


// opcode_pstore_hue:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep2 = (decodep2_t *)pc;
//     pc += sizeof(decodep2_t);

//     // wraparound to 16 bit range.
// //     // this makes it easy to run a circular rainbow
// //     op1 %= 65536;
//     // ^^^^^^ I don't think we actually need to do this.
//     // gfx will crunch from i32 to u16, which does the mod for free.

//     gfx_v_set_hue( data[decodep2->src], data[decodep2->index_x], data[decodep2->index_y], decodep2->array );

// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX

//     // wraparound to 16 bit range.
// //     // this makes it easy to run a circular rainbow
// //     op1 %= 65536;
//     // ^^^^^^ I don't think we actually need to do this.
//     // gfx will crunch from i32 to u16, which does the mod for free.

//     gfx_v_set_hue( data[src], data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;


// opcode_pstore_sat:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep2 = (decodep2_t *)pc;
//     pc += sizeof(decodep2_t);

//     // load source
//     value_i32 = data[decodep2->src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_sat( value_i32, data[decodep2->index_x], data[decodep2->index_y], decodep2->array );
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     // load source
//     value_i32 = data[src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_sat( value_i32, data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;


// opcode_pstore_val:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep2 = (decodep2_t *)pc;
//     pc += sizeof(decodep2_t);

//     // load source
//     value_i32 = data[decodep2->src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_val( value_i32, data[decodep2->index_x], data[decodep2->index_y], decodep2->array );
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     // load source
//     value_i32 = data[src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_val( value_i32, data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;

// opcode_pstore_pval:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep2 = (decodep2_t *)pc;
//     pc += sizeof(decodep2_t);

//     // load source
//     value_i32 = data[decodep2->src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     // get reference to target pixel array
//     pix_array = (gfx_pixel_array_t *)&data[decodep2->array * sizeof(gfx_pixel_array_t) + PIX_ARRAY_ADDR];

//     // get reference to palette
//     palette = (gfx_palette_t *)&data[pix_array->palette];

//     gfx_v_set_pval( value_i32, data[decodep2->index_x], data[decodep2->index_y], decodep2->array, palette );
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     // load source
//     value_i32 = data[src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     // get reference to target pixel array
//     pix_array = (gfx_pixel_array_t *)&data[array * sizeof(gfx_pixel_array_t) + PIX_ARRAY_ADDR];

//     // get reference to palette
//     palette = (gfx_palette_t *)&data[pix_array->palette];

//     gfx_v_set_pval( value_i32, data[index_x], data[index_y], array, palette );
//     #endif
// #endif    
//     DISPATCH;


// opcode_pstore_vfade:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep2 = (decodep2_t *)pc;
//     pc += sizeof(decodep2_t);

//     // load source
//     value_i32 = data[decodep2->src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_v_fade( value_i32, data[decodep2->index_x], data[decodep2->index_y], decodep2->array );
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     // load source
//     value_i32 = data[src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_v_fade( value_i32, data[index_x], data[index_y], array );
//     #endif
// #endif
//     DISPATCH;


// opcode_pstore_hsfade:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep2 = (decodep2_t *)pc;
//     pc += sizeof(decodep2_t);

//     // load source
//     value_i32 = data[decodep2->src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_hs_fade( value_i32, data[decodep2->index_x], data[decodep2->index_y], decodep2->array );
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     // load source
//     value_i32 = data[src];    

//     // clamp to our 16 bit range.
//     // we will essentially saturate at 0 or 65535,
//     // but will not wraparound
//     if( value_i32 > 65535 ){

//         value_i32 = 65535;
//     }
//     else if( value_i32 < 0 ){

//         value_i32 = 0;
//     }

//     gfx_v_set_hs_fade( value_i32, data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;


// opcode_pload_hue:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_hue( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_hue( data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;


// opcode_pload_sat:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_sat( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_sat( data[index_x], data[index_y], array );
//     #endif
// #endif
//     DISPATCH;


// opcode_pload_val:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_val( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_val( data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;

// opcode_pload_pval:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_pval( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_pval( data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;

// opcode_pload_vfade:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_v_fade( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_v_fade( data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;


// opcode_pload_hsfade:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_hs_fade( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_hs_fade( data[index_x], data[index_y], array );
//     #endif
// #endif
//     DISPATCH;


// opcode_db_store:

//     hash =  (catbus_hash_t32)(*pc++) << 24;
//     hash |= (catbus_hash_t32)(*pc++) << 16;
//     hash |= (catbus_hash_t32)(*pc++) << 8;
//     hash |= (catbus_hash_t32)(*pc++) << 0;

//     len = *pc++;

//     indexes[0] = 0;

//     for( uint32_t i = 0; i < len; i++ ){

//         index = *pc++;
//         index += ( *pc++ ) << 8;

//         indexes[i] = data[index];
//     }
    
//     type = *pc++;

//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     if( type_b_is_string( type ) ){

//         // special handling for string types

//         // load string address
//         string_addr = data[src];

//         // load string header
//         string = (vm_string_t *)&data[string_addr];

//         db_ptr = &data[string->addr + 1]; // actual string starts 1 word after header
//         db_ptr_len = string->length;
//     }
//     else{

//         db_ptr = &data[src];
//         db_ptr_len = sizeof(data[src]);
//     }

//     #ifdef VM_ENABLE_KV
//     #ifdef VM_ENABLE_CATBUS
//     catbus_i8_array_set( hash, type, indexes[0], 1, db_ptr, db_ptr_len );
//     #else
//     kvdb_i8_array_set( hash, type, indexes[0], db_ptr, db_ptr_len );
//     #endif
//     #endif
    
//     DISPATCH;


// opcode_db_load:
//     hash =  (catbus_hash_t32)(*pc++) << 24;
//     hash |= (catbus_hash_t32)(*pc++) << 16;
//     hash |= (catbus_hash_t32)(*pc++) << 8;
//     hash |= (catbus_hash_t32)(*pc++) << 0;

//     len = *pc++;

//     indexes[0] = 0;

//     for( uint32_t i = 0; i < len; i++ ){

//         index = *pc++;
//         index += ( *pc++ ) << 8;

//         indexes[i] = data[index];
//     }
    
//     type = *pc++;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_KV
//     #ifdef VM_ENABLE_CATBUS
//     if( catbus_i8_array_get( hash, type, indexes[0], 1, &data[dest] ) < 0 ){

//         data[dest] = 0;        
//     }
//     #else
//     if( kvdb_i8_array_get( hash, type, indexes[0], &data[dest], sizeof(data[dest]) ) < 0 ){

//         data[dest] = 0;        
//     }
//     #endif
//     #endif
    
//     DISPATCH;


// opcode_conv_i32_to_f16: 
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     data[decode2->dest] = data[decode2->src] * 65536;
// #else       
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     data[dest] = data[src] * 65536;
// #endif
//     DISPATCH;


// opcode_conv_f16_to_i32:
// #ifdef VM_OPTIMIZED_DECODE
//     decode2 = (decode2_t *)pc;
//     pc += sizeof(decode2_t);

//     data[decode2->dest] = data[decode2->src] / 65536;
// #else       
//     dest = *pc++;
//     dest += ( *pc++ ) << 8;
//     src = *pc++;
//     src += ( *pc++ ) << 8;

//     data[dest] = data[src] / 65536;
// #endif    
//     DISPATCH;


// opcode_is_v_fading:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_is_v_fading( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_is_v_fading( data[index_x], data[index_y], array );
//     #endif
// #endif
//     DISPATCH;


// opcode_is_hs_fading:
// #ifdef VM_OPTIMIZED_DECODE
//     decodep3 = (decodep3_t *)pc;
//     pc += sizeof(decodep3_t);

//     #ifdef VM_ENABLE_GFX
//     data[decodep3->dest] = gfx_u16_get_is_hs_fading( data[decodep3->index_x], data[decodep3->index_y], decodep3->array );
//     #endif
// #else
//     array = *pc++;

//     index_x = *pc++;
//     index_x += ( *pc++ ) << 8;
    
//     index_y = *pc++;
//     index_y += ( *pc++ ) << 8;

//     dest = *pc++;
//     dest += ( *pc++ ) << 8;

//     #ifdef VM_ENABLE_GFX
//     data[dest] = gfx_u16_get_is_hs_fading( data[index_x], data[index_y], array );
//     #endif
// #endif    
//     DISPATCH;


// #endif

// opcode_trap:
//     return VM_STATUS_TRAP;
}



int8_t vm_i8_run(
    uint8_t *stream,
    uint16_t func_addr,
    uint16_t pc_offset,
    vm_state_t *state ){

    uint32_t start_time = tmr_u32_get_system_time_us();

    cycles = VM_MAX_CYCLES;

    int32_t *global_data = (int32_t *)( stream + state->global_data_start );

    // load published vars
    vm_publish_t *publish = (vm_publish_t *)&stream[state->publish_start];

    uint32_t count = state->publish_count;

    while( count > 0 ){

        if( !type_b_is_string( publish->type ) ){

            kvdb_i8_get( publish->hash, publish->type, &global_data[publish->addr], sizeof(global_data[publish->addr]) );
        }

        publish++;
        count--;
    }

    #ifdef VM_ENABLE_GFX

    // set pixel arrays
    gfx_v_init_pixel_arrays( (gfx_pixel_array_t *)&stream[state->pix_obj_start], state->pix_obj_count );

    #endif

    // reset yield
    state->yield = 0;

    state->frame_number++;

    state->return_val = 0;

    int8_t status = _vm_i8_run_stream( stream, func_addr, pc_offset, state );

    cycles = VM_MAX_CYCLES - cycles;

    if( cycles > state->max_cycles ){

        state->max_cycles = cycles;        
    }

    // store published vars back to DB
    publish = (vm_publish_t *)&stream[state->publish_start];

    count = state->publish_count;

    while( count > 0 ){

        int32_t *ptr = &global_data[publish->addr];
        uint32_t len = sizeof(global_data[publish->addr]);

        // check if string
        // TODO
        // hack to deal with poor string handling between compiler, VM, and DB
        if( ( publish->type == CATBUS_TYPE_STRING512 ) ||
            ( publish->type == CATBUS_TYPE_STRING64 ) ){

            publish->type = CATBUS_TYPE_STRING64;

            ptr = &global_data[*ptr]; // dereference string
            len = ( *ptr & 0xffff0000 ) >> 16; // second half of first word of string is length
            ptr++;
            len++; // add null terminator

            len = 64;
            char buf[64];
            memset( buf, 0, sizeof(buf) );
            strncpy( buf, (char *)ptr, sizeof(buf) );
            ptr = (int32_t *)buf;
        }
        
        kvdb_i8_set( publish->hash, publish->type, ptr, len );

        publish++;
        count--;
    }

    #ifdef VM_ENABLE_GFX

    gfx_v_delete_pixel_arrays();

    #endif

    state->current_thread = -1;

    // record run time
    uint32_t elapsed = tmr_u32_elapsed_time_us( start_time );
    state->last_elapsed_us = elapsed;

    return status;
}


#define EVENT_LOOP 254
#define EVENT_NONE 255
static uint8_t _get_next_event( uint8_t *stream, vm_state_t *state, uint64_t *next_tick ){

    uint8_t event = EVENT_LOOP;
    uint64_t current_tick = state->tick;
    *next_tick = state->loop_tick;

    for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

        if( state->threads[i].func_addr == 0xffff ){

            continue;
        }

        /*
        TODO:

        Need to fix yields so the thread will delay until all other threads
        get a chance to run.  As it stands, a yield in a loop will
        block all other threads as the thread always indicates it is ready to
        run now.

        */

        uint64_t thread_tick = state->threads[i].tick;

        // check if thread tick is sooner than last one we checked
        if( thread_tick < *next_tick ){

            *next_tick = thread_tick;
            event = i;
        }
    }

    // check if nothing is ready to run
    if( *next_tick > current_tick ){
        
        event = EVENT_NONE;
    }

    return event;
}

uint64_t vm_u64_get_next_tick(
    uint8_t *stream,
    vm_state_t *state ){

    uint64_t next_tick;

    _get_next_event( stream, state, &next_tick );

    return next_tick;
}

int8_t vm_i8_run_tick(
    uint8_t *stream,
    vm_state_t *state,
    int32_t delta_ticks ){

    state->tick += delta_ticks;

    uint32_t elapsed_us = 0;

    int8_t status = VM_STATUS_DID_NOT_RUN;

    while( elapsed_us < ( VM_MAX_RUN_TIME * 1000 ) ){

        uint64_t next_tick;
        uint8_t event = _get_next_event( stream, state, &next_tick );

        if( event == EVENT_NONE ){

            break;
        }
        else if( event == EVENT_LOOP ){

            // run loop
            status = vm_i8_run_loop( stream, state );

            // add frame rate to next loop delay
            state->loop_tick += gfx_u16_get_vm_frame_rate();

            elapsed_us += state->last_elapsed_us;

            if( status != VM_STATUS_OK ){

                goto exit;
            }
        }
        else{ // threads

            uint8_t thread = event;
            state->current_thread = thread;
        
            status = vm_i8_run( stream, state->threads[thread].func_addr, state->threads[thread].pc_offset, state );

            elapsed_us += state->last_elapsed_us;

            state->current_thread = -1;

            if( status == VM_STATUS_OK ){

                // thread returned, kill it
                state->threads[thread].func_addr = 0xffff;
            }
            else if( status == VM_STATUS_YIELDED ){

                status = VM_STATUS_OK;
            }
            else{

                goto exit;
            }
        }
    }


exit:    
    
    // update total elapsed time
    state->last_elapsed_us = elapsed_us;

    return status;
}

int8_t vm_i8_run_init(
    uint8_t *stream,
    vm_state_t *state ){

    state->tick = 0;

    return vm_i8_run( stream, state->init_start, 0, state );
}

int8_t vm_i8_run_loop(
    uint8_t *stream,
    vm_state_t *state ){

    return vm_i8_run( stream, state->loop_start, 0, state );
}

// int32_t vm_i32_get_data( 
//     uint8_t *stream,
//     vm_state_t *state,
//     uint16_t addr ){

//     // bounds check
//     if( addr >= state->data_count ){

//         return 0;
//     }

//     int32_t *data_table = (int32_t *)( stream + state->data_start );

//     return data_table[addr];
// }

// void vm_v_get_data_multi( 
//     uint8_t *stream,
//     vm_state_t *state,
//     uint16_t addr, 
//     uint16_t len,
//     int32_t *dest ){

//     // bounds check
//     if( ( addr + len ) > state->data_count ){

//         return;
//     }

//     int32_t *data_table = (int32_t *)( stream + state->data_start );

//     while( len > 0 ){

//         *dest = data_table[addr];

//         dest++;
//         addr++;
//         len--;
//     }
// }

int32_t* vm_i32p_get_data_ptr( 
    uint8_t *stream,
    vm_state_t *state ){

    return (int32_t *)( stream + state->global_data_start );    
}

// void vm_v_set_data( 
//     uint8_t *stream,
//     vm_state_t *state,
//     uint16_t addr, 
//     int32_t data ){

//     // bounds check
//     if( addr >= state->data_count ){

//         return;
//     }

//     int32_t *data_table = (int32_t *)( stream + state->data_start );

//     data_table[addr] = data;
// }


int8_t vm_i8_check_header( vm_program_header_t *prog_header ){

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

    uint16_t obj_start = sizeof(vm_program_header_t);

    // read keys
    if( ( obj_start % 4 ) != 0 ){

        return VM_STATUS_READ_KEYS_MISALIGN;
    }
    obj_start += prog_header->read_keys_len;

    // write keys
    if( ( obj_start % 4 ) != 0 ){

        return VM_STATUS_WRITE_KEYS_MISALIGN;
    }
    obj_start += prog_header->write_keys_len;

    // publish
    if( ( obj_start % 4 ) != 0 ){

        return VM_STATUS_PUBLISH_VARS_MISALIGN;
    }
    obj_start += prog_header->publish_len;

    // link
    if( ( obj_start % 4 ) != 0 ){

        return VM_STATUS_LINK_MISALIGN;
    }
    obj_start += prog_header->link_len;

    // db
    if( ( obj_start % 4 ) != 0 ){

        return VM_STATUS_DB_MISALIGN;
    }
    obj_start += prog_header->db_len;

    // cron
    if( ( obj_start % 4 ) != 0 ){

        return VM_STATUS_CRON_MISALIGN;
    }
    
    return VM_STATUS_OK;
}

int8_t vm_i8_load_program(
    uint8_t vm_id, 
    char *program_fname, 
    mem_handle_t *handle,
    vm_state_t *state ){

    int8_t status = VM_STATUS_ERROR;
    *handle = -1;

    // return -1;

    // open file
    file_t f = fs_f_open( program_fname, FS_MODE_READ_ONLY );

    if( f < 0 ){

        // try again, adding .fxb extension
        char s[FFS_FILENAME_LEN];
        memset( s, 0, sizeof(s) );
        strlcpy( s, program_fname, sizeof(s) );
        strlcat( s, ".fxb", sizeof(s) );

        f = fs_f_open( s, FS_MODE_READ_ONLY );

        if( f < 0 ){

            status = VM_STATUS_FX_FILE_NOT_FOUND;
            goto error;
        }
    }

    fs_v_seek( f, 0 );    
    int32_t check_len = fs_i32_get_size( f ) - sizeof(uint32_t);

    uint32_t computed_file_hash = hash_u32_start();

    // check file hash
    while( check_len > 0 ){

        uint8_t chunk[512];

        uint16_t copy_len = sizeof(chunk);

        if( copy_len > check_len ){

            copy_len = check_len;
        }

        int16_t read = fs_i16_read( f, chunk, copy_len );

        if( read < 0 ){

            status = VM_STATUS_ERROR;

            // this should not happen. famous last words.
            goto error;
        }

        // update hash
        computed_file_hash = hash_u32_partial( computed_file_hash, chunk, copy_len );
        
        check_len -= read;
    }

    // read file hash
    uint32_t file_hash = 0;
    fs_i16_read( f, (uint8_t *)&file_hash, sizeof(file_hash) );

    // check hashes
    if( file_hash != computed_file_hash ){

        status = VM_STATUS_ERR_BAD_FILE_HASH;
        goto error;
    }

    // read header
    fs_v_seek( f, 0 );
    vm_program_header_t header;
    fs_i16_read( f, (uint8_t *)&header, sizeof(header) );

    status = vm_i8_check_header( &header );

    if( status < 0 ){

        goto error;
    }

    uint32_t vm_size = header.code_len + 
                       header.func_info_len + 
                       header.global_data_len + 
                       header.local_data_len + 
                       header.constant_len +
                       header.read_keys_len + 
                       header.write_keys_len +
                       header.publish_len + 
                       header.link_len +
                       header.db_len +
                       header.cron_len +
                       header.pix_obj_len;

    // allocate memory
    *handle = mem2_h_alloc2( vm_size, MEM_TYPE_VM_DATA );

    if( *handle < 0 ){

        status = VM_STATUS_LOAD_ALLOC_FAIL;   
        goto error;
    }

    // init VM state:    
    memset( state, 0, sizeof(vm_state_t) );

    state->vm_id = vm_id;

    // reset thread state
    for( uint8_t i = 0; i < cnt_of_array(state->threads); i++ ){

        state->threads[i].func_addr = 0xffff;
        state->threads[i].tick      = 0;
    }

    state->current_thread = -1;
    
    state->program_name_hash = header.program_name_hash;            

    state->init_start = header.init_start;
    state->loop_start = header.loop_start;

    uint16_t obj_start = 0;

    state->func_info_start      = obj_start;
    state->func_info_len        = header.func_info_len;
    obj_start += header.func_info_len;

    state->pix_obj_count = header.pix_obj_len / sizeof(gfx_pixel_array_t);
    state->pix_obj_start = obj_start;
    obj_start += header.pix_obj_len;

    state->read_keys_count = header.read_keys_len / sizeof(uint32_t);
    state->read_keys_start = obj_start;
    obj_start += header.read_keys_len;

    state->write_keys_count = header.write_keys_len / sizeof(uint32_t);
    state->write_keys_start = obj_start;
    obj_start += header.write_keys_len;

    state->publish_count = header.publish_len / sizeof(vm_publish_t);
    state->publish_start = obj_start;
    obj_start += header.publish_len;

    state->link_count = header.link_len / sizeof(link_t);
    state->link_start = obj_start;
    obj_start += header.link_len;

    state->db_count = header.db_len / sizeof(catbus_meta_t);
    state->db_start = obj_start;
    obj_start += header.db_len;

    state->cron_count = header.cron_len / sizeof(cron_t);
    state->cron_start = obj_start;
    obj_start += header.cron_len;

    // set up final items for VM execution

    state->pool_start           = obj_start;
    state->pool_len             = header.constant_len;

    state->code_start           = state->pool_start + state->pool_len;

    state->local_data_start     = state->code_start + header.code_len;
    state->local_data_len       = header.local_data_len;
    state->local_data_count     = state->local_data_len / DATA_LEN;

    state->global_data_start    = state->local_data_start + header.local_data_len;
    state->global_data_len      = header.global_data_len;
    state->global_data_count    = state->global_data_len / DATA_LEN;

    uint8_t *stream = mem2_vp_get_ptr( *handle );

    if( ( (uint32_t)stream % 4 ) != 0 ){

        status = VM_STATUS_STREAM_MISALIGN;
        goto error;
    }

    // **********************
    // load function table:
    // **********************
    uint8_t *obj_ptr = stream;

    // load data from file
    int16_t read_len = fs_i16_read( f, obj_ptr, header.func_info_len );

    if( read_len != header.func_info_len ){

        status = VM_STATUS_ERR_BAD_FILE_READ;
        goto error;
    }

    obj_ptr += header.func_info_len;

    // ******************
    // load objects:
    // ******************
    if( header.pix_obj_len > 0 ){

        if( fs_i16_read( f, obj_ptr, header.pix_obj_len ) != header.pix_obj_len ){

            status = VM_STATUS_ERR_BAD_FILE_READ;
            goto error;
        }       

        obj_ptr += header.pix_obj_len;
    }

    // ******************
    // load published vars:
    // ******************
    if( header.publish_len > 0 ){

        for( uint16_t i = 0; i < state->publish_count; i++ ){

            vm_publish_t *publish = (vm_publish_t *)obj_ptr;

            if( fs_i16_read( f, (uint8_t *)obj_ptr, sizeof(vm_publish_t) ) != sizeof(vm_publish_t) ){

                status = VM_STATUS_ERR_BAD_FILE_READ;
                goto error;
            }   

            if( publish->addr >= state->global_data_count ){

                status = VM_STATUS_BAD_PUBLISH_ADDR;
                goto error;
            }

            kvdb_i8_add( publish->hash, publish->type, 1, 0, 0 );
            kvdb_v_set_tag( publish->hash, ( 1 << vm_id ) );

            obj_ptr += sizeof(vm_publish_t);
        }
    }

    
    // ******************
    // load constant pool:
    // ******************

    // check alignment
    if( ( (uint32_t)obj_ptr % 4 ) != 0 ){

        status = VM_STATUS_POOL_MISALIGN;
        goto error;
    }

    // check magic number
    uint32_t pool_magic = 0;
    fs_i16_read( f, &pool_magic, sizeof(pool_magic) );

    if( pool_magic != POOL_MAGIC ){

        status = VM_STATUS_ERR_BAD_POOL_MAGIC;
        goto error;
    }

    // load data from file
    read_len = fs_i16_read( f, obj_ptr, header.constant_len );

    if( read_len != header.constant_len ){

        status = VM_STATUS_ERR_BAD_FILE_READ;
        goto error;
    }

    // ******************
    // load code:
    // ******************
    uint8_t *code_ptr = stream + state->code_start;

    // check alignment
    if( ( (uint32_t)code_ptr % 4 ) != 0 ){

        status = VM_STATUS_CODE_MISALIGN;
        goto error;
    }

    // check magic number
    uint32_t code_magic = 0;
    fs_i16_read( f, &code_magic, sizeof(code_magic) );

    if( code_magic != CODE_MAGIC ){

        status = VM_STATUS_ERR_BAD_CODE_MAGIC;
        goto error;
    }

    // load data from file
    read_len = fs_i16_read( f, code_ptr, header.code_len );

    if( read_len != header.code_len ){

        status = VM_STATUS_ERR_BAD_FILE_READ;
        goto error;
    }

    // ******************
    // Stream hash:
    // This is not currently checked, so we skip over it.
    // ******************
    uint32_t stream_hash = 0;
    fs_i16_read( f, &stream_hash, sizeof(stream_hash) );    
    

    // **********************
    // Metadata:
    // set KV names
    // **********************
    // check magic number
    uint32_t meta_magic = 0;
    fs_i16_read( f, &meta_magic, sizeof(meta_magic) );

    if( meta_magic != META_MAGIC ){

        status = VM_STATUS_ERR_BAD_META_MAGIC;
        goto error;
    }

    char meta_string[KV_NAME_LEN];
    memset( meta_string, 0, sizeof(meta_string) );

    // skip first string, it's the script name
    fs_v_seek( f, fs_i32_tell( f ) + sizeof(meta_string) );

    // load meta names to database lookup
    while( fs_i16_read( f, meta_string, sizeof(meta_string) ) == sizeof(meta_string) ){
    
        kvdb_v_set_name( meta_string );
        
        memset( meta_string, 0, sizeof(meta_string) );
    }    


    // **********************
    // Zero out data segments:
    // **********************
    int32_t *local_data_ptr = (int32_t *)( stream + state->local_data_start );

    // check alignment
    if( ( (uint32_t)local_data_ptr % 4 ) != 0 ){

        status = VM_STATUS_DATA_MISALIGN;
        goto error;
    }

    memset( local_data_ptr, 0, header.local_data_len );

    int32_t *global_data_ptr = (int32_t *)( stream + state->global_data_start );

    // check alignment
    if( ( (uint32_t)global_data_ptr % 4 ) != 0 ){

        status = VM_STATUS_DATA_MISALIGN;
        goto error;
    }

    memset( global_data_ptr, 0, header.global_data_len );


    // init RNG seed to device ID
    uint64_t rng_seed;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &rng_seed );

    // make sure seed is never 0 (otherwise RNG will not work)
    if( rng_seed == 0 ){

        rng_seed = 1;
    }

    state->rng_seed = rng_seed;


    state->tick = 0;
    state->loop_tick = 10; // start loop tick with a slight delay


    // init database entries for published var default values
    vm_publish_t *publish = (vm_publish_t *)&stream[state->publish_start];

    uint32_t count = state->publish_count;

    while( count > 0 ){

        if( !type_b_is_string( publish->type ) ){

            kvdb_i8_set( publish->hash, publish->type, &global_data_ptr[publish->addr], sizeof(global_data_ptr[publish->addr]) );
        }

        publish++;
        count--;
    }

    fs_f_close( f );

    return VM_STATUS_OK;

error:
    
    if( f > 0 ){

        fs_f_close( f );
    }

    if( *handle > 0 ){

        mem2_v_free( *handle );
    }
    
    return status;
}

// void vm_v_init_db(
//     uint8_t *stream,
//     vm_state_t *state,
//     uint8_t tag ){

//     int32_t *data = (int32_t *)( stream + state->data_start );

//     // add published vars to DB
//     uint32_t count = state->publish_count;
//     vm_publish_t *publish = (vm_publish_t *)&stream[state->publish_start];

//     while( count > 0 ){

//         int32_t *ptr = &data[publish->addr];
//         uint32_t len = type_u16_size(publish->type);

//         // check if string
//         // TODO
//         // hack to deal with poor string handling between compiler, VM, and DB
//         if( publish->type == CATBUS_TYPE_STRING512 ){

//             publish->type = CATBUS_TYPE_STRING64;

//             ptr = &data[*ptr]; // dereference string
//             len = ( *ptr & 0xffff0000 ) >> 16; // second half of first word of string is length
//             ptr++;
//             len++; // add null terminator

//             len = 64;
//         }

//         kvdb_i8_add( publish->hash, publish->type, 1, ptr, len );
//         kvdb_v_set_tag( publish->hash, tag );

//         publish++;
//         count--;
//     }
// }


// void vm_v_clear_db( uint8_t tag ){

//     // delete existing entries
//     kvdb_v_clear_tag( 0, tag );
// }

