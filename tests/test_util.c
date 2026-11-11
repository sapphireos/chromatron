#include "unity.h"
#include "util.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

void test_util_abs( void ){

    TEST_ASSERT_EQUAL_UINT8( 3, abs8( -3 ) );
    TEST_ASSERT_EQUAL_UINT8( 3, abs8( 3 ) );
    TEST_ASSERT_EQUAL_UINT16( 400, abs16( -400 ) );
    TEST_ASSERT_EQUAL_UINT32( 100000, abs32( -100000 ) );
    TEST_ASSERT_EQUAL_UINT64( 10000000000ULL, abs64( -10000000000LL ) );
}

void test_util_linear_interp( void ){

    // y = y0 + (x - x0) * (y1 - y0) / (x1 - x0)
    TEST_ASSERT_EQUAL_UINT16( 50, util_u16_linear_interp( 50, 0, 0, 100, 100 ) );
    TEST_ASSERT_EQUAL_UINT16( 0, util_u16_linear_interp( 0, 0, 0, 100, 100 ) );
    TEST_ASSERT_EQUAL_UINT16( 100, util_u16_linear_interp( 100, 0, 0, 100, 100 ) );

    // ascending ramp between arbitrary endpoints
    TEST_ASSERT_EQUAL_UINT16( 50, util_u16_linear_interp( 50, 0, 10, 100, 90 ) );

    // zero-width range returns y0
    TEST_ASSERT_EQUAL_UINT16( 7, util_u16_linear_interp( 99, 5, 7, 5, 3 ) );

    // KNOWN SUT BUG (FAILURES.md): y1 - y0 is computed in uint16_t, so a
    // descending ramp (y0 > y1) wraps and produces garbage. 50 should
    // interpolate to 50; the wrapped diff (65436) yields
    // 100 + 50 * 65436 / 100 = 32818.
    TEST_ASSERT_EQUAL_UINT16( 32818, util_u16_linear_interp( 50, 0, 100, 100, 0 ) );
}

void test_util_sort( void ){

    uint16_t u16[5] = { 40, 10, 50, 20, 30 };

    util_v_bubble_sort_u16( u16, 5 );

    uint16_t sorted16[5] = { 10, 20, 30, 40, 50 };

    TEST_ASSERT_EQUAL_MEMORY( sorted16, u16, sizeof(u16) );

    uint32_t u32[4] = { 4, 1, 3, 2 };

    util_v_bubble_sort_u32( u32, 4 );

    uint32_t sorted32[4] = { 1, 2, 3, 4 };

    TEST_ASSERT_EQUAL_MEMORY( sorted32, u32, sizeof(u32) );

    uint32_t rev[4] = { 4, 1, 3, 2 };

    util_v_bubble_sort_reversed_u32( rev, 4 );

    uint32_t sorted_rev[4] = { 4, 3, 2, 1 };

    TEST_ASSERT_EQUAL_MEMORY( sorted_rev, rev, sizeof(rev) );
}

void test_util_ewma( void ){

    // ratio 128 is a 1/2 / 1/2 blend; 255 is 255/256 new (rounds to 99)
    TEST_ASSERT_EQUAL_INT16( 50, util_i16_ewma( 100, 0, 128 ) );
    TEST_ASSERT_EQUAL_UINT16( 50, util_u16_ewma( 100, 0, 128 ) );
    TEST_ASSERT_EQUAL_UINT32( 50, util_u32_ewma( 100, 0, 128 ) );

    // near-extremes
    TEST_ASSERT_EQUAL_INT16( 99, util_i16_ewma( 100, 0, 255 ) );

    // the anti-stall nudge: with ratio 0 the blend is exactly `old`
    // (100); the filter would never move, so it steps one count toward
    // the new value
    TEST_ASSERT_EQUAL_INT16( 99, util_i16_ewma( 0, 100, 0 ) );
}

void test_util_average( void ){

    uint8_t u8[4] = { 10, 20, 30, 40 };

    TEST_ASSERT_EQUAL_UINT8( 25, util_u8_average( u8, 4 ) );

    int16_t i16[3] = { -10, 10, 20 };

    TEST_ASSERT_EQUAL_INT16( 6, util_i16_average( i16, 3 ) );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_util_abs );
    RUN_TEST( test_util_linear_interp );
    RUN_TEST( test_util_sort );
    RUN_TEST( test_util_ewma );
    RUN_TEST( test_util_average );

    return UNITY_END();
}
