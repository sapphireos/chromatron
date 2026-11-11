#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "unity.h"
#include "keyvalue.h"
#include "kvdb.h"
#include "hash.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

#define H( s )  hash_u32_string( ( char * )( s ) )

// test parameters, registered through the .kv_meta section exactly like
// firmware parameters
static uint16_t test_u16 = 100;
static bool test_bool;
static uint8_t test_arr[4] = { 1, 2, 3, 4 };
static uint8_t test_str[32];
static uint16_t test_persist = 7;

KV_SECTION_META kv_meta_t test_kv[] = {
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &test_u16,       0,                   "kvtest_u16" },
    { CATBUS_TYPE_BOOL,       0, 0,                &test_bool,      0,                   "kvtest_bool" },
    { CATBUS_TYPE_UINT8,      3, 0,                test_arr,       0,                   "kvtest_arr" },
    { CATBUS_TYPE_STRING32,   0, 0,                test_str,       0,                   "kvtest_str" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &test_persist,  0,                   "kvtest_persist" },
};

// kv and its dynamic db (kvdb) must both be initialized before use, same
// order as the firmware boot path.  Init happens once, in the first
// test; the later tests run against that state.
static void kv_sys_init( void ){

    kvdb_v_init();
    kv_v_init();
}

void test_kv_registered_defaults( void ){

    // fresh boot: drop any persist files left by a previous run before
    // initializing.  This must happen exactly once per process -
    // kv_v_init() orphans its hash index on re-init (it never frees
    // ram_index, ASAN confirms the leak) and the persist pass never
    // rewrites the file header, so a deleted-then-re-inited file is
    // invalid.  The firmware has the same shape: one init per boot.
    remove( "kv_data" );
    remove( "kv_names" );

    kv_sys_init();

    uint16_t v;
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_get( H( "kvtest_u16" ), &v, sizeof(v) ) );
    TEST_ASSERT_EQUAL_UINT16( 100, v );

    bool b;
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_get( H( "kvtest_bool" ), &b, sizeof(b) ) );
    TEST_ASSERT_EQUAL_INT16( 0, ( int16_t )b );

    // type and length are exposed for the fixed parameter
    TEST_ASSERT_EQUAL_UINT16( CATBUS_TYPE_UINT16, kv_i8_type( H( "kvtest_u16" ) ) );
    TEST_ASSERT_EQUAL_INT16( 2, kv_i16_len( H( "kvtest_u16" ) ) );

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_NOT_FOUND,
                            kv_i8_get( H( "not_a_parameter" ), &v, sizeof(v) ) );
}

void test_kv_set_get_roundtrip( void ){

    uint16_t v = 4321;

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_set( H( "kvtest_u16" ), &v, sizeof(v) ) );
    TEST_ASSERT_EQUAL_UINT16( 4321, test_u16 );

    uint16_t out;
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_get( H( "kvtest_u16" ), &out, sizeof(out) ) );
    TEST_ASSERT_EQUAL_UINT16( 4321, out );

    // set with the wrong length is rejected, variable untouched
    uint8_t small = 5;

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_TYPE_MISMATCH,
                            kv_i8_set( H( "kvtest_u16" ), &small, sizeof(small) ) );
    TEST_ASSERT_EQUAL_UINT16( 4321, test_u16 );

    // boolean helpers
    bool t = true;

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_set( H( "kvtest_bool" ), &t, sizeof(t) ) );
    TEST_ASSERT_TRUE( kv_b_get_boolean( H( "kvtest_bool" ) ) );
}

void test_kv_array_set_get( void ){

    uint8_t one = 9;

    // single element
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_array_set( H( "kvtest_arr" ), 2, 1, &one, sizeof(one) ) );
    TEST_ASSERT_EQUAL_UINT8( 9, test_arr[2] );

    uint8_t out;
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_array_get( H( "kvtest_arr" ), 2, 1, &out, sizeof(out) ) );
    TEST_ASSERT_EQUAL_UINT8( 9, out );

    // the whole array at once (index 0, count 0)
    uint8_t all[4] = { 7, 8, 9, 10 };

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_array_set( H( "kvtest_arr" ), 0, 0, all, sizeof(all) ) );
    TEST_ASSERT_EQUAL_MEMORY( all, test_arr, sizeof(all) );

    uint8_t all_out[4];

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_array_get( H( "kvtest_arr" ), 0, 0, all_out, sizeof(all_out) ) );
    TEST_ASSERT_EQUAL_MEMORY( all, all_out, sizeof(all) );

    // index past the end is rejected
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OUT_OF_BOUNDS,
                            kv_i8_array_set( H( "kvtest_arr" ), 4, 1, &one, sizeof(one) ) );
}

void test_kv_string_and_names( void ){

    // string parameter: the whole fixed-size buffer is set
    char s[32];

    memset( s, 0, sizeof(s) );
    strcpy( s, "hello" );

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_set( H( "kvtest_str" ), s, sizeof(s) ) );

    char out[32];

    memset( out, 0, sizeof(out) );
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_get( H( "kvtest_str" ), out, sizeof(out) ) );
    TEST_ASSERT_EQUAL_STRING( "hello", out );

    // fixed-parameter names come straight from the .kv_meta section
    char fixed_name[KV_NAME_LEN];

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_get_name( H( "kvtest_u16" ), fixed_name ) );
    TEST_ASSERT_EQUAL_STRING( "kvtest_u16", fixed_name );
}

