#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "keyvalue.h"
#include "kvdb.h"
#include "hash.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

#define H( s )  hash_u32_string( ( char * )( s ) )

// one fixed parameter in this binary's .kv_meta section: it exists to
// test the HASH_CONFLICT guard (kvdb cannot add a hash that is
// already a fixed parameter)
static uint16_t kvdb_fixed = 1;

KV_SECTION_META kv_meta_t test_kvdb[] = {
    { CATBUS_TYPE_UINT16, 0, 0, &kvdb_fixed, 0, "kvdb_fixed" },
};

static int notifier_calls;
static catbus_hash_t32 notifier_hash;

static void test_notifier( catbus_hash_t32 hash, catbus_type_t8 type, const void *data ){

    (void)type;
    (void)data;

    notifier_calls++;
    notifier_hash = hash;
}

// kv and its dynamic db (kvdb) must both be initialized before use, same
// order as the firmware boot path.  Init happens once, in the first
// test; the later tests run against that state.
static void kv_sys_init( void ){

    kvdb_v_init();
    kv_v_init();
}

void test_kvdb_add_get_delete( void ){

    // fresh boot: drop any files left by a previous run (see the note
    // in test_keyvalue.c on why this happens before, not after, init)
    remove( "kv_data" );
    remove( "kv_names" );

    kv_sys_init();

    uint32_t dyn = H( "kvdb_add" );
    uint32_t val = 0x12345678;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_add( dyn, CATBUS_TYPE_UINT32, 1, &val, sizeof(val) ) );

    uint32_t out = 0;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_get( dyn, CATBUS_TYPE_UINT32, &out, sizeof(out) ) );
    TEST_ASSERT_EQUAL_HEX32( 0x12345678, out );

    // db size and count grow with dynamic entries
    uint16_t size_before = kvdb_u16_db_size();
    uint16_t count_before = kvdb_u16_count();

    uint32_t dyn2 = H( "kvdb_add2" );
    uint32_t val2 = 1;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_add( dyn2, CATBUS_TYPE_UINT32, 1, &val2, sizeof(val2) ) );
    TEST_ASSERT_TRUE( kvdb_u16_db_size() >= size_before + 8 );
    TEST_ASSERT_EQUAL_UINT16( count_before + 1, kvdb_u16_count() );

    kvdb_v_delete( dyn2 );
    TEST_ASSERT_EQUAL_UINT16( count_before, kvdb_u16_count() );

    kvdb_v_delete( dyn );
}

void test_kvdb_names( void ){

    uint32_t dyn = H( "kvdb_named" );
    uint32_t val = 0xDEADBEEF;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_add( dyn, CATBUS_TYPE_UINT32, 1, &val, sizeof(val) ) );
    kvdb_v_set_name( "kvdb_named" );

    char name[CATBUS_STRING_LEN];

    memset( name, 0, sizeof(name) );
    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK, kvdb_i8_lookup_name( dyn, name ) );
    TEST_ASSERT_EQUAL_STRING( "kvdb_named", name );

    // the keyvalue layer resolves dynamic names via the kvdb name file
    kv_meta_t meta;

    TEST_ASSERT_EQUAL_INT8( KV_ERR_STATUS_OK, kv_i8_lookup_hash_with_name( dyn, &meta ) );
    TEST_ASSERT_EQUAL_UINT32( dyn, meta.hash );
    TEST_ASSERT_EQUAL_STRING( "kvdb_named", meta.name );

    kvdb_v_delete( dyn );
}

void test_kvdb_index_hash( void ){

    uint32_t dyn = H( "kvdb_index" );
    uint32_t val = 0xAABBCCDD;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_add( dyn, CATBUS_TYPE_UINT32, 1, &val, sizeof(val) ) );

    // index/hash mapping round-trips
    int16_t index = kvdb_i16_get_index_for_hash( dyn );

    TEST_ASSERT_TRUE( index >= 0 );
    TEST_ASSERT_EQUAL_UINT32( dyn, kvdb_h_get_hash_for_index( index ) );

    // unknown hash -> negative index
    TEST_ASSERT_TRUE( kvdb_i16_get_index_for_hash( H( "kvdb_no_such" ) ) < 0 );

    kvdb_v_delete( dyn );
}

void test_kvdb_hash_conflict( void ){

    // a hash registered in the fixed .kv_meta region cannot also be
    // added as a dynamic entry
    uint32_t val = 1;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_HASH_CONFLICT,
                            kvdb_i8_add( H( "kvdb_fixed" ), CATBUS_TYPE_UINT32, 1, &val, sizeof(val) ) );
}

void test_kvdb_notifier( void ){

    notifier_calls = 0;
    notifier_hash = 0;

    uint32_t dyn = H( "kvdb_dyn" );
    uint16_t seed = 10;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_add( dyn, CATBUS_TYPE_UINT16, 1, &seed, sizeof(seed) ) );
    kvdb_v_set_notifier( dyn, test_notifier );

    // changed value -> notifier fires
    uint16_t v = 555;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK, kvdb_i8_set( dyn, CATBUS_TYPE_NONE, &v, sizeof(v) ) );
    TEST_ASSERT_EQUAL_INT( 1, notifier_calls );
    TEST_ASSERT_EQUAL_UINT32( dyn, notifier_hash );

    // unchanged value -> no notify
    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK, kvdb_i8_set( dyn, CATBUS_TYPE_NONE, &v, sizeof(v) ) );
    TEST_ASSERT_EQUAL_INT( 1, notifier_calls );

    kvdb_v_delete( dyn );
}

void test_kvdb_tags( void ){

    uint32_t dyn = H( "kvdb_tag" );
    uint32_t val = 1;
    uint16_t count_before = kvdb_u16_count();

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_OK,
                            kvdb_i8_add( dyn, CATBUS_TYPE_UINT32, 1, &val, sizeof(val) ) );
    TEST_ASSERT_EQUAL_UINT16( count_before + 1, kvdb_u16_count() );

    // tags live on dynamic kvdb entries; clearing the last tag purges
    // the entry
    kvdb_v_set_tag( dyn, 0x01 );
    kvdb_v_clear_tag( dyn, 0x01 );
    TEST_ASSERT_EQUAL_UINT16( count_before, kvdb_u16_count() );

    uint32_t out;

    TEST_ASSERT_EQUAL_INT8( KVDB_STATUS_NOT_FOUND,
                            kvdb_i8_get( dyn, CATBUS_TYPE_UINT32, &out, sizeof(out) ) );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_kvdb_add_get_delete );
    RUN_TEST( test_kvdb_names );
    RUN_TEST( test_kvdb_index_hash );
    RUN_TEST( test_kvdb_hash_conflict );
    RUN_TEST( test_kvdb_notifier );
    RUN_TEST( test_kvdb_tags );

    return UNITY_END();
}
