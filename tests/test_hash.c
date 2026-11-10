#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "hash.h"
#include "vectors.h"

// string literals to the non-const char* SUT API
#define H32(s) hash_u32_string((char *)(s))
#define H64(s) hash_u64_string((char *)(s))

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

// published FNV-1a test vectors
void test_fnv1a32_known_vectors( void ){

    TEST_ASSERT_EQUAL_HEX32( 0xe40c292c, H32( "a" ) );
    TEST_ASSERT_EQUAL_HEX32( 0xbf9cf968, H32( "foobar" ) );
}

void test_fnv1a64_known_vectors( void ){

    TEST_ASSERT_EQUAL_HEX64( 0xaf63dc4c8601ec8cULL, H64( "a" ) );
    TEST_ASSERT_EQUAL_HEX64( 0x85944171f73967e8ULL, H64( "foobar" ) );
}

// the API special-cases empty input to 0 (FNV basis would be the
// "hash of nothing" - the firmware relies on 0 meaning "unhashed")
void test_empty_input_returns_zero( void ){

    uint8_t empty = 0;

    TEST_ASSERT_EQUAL_UINT32( 0, hash_u32_data( &empty, 0 ) );
    TEST_ASSERT_EQUAL_UINT64( 0, hash_u64_data( &empty, 0 ) );
    TEST_ASSERT_EQUAL_UINT32( 0, H32( "" ) );
    TEST_ASSERT_EQUAL_UINT64( 0, H64( "" ) );
}

// deterministic, and different inputs give different outputs
void test_deterministic( void ){

    TEST_ASSERT_EQUAL_UINT32( H32( "test_key" ), H32( "test_key" ) );
    TEST_ASSERT_NOT_EQUAL( H32( "test_key" ), H32( "test_key2" ) );
    TEST_ASSERT_NOT_EQUAL( H64( "test_key" ), H64( "test_key2" ) );
}

// incremental composition: start + partial chunks == whole string
void test_partial_composition_32( void ){

    const uint8_t s[] = "foobar";

    uint32_t whole = H32( "foobar" );

    uint32_t parts = hash_u32_start();
    parts = hash_u32_partial( parts, (uint8_t *)s, 3 );
    parts = hash_u32_partial( parts, (uint8_t *)s + 3, 3 );
    TEST_ASSERT_EQUAL_UINT32( whole, parts );

    uint32_t singles = hash_u32_start();
    for( int i = 0; i < 6; i++ ){
        singles = hash_u32_single( singles, s[i] );
    }
    TEST_ASSERT_EQUAL_UINT32( whole, singles );
}

void test_partial_composition_64( void ){

    const uint8_t s[] = "foobar";

    uint64_t whole = H64( "foobar" );

    uint64_t parts = hash_u64_start();
    parts = hash_u64_partial( parts, (uint8_t *)s, 4 );
    parts = hash_u64_partial( parts, (uint8_t *)s + 4, 2 );
    TEST_ASSERT_EQUAL_UINT64( whole, parts );

    uint64_t singles = hash_u64_start();
    for( int i = 0; i < 6; i++ ){
        singles = hash_u64_single( singles, s[i] );
    }
    TEST_ASSERT_EQUAL_UINT64( whole, singles );
}

// golden vectors: kv_hashes.json was captured from the firmware; every key
// hash must still match, or keyvalue lookups will break across versions
void test_kv_vectors( void ){

    char msg[64];

    for( unsigned i = 0; i < KV_VECTORS_COUNT; i++ ){

        snprintf( msg, sizeof(msg), "kv vector mismatch: %s",
                  KV_VECTORS[i].name );
        TEST_ASSERT_EQUAL_UINT32_MESSAGE( KV_VECTORS[i].hash,
                                          H32( KV_VECTORS[i].name ), msg );
    }
}

// golden vectors: catbus_hashes.json (hash -> name), checked the other way
void test_catbus_vectors( void ){

    char msg[64];

    for( unsigned i = 0; i < CATBUS_VECTORS_COUNT; i++ ){

        snprintf( msg, sizeof(msg), "catbus vector mismatch: %s",
                  CATBUS_VECTORS[i].name );
        TEST_ASSERT_EQUAL_UINT32_MESSAGE( CATBUS_VECTORS[i].hash,
                                          H32( CATBUS_VECTORS[i].name ),
                                          msg );
    }
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_fnv1a32_known_vectors );
    RUN_TEST( test_fnv1a64_known_vectors );
    RUN_TEST( test_empty_input_returns_zero );
    RUN_TEST( test_deterministic );
    RUN_TEST( test_partial_composition_32 );
    RUN_TEST( test_partial_composition_64 );
    RUN_TEST( test_kv_vectors );
    RUN_TEST( test_catbus_vectors );

    return UNITY_END();
}