void test_kv_catbus_meta( void ){

    // catbus meta mirrors the fixed parameter
    catbus_meta_t cmeta;

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_get_catbus_meta( H( "kvtest_u16" ), &cmeta ) );
    TEST_ASSERT_EQUAL_UINT32( H( "kvtest_u16" ), cmeta.hash );
    TEST_ASSERT_EQUAL_UINT16( CATBUS_TYPE_UINT16, cmeta.type );

    // array parameters expose the meta array_len, not the element count
    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK,
                            kv_i8_get_catbus_meta( H( "kvtest_arr" ), &cmeta ) );
    TEST_ASSERT_EQUAL_UINT16( CATBUS_TYPE_UINT8, cmeta.type );
    TEST_ASSERT_EQUAL_UINT8( 3, cmeta.count );
}

// LAST in the file: kv_v_shutdown starts the persist pass on the persist
// thread, which stays alive afterwards, so no later test may re-init.
void test_kv_persist_file( void ){

    uint16_t changed = 4242;

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_set( H( "kvtest_persist" ), &changed, sizeof(changed) ) );

    // non-persisted parameters are not written to the persist file
    uint8_t arr[4] = { 7, 8, 9, 10 };

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_set( H( "kvtest_arr" ), arr, sizeof(arr) ) );

    // persist block layout, mirrored from keyvalue.c (private there):
    // { hash, type, array_len, reserved[4] } + up to 64 bytes of data
    typedef struct{
        uint32_t hash;
        uint8_t type;
        uint8_t array_len;
        uint8_t reserved[4];
    } persist_block_hdr_t;
    #define PERSIST_BLOCK_LEN ( ( int )sizeof(persist_block_hdr_t) + 64 )

    uint8_t buf[PERSIST_BLOCK_LEN];
    persist_block_hdr_t *hdr = ( persist_block_hdr_t * )buf;

    // shutdown runs the persist pass on the persist thread. Its writes
    // are cached in the fs stub and hit disk when the thread closes the
    // file at the end of the pass - so poll the file directly (a plain
    // fopen, never the stub's table: the thread owns that) until the
    // block is on disk
    kv_v_shutdown();

    int persist_found = 0;

    for( int tries = 0; tries < 500 && !persist_found; tries++ ){

        FILE *scan = fopen( "kv_data", "rb" );

        if( scan != NULL ){

            kv_persist_file_header_t header;

            if( fread( &header, 1, sizeof(header), scan ) == sizeof(header) &&
                header.magic == KV_PERSIST_MAGIC ){

                fseek( scan, sizeof(header), SEEK_SET );

                while( fread( buf, 1, PERSIST_BLOCK_LEN, scan ) == PERSIST_BLOCK_LEN ){

                    // wait for the block to hold the value set above: an
                    // earlier auto-persist (triggered by test 2's persist
                    // flag set) may have written the block with the old
                    // value before this test's pass ran
                    if( hdr->hash == H( "kvtest_persist" ) &&
                        *( uint16_t * )( buf + sizeof(persist_block_hdr_t) ) == 4242 ){

                        persist_found = 1;
                        break;
                    }
                }
            }

            fclose( scan );
        }

        if( !persist_found ){

            usleep( 10000 );
        }
    }

    TEST_ASSERT_TRUE( persist_found );

    // read the persist file back and check its real format: header,
    // then fixed-size blocks
    FILE *f = fopen( "kv_data", "rb" );

    TEST_ASSERT_NOT_NULL( f );

    kv_persist_file_header_t header;

    TEST_ASSERT_EQUAL_INT( ( int )sizeof(header), ( int )fread( &header, 1, sizeof(header), f ) );
    TEST_ASSERT_EQUAL_HEX32( KV_PERSIST_MAGIC, header.magic );

    int transient_found = 0;

    while( fread( buf, 1, PERSIST_BLOCK_LEN, f ) == PERSIST_BLOCK_LEN ){

        if( hdr->hash == H( "kvtest_persist" ) ){

            TEST_ASSERT_EQUAL_UINT16( CATBUS_TYPE_UINT16, hdr->type );
            TEST_ASSERT_EQUAL_HEX16( 4242, *( uint16_t * )( buf + sizeof(persist_block_hdr_t) ) );
        }

        if( hdr->hash == H( "kvtest_arr" ) ){

            transient_found = 1;
        }
    }

    fclose( f );
    TEST_ASSERT_FALSE( transient_found );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_kv_registered_defaults );
    RUN_TEST( test_kv_set_get_roundtrip );
    RUN_TEST( test_kv_array_set_get );
    RUN_TEST( test_kv_string_and_names );
    RUN_TEST( test_kv_catbus_meta );
    RUN_TEST( test_kv_persist_file );

    return UNITY_END();
}
