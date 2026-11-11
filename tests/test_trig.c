#include "unity.h"
#include "trig.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

// 16.25 degree scale: 65536 = 360 degrees, table is 256 entries per
// quarter (one entry per 0.225 degrees)

void test_trig_sine_table( void ){

    TEST_ASSERT_EQUAL_INT16( 0, sine( 0 ) );

    // 90 degrees is the table maximum
    TEST_ASSERT_EQUAL_INT16( 32766, sine( 16384 ) );

    // 180 and 360 wrap back through zero
    TEST_ASSERT_EQUAL_INT16( 0, sine( 32768 ) );
    TEST_ASSERT_EQUAL_INT16( 0, sine( 65535 ) );

    // 270 degrees is the negative peak
    TEST_ASSERT_EQUAL_INT16( -32766, sine( 49152 ) );

    // 45 degrees ~ 0.7071 * 32767 (table quantization tolerance)
    int16_t s45 = sine( 8192 );

    TEST_ASSERT_TRUE( s45 > 23100 && s45 < 23250 );

    // odd symmetry: sine(a + 32768) == -sine(a)
    TEST_ASSERT_EQUAL_INT16( -sine( 4096 ), sine( 4096 + 32768 ) );
}

void test_trig_cosine( void ){

    // cosine is sine a quarter turn on
    TEST_ASSERT_EQUAL_INT16( 32766, cosine( 0 ) );
    TEST_ASSERT_EQUAL_INT16( 0, cosine( 16384 ) );
    TEST_ASSERT_EQUAL_INT16( -32766, cosine( 32768 ) );
    TEST_ASSERT_EQUAL_INT16( sine( 16384 ), cosine( 0 ) );
}

void test_trig_triangle( void ){

    TEST_ASSERT_EQUAL_INT16( 0, triangle( 0 ) );
    TEST_ASSERT_EQUAL_INT16( 32768, triangle( 16384 ) );
    TEST_ASSERT_EQUAL_INT16( 0, triangle( 32768 ) );
    TEST_ASSERT_EQUAL_INT16( -32768, triangle( 49152 ) );

    // linear ramps: midpoint of the rising edge
    TEST_ASSERT_EQUAL_INT16( 8192, triangle( 8192 / 2 ) );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_trig_sine_table );
    RUN_TEST( test_trig_cosine );
    RUN_TEST( test_trig_triangle );

    return UNITY_END();
}
