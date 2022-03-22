// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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


#include "cpu.h"

#include "system.h"
#include "threading.h"
#include "sockets.h"
#include "config.h"
#include "timers.h"
#include "fs.h"
#include "sockets.h"
#include "list.h"

#include "dns.h"

#ifdef ENABLE_WIFI

#define NO_LOGGING
#include "logging.h"

typedef struct{
    socket_t sock;
    list_node_t query;
    uint8_t tries;
} resolver_state_t;

static list_t query_list;

uint16_t build_query( char *name, uint16_t id, void *buf, uint16_t bufsize );

PT_THREAD( dns_thread( pt_t *pt, void *state ) );
PT_THREAD( resolver_thread( pt_t *pt, resolver_state_t *state ) );

static uint32_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &query_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &query_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}


void dns_v_init( void ){

    list_v_init( &query_list );

    thread_t_create( dns_thread,
                     PSTR("dns"),
                     0,
                     0 );

    // create vfile
    fs_f_create_virtual( PSTR("dns_cache"), vfile );
}

thread_t start_resolver( list_node_t query ){

    dns_query_t *query_state = list_vp_get_data( query );

    // check query state
    if( query_state->status == DNS_ENTRY_STATUS_RESOLVING ){

        return -1;
    }

    // set up thread state
    resolver_state_t state;

    state.query = query;

    // start thread
    thread_t thread =  thread_t_create( THREAD_CAST(resolver_thread),
                                         PSTR("dns_resolver"),
                                         &state,
                                         sizeof(state) );

    // check thread creation
    if( thread >= 0 ){

        query_state->status = DNS_ENTRY_STATUS_RESOLVING;
    }

    return thread;
}

int8_t dns_i8_add_entry( char *name ){

    // check for duplicate entries
    list_node_t ln = query_list.head;

    while( ln >= 0 ){

        dns_query_t *query = list_vp_get_data( ln );

        // compare names
        if( strncmp( &query->name, name, DNS_MAX_NAME_LEN ) == 0 ){

            return 0;
        }

        ln = list_ln_next( ln );
    }

    // create new query
    ln = list_ln_create_node2( 0, sizeof(dns_query_t) + strlen( name ), MEM_TYPE_DNS_QUERY );

    // check creation
    if( ln < 0 ){

        return -1;
    }

    // set up query
    dns_query_t *query = list_vp_get_data( ln );

    query->status   = DNS_ENTRY_STATUS_INVALID;
    query->ip       = ip_a_addr(0,0,0,0);
    query->ttl      = 0;

    strcpy( &query->name, name );

    // add to list
    list_v_insert_tail( &query_list, ln );

    return 0;
}

// search DNS cache for name
// if there is no entry, adds it so the resolver will perform a query
ip_addr4_t dns_a_query( char *name ){

    // search cache for entry
    list_node_t ln = query_list.head;

    while( ln >= 0 ){

        dns_query_t *query = list_vp_get_data( ln );

        // compare names
        if( strncmp( &query->name, name, DNS_MAX_NAME_LEN ) == 0 ){

            // check status
            if( query->status == DNS_ENTRY_STATUS_INVALID ){

                // start resolver thread
                start_resolver( ln );
            }

            return query->ip;
        }

        ln = list_ln_next( ln );
    }

    dns_i8_add_entry( name );

    return ip_a_addr(0,0,0,0);
}


PT_THREAD( dns_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // wait until IP is configured
    THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

    // get DNS server config
    ip_addr4_t dns_server;
    cfg_i8_get( CFG_PARAM_DNS_SERVER, &dns_server );

    // if DNS server address is not available
    if( ip_b_is_zeroes( dns_server ) ){

        // kill thread
        THREAD_EXIT( pt );
    }

    while(1){

        TMR_WAIT( pt, 1000 );

        // iterate through queries
        list_node_t ln = query_list.head;

        while( ln >= 0 ){

            dns_query_t *query = list_vp_get_data( ln );

            // check TTL and decrement
            if( query->ttl > 0 ){

                query->ttl--;

            }

            // check if TTL expired
            if( query->ttl == 0 ){

                // check status
                if( query->status == DNS_ENTRY_STATUS_VALID ){

                    // invalidate status
                    query->status = DNS_ENTRY_STATUS_INVALID;
                }
            }

            ln = list_ln_next( ln );
        }
    }

PT_END( pt );
}


