#include <string.h>

#include "unity.h"
#include "bool.h"
#include "pixelarray.h"
#include "bytecode.h"
#include "gfx_lib.h"
#include "hash.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

#define H( s )  hash_u32_string( ( char * )( s ) )

// declared in gfx_lib.c but missing from gfx_lib.h
extern void gfx_v_set_pix_count( uint16_t setting );

static void pixelarray_sys_start( uint16_t pix_count ){

    gfx_v_set_pix_count( pix_count );
    pixelarray_init();
}

void test_pixelarray_create_lookup( void ){

    pixelarray_sys_start( 8 );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_count() );

    pixelarray_create( H( "pa_pixels" ), 0 );
    TEST_ASSERT_EQUAL_INT( 1, pixelarray_count() );

    // lookup fills the struct
    pixel_array_t out;

    memset( &out, 0, sizeof(out) );
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_lookup( H( "pa_pixels" ), &out ) );
    TEST_ASSERT_EQUAL_UINT32( H( "pa_pixels" ), out.name_hash );
    TEST_ASSERT_EQUAL_INT16( 8, out.count );

    // duplicate create is a no-op
    pixelarray_create( H( "pa_pixels" ), 0 );
    TEST_ASSERT_EQUAL_INT( 1, pixelarray_count() );

    TEST_ASSERT_EQUAL_INT( -1, pixelarray_lookup( H( "no_such_array" ), &out ) );

    pixelarray_deinit();
}

void test_pixelarray_store_load_1d( void ){

    pixelarray_sys_start( 8 );
    pixelarray_create( H( "pa_pix" ), 0 );

    // store / load round trip
    pixelarray_pstore_val( H( "pa_pix" ), 3, 12345 );

    int32_t val = 0;

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_pix" ), PIXEL_ATTR_VAL, 3, -1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 12345, val );

    // out of range values are clamped
    pixelarray_pstore_val( H( "pa_pix" ), 0, 70000 );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_pix" ), PIXEL_ATTR_VAL, 0, -1, &val ) );

    // 65535 is coerced to 1.0 in fixed16 on the way out
    TEST_ASSERT_EQUAL_INT32( 65536, val );

    pixelarray_pstore_val( H( "pa_pix" ), 1, -5 );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_pix" ), PIXEL_ATTR_VAL, 1, -1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 0, val );

    // unknown array fails
    TEST_ASSERT_EQUAL_INT( -1, pixelarray_load( H( "missing" ), PIXEL_ATTR_VAL, 0, -1, &val ) );

    pixelarray_deinit();
}

void test_pixelarray_2d( void ){

    pixelarray_sys_start( 8 );

    // pixelarray_init() allocates a 1-slot (zeroed) backing array but
    // does not refresh the cache, so the first create() appends and the
    // cache becomes [free, A].  The second create() fills the free slot:
    // cache is now [B, A] with no free slot, so the pixelarray_add()
    // below is forced down the append path.
    pixelarray_create( H( "pa_2d_a" ), 0 );
    pixelarray_create( H( "pa_2d_b" ), 0 );

    // the gfx master array is not cleared on (re)start; the 1D test left
    // 65535 in pixel 0 (a clamped 70000).  Zero it via pa_2d_a for the
    // (0,0) assertion below.
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_pstore_val( H( "pa_2d_a" ), 0, 0 ) );

    // 2x2 sub-array at an offset in the 8 pixel master array:
    // (x, y) -> flat index x + y * size_x
    pixel_array_t pa = {
        .name_hash = H( "pa_2d" ),
        .count = 4,
        .index = 0,
        .size_x = 2,
        .size_y = 2,
        .reverse = false,
        .vm_id = 0,
    };

    pixelarray_add( &pa, 0 );

    // KNOWN SUT BUG (FAILURES.md #1): the append path of pixelarray_add
    // never calls pixelarray_deref(), so the cached view
    // (pixarray_ptr/pixarray_count) misses the new element while the
    // live view (pixelarray_lookup, read via array_get_data/count)
    // still sees it.  Every store/load op goes through the cached view,
    // so it fails until a pixelarray_deref() forces a refresh.
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_lookup( H( "pa_2d" ), NULL ) );
    TEST_ASSERT_EQUAL_INT( -1, pixelarray_lookup_ptr( H( "pa_2d" ), NULL ) );
    TEST_ASSERT_EQUAL_INT( -1, pixelarray_pstore_val2( H( "pa_2d" ), 1, 1, 777 ) );

    pixelarray_deref();
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_lookup_ptr( H( "pa_2d" ), NULL ) );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_pstore_val2( H( "pa_2d" ), 1, 1, 777 ) );

    int32_t val = 0;

    // (1,1) is flat index 3 within the sub-array
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_2d" ), PIXEL_ATTR_VAL, 1, 1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 777, val );

    // (0,0) is untouched
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_2d" ), PIXEL_ATTR_VAL, 0, 0, &val ) );
    TEST_ASSERT_EQUAL_INT32( 0, val );

    pixelarray_deinit();
}

void test_pixelarray_vector_op( void ){

    pixelarray_sys_start( 4 );
    pixelarray_create( H( "pa_vec" ), 0 );

    pixelarray_pstore_val( H( "pa_vec" ), 0, 10 );
    pixelarray_pstore_val( H( "pa_vec" ), 1, 20 );

    // add to every pixel in the array
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_vector_op_val( H( "pa_vec" ), ASSIGN_OP_ADD, 5 ) );

    int32_t val = 0;

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_vec" ), PIXEL_ATTR_VAL, 0, -1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 15, val );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_vec" ), PIXEL_ATTR_VAL, 1, -1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 25, val );

    // subtract
    TEST_ASSERT_EQUAL_INT( 0, pixelarray_vector_op_val( H( "pa_vec" ), ASSIGN_OP_SUB, 15 ) );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_vec" ), PIXEL_ATTR_VAL, 0, -1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 0, val );

    TEST_ASSERT_EQUAL_INT( 0, pixelarray_load( H( "pa_vec" ), PIXEL_ATTR_VAL, 1, -1, &val ) );
    TEST_ASSERT_EQUAL_INT32( 10, val );

    pixelarray_deinit();
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_pixelarray_create_lookup );
    RUN_TEST( test_pixelarray_store_load_1d );
    RUN_TEST( test_pixelarray_2d );
    RUN_TEST( test_pixelarray_vector_op );

    return UNITY_END();
}
