#include <string.h>

#include "unity.h"
#include "catbus_common.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

void test_specific_i64_roundtrip( void ){

    uint16_t u16 = 1234;

    TEST_ASSERT_EQUAL_INT64( 1234, specific_to_i64( CATBUS_TYPE_UINT16, &u16 ) );

    uint16_t u16_back = 0;
    i64_to_specific( 1234, CATBUS_TYPE_UINT16, &u16_back );
    TEST_ASSERT_EQUAL_UINT16( 1234, u16_back );

    int16_t i16 = -1234;

    TEST_ASSERT_EQUAL_INT64( -1234, specific_to_i64( CATBUS_TYPE_INT16, &i16 ) );

    int16_t i16_back = 0;
    i64_to_specific( -1234, CATBUS_TYPE_INT16, &i16_back );
    TEST_ASSERT_EQUAL_INT16( -1234, i16_back );

    // bools go through as 0/1
    bool b = true;

    TEST_ASSERT_EQUAL_INT64( 1, specific_to_i64( CATBUS_TYPE_BOOL, &b ) );

    bool b_back = false;
    i64_to_specific( 42, CATBUS_TYPE_BOOL, &b_back );
    TEST_ASSERT_TRUE( b_back );

    // out-of-range values clamp on write-back
    uint8_t u8 = 0;

    i64_to_specific( 300, CATBUS_TYPE_UINT8, &u8 );
    TEST_ASSERT_EQUAL_UINT8( 255, u8 );

    int8_t i8 = 0;

    i64_to_specific( -300, CATBUS_TYPE_INT8, &i8 );
    TEST_ASSERT_EQUAL_INT8( -128, i8 );

    // a uint64 beyond INT64_MAX clamps on read
    uint64_t u64 = UINT64_MAX;

    TEST_ASSERT_EQUAL_INT64( INT64_MAX, specific_to_i64( CATBUS_TYPE_UINT64, &u64 ) );

    // unsupported types are rejected
    TEST_ASSERT_EQUAL_INT64( -1, specific_to_i64( CATBUS_TYPE_REF, &u16 ) );
}

void test_type_convert_numeric( void ){

    // numeric to numeric goes through the i64 representation; the
    // return value reports whether the destination changed
    uint8_t src8 = 7;

    uint16_t dst16 = 7;
    TEST_ASSERT_EQUAL_INT8( 0, type_i8_convert( CATBUS_TYPE_UINT16, &dst16,
                                                CATBUS_TYPE_UINT8, &src8, sizeof(src8) ) );

    dst16 = 5;
    TEST_ASSERT_EQUAL_INT8( 1, type_i8_convert( CATBUS_TYPE_UINT16, &dst16,
                                                CATBUS_TYPE_UINT8, &src8, sizeof(src8) ) );
    TEST_ASSERT_EQUAL_UINT16( 7, dst16 );

    // int8 to int16 sign extends
    int8_t src8s = -2;
    int16_t dst16s = 0;

    type_i8_convert( CATBUS_TYPE_INT16, &dst16s, CATBUS_TYPE_INT8, &src8s, sizeof(src8s) );
    TEST_ASSERT_EQUAL_INT16( -2, dst16s );

    // uint16 to int16 clamps at INT16_MAX (no two's complement wrap).
    // Note the change-detection return compares the *unclamped* source
    // (60000) against the destination, not the clamped result.
    uint16_t src_u16 = 60000;
    int16_t dst_i16 = 0;

    TEST_ASSERT_EQUAL_INT8( 1, type_i8_convert( CATBUS_TYPE_INT16, &dst_i16,
                                                CATBUS_TYPE_UINT16, &src_u16, sizeof(src_u16) ) );
    TEST_ASSERT_EQUAL_INT16( 32767, dst_i16 );

    // int32 to fixed16 shifts left 16
    int32_t src_i32 = 3;
    fixed16_t dst_f16 = 0;

    type_i8_convert( CATBUS_TYPE_FIXED16, &dst_f16, CATBUS_TYPE_INT32, &src_i32, sizeof(src_i32) );
    TEST_ASSERT_EQUAL_INT32( 3 * 65536, dst_f16 );

    // fixed16 to int32 arithmetic-shifts right 16 (toward -inf):
    // -70000 is -1.068 units, which floors to -2.  As with
    // int32->fixed16 above, the change-detection return compares the
    // shifted source against the *unshifted* destination, so it is not
    // a true "changed?" indicator here.
    fixed16_t src_f16 = -70000;
    int32_t dst_i32 = 0;

    type_i8_convert( CATBUS_TYPE_INT32, &dst_i32, CATBUS_TYPE_FIXED16, &src_f16, sizeof(src_f16) );
    TEST_ASSERT_EQUAL_INT32( -2, dst_i32 );
}

void test_type_convert_string( void ){

    char src[CATBUS_STRING_LEN];

    strcpy( src, "hello" );

    // string to string: dest is zeroed first, then min(dst,src) copied
    char dst[CATBUS_STRING_LEN];

    memset( dst, 0x55, sizeof(dst) );

    type_i8_convert( CATBUS_TYPE_STRING32, dst, CATBUS_TYPE_STRING32, src, strlen( src ) );
    TEST_ASSERT_EQUAL_STRING( "hello", dst );

    // string to numeric: dest is zeroed
    uint16_t n = 0xABCD;

    type_i8_convert( CATBUS_TYPE_UINT16, &n, CATBUS_TYPE_STRING32, src, strlen( src ) );
    TEST_ASSERT_EQUAL_UINT16( 0, n );
}

void test_type_u16_size_meta( void ){

    // (count + 1) * element size; count is the meta array_len field
    catbus_meta_t m = { .hash = 1, .type = CATBUS_TYPE_UINT16, .count = 3 };

    TEST_ASSERT_EQUAL_UINT16( ( 3 + 1 ) * 2, type_u16_size_meta( &m ) );

    m.type = CATBUS_TYPE_UINT8;
    m.count = 3;

    TEST_ASSERT_EQUAL_UINT16( ( 3 + 1 ) * 1, type_u16_size_meta( &m ) );
}

void test_type_fixed16( void ){

    TEST_ASSERT_EQUAL_INT32( 123 * 65536, type_f16_from_i32( 123 ) );
    TEST_ASSERT_EQUAL_INT32( 123, type_i32_from_f16( 123 * 65536 ) );

    // the decimal part is tenths of a unit, added with a positive sign
    // even for a negative integer part
    TEST_ASSERT_EQUAL_INT32( 12 * 65536 + ( 3 * 65536 ) / 10,
                             type_f16_from_decimal( 12, 3 ) );

    TEST_ASSERT_EQUAL_INT32( -12 * 65536 + ( 3 * 65536 ) / 10,
                             type_f16_from_decimal( -12, 3 ) );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_specific_i64_roundtrip );
    RUN_TEST( test_type_convert_numeric );
    RUN_TEST( test_type_convert_string );
    RUN_TEST( test_type_u16_size_meta );
    RUN_TEST( test_type_fixed16 );

    return UNITY_END();
}