PT_THREAD( resolver_thread( pt_t *pt, resolver_state_t *state ) )
{
PT_BEGIN( pt );

    // create socket
    state->sock = sock_s_create( SOS_SOCK_DGRAM );

    // check creation
    if( state->sock < 0 ){

        THREAD_EXIT( pt );
    }

    #ifndef NO_LOGGING
    // get query state
    dns_query_t *query_state = list_vp_get_data( state->query );
    #endif

    log_v_debug_P( PSTR("Resolving: %s"), &query_state->name );

    // set timeout
    sock_v_set_timeout( state->sock, DNS_QUERY_TIMEOUT );

    state->tries = DNS_QUERY_ATTEMPTS;

    while( state->tries > 0 ){

        state->tries--;

        // get query state
        dns_query_t *query_state = list_vp_get_data( state->query );

        uint8_t buf[256];

        // create query
        uint16_t len = build_query( &query_state->name, 0x1234, buf, sizeof(buf) );

        // set up remote address
        sock_addr_t raddr;

        cfg_i8_get( CFG_PARAM_DNS_SERVER, &raddr.ipaddr );
        raddr.port = DNS_SERVER_PORT;

        // send it
        sock_i16_sendto( state->sock, buf, len, &raddr );

        // wait for packet
        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( state->sock ) < 0 );

        // check for timeout
        if( sock_i16_get_bytes_read( state->sock ) < 0 ){

            log_v_debug_P( PSTR("DNS query timed out") );

            continue;
        }

        // parse record
        dns_parsed_a_record_t record;

        if( dns_i8_parse_A_record( sock_vp_get_data( state->sock ),
                                   sock_i16_get_bytes_read( state->sock ),
                                   &record ) == 0 ){

            dns_query_t *query_state = list_vp_get_data( state->query );

            query_state->ttl    = record.ttl;
            query_state->ip     = record.ip;
            query_state->status = DNS_ENTRY_STATUS_VALID;

            log_v_debug_P( PSTR("Query: %s resolved to: %d.%d.%d.%d TTL: %d"),
                           &query_state->name,
                           query_state->ip.ip3,
                           query_state->ip.ip2,
                           query_state->ip.ip1,
                           query_state->ip.ip0,
                           query_state->ttl );

            // if TTL is 0, set it to 1 just so the requesting thread has time
            // to retrieve the address before we set the entry to invalid
            if( query_state->ttl == 0 ){

                query_state->ttl = 1;
            }

            goto done;
        }
        else{

            log_v_debug_P( PSTR("DNS parse error") );
        }
    }

done:
    sock_v_release( state->sock );

    dns_query_t *query_state = list_vp_get_data( state->query );

    // check if resolution failed
    if( query_state->status == DNS_ENTRY_STATUS_RESOLVING ){

        query_state->status = DNS_ENTRY_STATUS_INVALID;
    }

PT_END( pt );
}


// builds a simple query for an A record,
// also requests server recursion
uint16_t build_query( char *name, uint16_t id, void *buf, uint16_t bufsize ){

    uint16_t len = sizeof(dns_header_t) + strnlen(name, bufsize) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);

    // check if buffer is large enough to contain message
    if( len > bufsize ){

        return 0;
    }

    dns_header_t *header = (dns_header_t *)buf;
    
    header->id                  = id;
    header->flags               = HTONS(DNS_FLAGS_RD);
    header->questions           = HTONS(1);
    header->answers             = 0;
    header->authority_records   = 0;
    header->additional_records  = 0;

    bufsize -= ( sizeof(dns_header_t) + sizeof(uint8_t) );

    // get pointer to where we should copy the name to be queried
    char *label = buf + sizeof(dns_header_t) + sizeof(uint8_t);

    // copy
    strlcpy( label, name, bufsize );

    uint8_t *count = (uint8_t *)( label - sizeof(uint8_t) );
    *count = 0;

    while( *label != '\0' ){

        // count length of length until next '.'
        if( *label != '.' ){

            (*count)++;
        }
        else{

            count = (uint8_t *)label;
            *count = 0;
        }

        label++;
    }

    label++;

    *(uint16_t *)label = HTONS(DNS_TYPE_A);
    label += sizeof(uint16_t);
    *(uint16_t *)label = HTONS(DNS_CLASS_IN);

    return len;
}


