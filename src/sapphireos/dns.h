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

#ifndef _DNS_H
#define _DNS_H

#include "config.h"
#include "ip.h"

#define DNS_SERVER_PORT         53

#define DNS_MAX_NAME_LEN        ( CFG_STR_LEN )
#define DNS_CACHE_ENTRIES       3
#define DNS_QUERY_TIMEOUT       5
#define DNS_QUERY_ATTEMPTS      5

typedef struct __attribute__((packed)){
    uint8_t status;
    ip_addr4_t ip;
    uint32_t ttl;
    char name; // first byte of name
} dns_query_t;


#define DNS_ENTRY_STATUS_EMPTY          0
#define DNS_ENTRY_STATUS_VALID          1
#define DNS_ENTRY_STATUS_INVALID        2
#define DNS_ENTRY_STATUS_RESOLVING      3


typedef struct __attribute__((packed)){
    uint16_t id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority_records;
    uint16_t additional_records;
} dns_header_t;

// flags (and masks):
#define DNS_FLAGS_QR            0b1000000000000000
#define DNS_FLAGS_OPCODE        0b0111100000000000
#define DNS_FLAGS_AA            0b0000010000000000
#define DNS_FLAGS_TC            0b0000001000000000
#define DNS_FLAGS_RD            0b0000000100000000
#define DNS_FLAGS_RA            0b0000000010000000
#define DNS_FLAGS_Z             0b0000000001110000
#define DNS_FLAGS_RCODE         0b0000000000001111

// opcodes (OPCODE)
#define DNS_OPCODE_QUERY        0b0000000000000000 
// other opcodes are available, but this implementation
// only supports queries

// response codes (RCODE)
#define DNS_RCODE_OK                0
#define DNS_RCODE_FORMAT_ERROR      1
#define DNS_RCODE_SERVER_FAILURE    2
#define DNS_RCODE_NAME_ERROR        3
#define DNS_RCODE_NOT_IMPL          4
#define DNS_RCODE_REFUSED           5

// classes
#define DNS_CLASS_IN                1

// types
#define DNS_TYPE_A                  1
#define DNS_TYPE_PTR                12
#define DNS_TYPE_SRV                33
#define DNS_TYPE_TXT                16

// resource record header data
typedef struct __attribute__((packed)){
    // name (variable length)
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
} dns_rr_t;

typedef struct __attribute__((packed)){
    dns_rr_t rr;
    uint32_t addr;
} dns_a_record_t;

typedef struct __attribute__((packed)){
    dns_rr_t rr;
    // name (variable length)
} dns_ptr_record_t;

typedef struct __attribute__((packed)){
    dns_rr_t rr;
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    // target name (variable length)
} dns_srv_record_t;

typedef struct __attribute__((packed)){
    dns_rr_t rr;
    // text (variable length)
} dns_txt_record_t;

typedef struct __attribute__((packed)){
    uint16_t id;
    uint32_t ttl;
    ip_addr4_t ip;
} dns_parsed_a_record_t;


void dns_v_init( void );
int8_t dns_i8_add_entry( char *name );
ip_addr4_t dns_a_query( char *name );

int8_t dns_i8_parse_A_record( void *response, uint16_t response_len, dns_parsed_a_record_t *record );


#endif

