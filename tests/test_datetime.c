#include "unity.h"
#include "bool.h"
#include "datetime.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

// SAPPHIRE_NTP_EPOCH counts seconds from 1900-01-01 (NTP era); the
// anchor below is 2010-05-16T13:28:17 UTC, a Sunday

#define DT_ANCHOR_SECONDS   3483005297UL

void test_datetime_leap_year( void ){

    datetime_t dt;

    datetime_v_get_epoch( &dt );
    dt.year = 1900;
    TEST_ASSERT_FALSE( datetime_b_is_leap_year( &dt ) ); // century, not divisible by 400

    dt.year = 2000;
    TEST_ASSERT_TRUE( datetime_b_is_leap_year( &dt ) );

    dt.year = 2024;
    TEST_ASSERT_TRUE( datetime_b_is_leap_year( &dt ) );

    dt.year = 2023;
    TEST_ASSERT_FALSE( datetime_b_is_leap_year( &dt ) );
}

void test_datetime_seconds_roundtrip( void ){

    datetime_t dt;

    datetime_v_seconds_to_datetime( DT_ANCHOR_SECONDS, &dt );

    TEST_ASSERT_EQUAL_UINT16( 2010, dt.year );
    TEST_ASSERT_EQUAL_UINT8( 5, dt.month );
    TEST_ASSERT_EQUAL_UINT8( 16, dt.day );
    TEST_ASSERT_EQUAL_UINT8( 13, dt.hours );
    TEST_ASSERT_EQUAL_UINT8( 28, dt.minutes );
    TEST_ASSERT_EQUAL_UINT8( 17, dt.seconds );

    // and the conversion inverts
    TEST_ASSERT_EQUAL_UINT32( DT_ANCHOR_SECONDS,
                              datetime_u32_datetime_to_seconds( &dt ) );

    // epoch start
    datetime_v_seconds_to_datetime( 0, &dt );

    TEST_ASSERT_EQUAL_UINT16( 1900, dt.year );
    TEST_ASSERT_EQUAL_UINT8( 1, dt.month );
    TEST_ASSERT_EQUAL_UINT8( 1, dt.day );
}

void test_datetime_iso8601_roundtrip( void ){

    datetime_t dt;

    // parse "2010-05-16T13:28:17" (timezone suffix is ignored by the
    // parser, same as firmware)
    char iso[] = "2010-05-16T13:28:17-05:00";

    datetime_v_parse_iso8601( iso, 20, &dt );

    TEST_ASSERT_EQUAL_UINT16( 2010, dt.year );
    TEST_ASSERT_EQUAL_UINT8( 5, dt.month );
    TEST_ASSERT_EQUAL_UINT8( 16, dt.day );
    TEST_ASSERT_EQUAL_UINT8( 13, dt.hours );
    TEST_ASSERT_EQUAL_UINT8( 28, dt.minutes );
    TEST_ASSERT_EQUAL_UINT8( 17, dt.seconds );

    // format and compare (no timezone is written)
    char out[32];

    datetime_v_to_iso8601( out, sizeof(out), &dt );
    TEST_ASSERT_EQUAL_STRING( "2010-05-16T13:28:17", out );

    // single-digit fields are zero padded
    datetime_v_get_epoch( &dt );
    dt.year = 1999;
    dt.month = 1;
    dt.day = 2;
    dt.hours = 3;
    dt.minutes = 4;
    dt.seconds = 5;

    datetime_v_to_iso8601( out, sizeof(out), &dt );
    TEST_ASSERT_EQUAL_STRING( "1999-01-02T03:04:05", out );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_datetime_leap_year );
    RUN_TEST( test_datetime_seconds_roundtrip );
    RUN_TEST( test_datetime_iso8601_roundtrip );

    return UNITY_END();
}