// parse through a label and return the length of the label.
// this does not parse data from the label, just allows us to
// scan past it.

uint16_t skip_label( void *label, void *end ){

    // check for label compression
    // (starts with 2 1s)
    if( ( *(uint8_t *)label & 0xc000 ) != 0 ){

        return 2;
    }

    // scan to null terminator
    return strnlen( (char *)label, end - label );

    /*
    NOTES:
    This code is commented out since we don't need it, but it is kept here because
    it shows how to parse individual parts of a label.  Each component starts with
    a 1 octet length followed by text.

    www.google.com looks like this:
    03www06google03com00

    The '03', '06', and '00' are single octets shown in hex.
    The end of the name is deliminated by a 0

    // get label length
    uint8_t label_len = (uint8_t *)response;

    // skip past label
    response += label_len + sizeof(uint8_t);

    // check if we get to a null terminator
    if( (uint8_t *)response == 0 ){

    }
    */
}

int8_t dns_i8_parse_A_record( void *response, uint16_t response_len, dns_parsed_a_record_t *record ){

    void *end = response + response_len;

    // get header
    static dns_header_t *header; header = (dns_header_t *)response;

    uint16_t flags = HTONS(header->flags);

    // check if response
    if( ( flags & DNS_FLAGS_QR ) == 0 ){

        // message contains a query, not a response
        return -1;
    }

    // check opcode
    if( ( flags & DNS_FLAGS_OPCODE ) != DNS_OPCODE_QUERY ){

        // message is not a reply to a query
        return -2;
    }

    // check error code
    if( ( flags & DNS_FLAGS_RCODE ) != 0 ){

        // server indicates an error occurred
        return -3;
    }

    // copy ID
    record->id = header->id;

    // check answers (should be at least 1)
    if( HTONS(header->answers) == 0 ){

        // no answers contained in message
        return -4;
    }

    // advance pointer past header
    response += sizeof(dns_header_t);

    uint16_t questions = HTONS(header->questions);
    uint16_t answers = HTONS(header->answers);

    // scan through questions
    while( ( questions > 0 ) && ( response < end ) ){

        response += skip_label( response, end );

        // next 4 octets should be type and class, skip those
        response += sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);

        // parsed 1 question, decrement count
        questions--;
    }

    // check if all questions were parsed
    if( questions > 0 ){

        // reached end of buffer, malformed message
        return -5;
    }

    // answers section
    while( ( answers > 0 ) && ( response < end ) ){

        // skip name part
        response += skip_label( response, end );

        // get pointer to resource record
        static dns_rr_t *rr; rr = (dns_rr_t *)response;

        // advance response pointer past end of record
        response += sizeof(dns_rr_t) + HTONS(rr->rdlength);

        // check type, class, and rdata length (must be 4 for IPv4 address)
        if( ( HTONS(rr->type) == DNS_TYPE_A ) &&
            ( HTONS(rr->class) == DNS_CLASS_IN ) &&
            ( HTONS(rr->rdlength) == sizeof(uint32_t) ) ){

            // copy ttl
            record->ttl = HTONL(rr->ttl);

            // get A record
            dns_a_record_t *a_record = (dns_a_record_t *)rr;

            // copy IP address
            memcpy( &record->ip, &a_record->addr, sizeof(record->ip) );

            // found A record
            return 0;
        }

        answers--;
    }

    // message did not contain an A record
    return -6;
}

#endif