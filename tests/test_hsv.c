#include "unity.h"
#include "hsv_to_rgb.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

// exact values follow from the piecewise-linear MAP() curve in
// hsv_to_rgb.c (HUE_CURVE_ADVANCED): 65536 hue units = 360 degrees

void test_hsv_primary_hues( void ){

    uint16_t r, g, b;

    // red
    gfx_v_hsv_to_rgb( 0, 65535, 65535, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 65534, r );
    TEST_ASSERT_EQUAL_UINT16( 0, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );

    // orange (60 degrees): r holds at the plateau, g ramps up
    // (nonzero channels lose one count in the final brightness multiply)
    gfx_v_hsv_to_rgb( 8191, 65535, 65535, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 43689, r );
    TEST_ASSERT_EQUAL_UINT16( 21844, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );

    // yellow (120 degrees)
    gfx_v_hsv_to_rgb( 16383, 65535, 65535, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 43689, r );
    TEST_ASSERT_EQUAL_UINT16( 43689, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );

    // green (180 degrees): full channels come out 65534 - the final
    // brightness multiply (x * 65535 / 65536) loses one count
    gfx_v_hsv_to_rgb( 24575, 65535, 65535, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 0, r );
    TEST_ASSERT_EQUAL_UINT16( 65534, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );

    // cyan (240 degrees)
    gfx_v_hsv_to_rgb( 32767, 65535, 65535, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 0, r );
    TEST_ASSERT_EQUAL_UINT16( 43689, g );
    TEST_ASSERT_EQUAL_UINT16( 21844, b );

    // hue wraps: 65535 is red again
    gfx_v_hsv_to_rgb( 65535, 65535, 65535, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 65534, r );
    TEST_ASSERT_EQUAL_UINT16( 0, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );
}

void test_hsv_saturation_floor( void ){

    uint16_t r, g, b;

    // half-saturated cyan: the desaturated channels are floored at
    // (65535 - s) before brightness is applied
    gfx_v_hsv_to_rgb( 32767, 32767, 65535, &r, &g, &b );

    TEST_ASSERT_UINT16_WITHIN( 1, 32768, r );
    TEST_ASSERT_UINT16_WITHIN( 2, 43690, g );
    TEST_ASSERT_UINT16_WITHIN( 1, 32768, b );

    // zero brightness: everything black
    gfx_v_hsv_to_rgb( 0, 65535, 0, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 0, r );
    TEST_ASSERT_EQUAL_UINT16( 0, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );

    // half brightness: red scales by 1/2 (65535 * 32767 / 65536 = 32766)
    gfx_v_hsv_to_rgb( 0, 65535, 32767, &r, &g, &b );
    TEST_ASSERT_EQUAL_UINT16( 32766, r );
    TEST_ASSERT_EQUAL_UINT16( 0, g );
    TEST_ASSERT_EQUAL_UINT16( 0, b );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_hsv_primary_hues );
    RUN_TEST( test_hsv_saturation_floor );

    return UNITY_END();
}
