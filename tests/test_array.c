#include "unity.h"
#include "array.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

void test_array_alloc_add_grow( void ){

    array_t arr = array_alloc( sizeof(uint16_t), 2 );

    TEST_ASSERT_NOT_NULL( array_get_data( arr ) );
    TEST_ASSERT_EQUAL_INT( 0, array_get_count( arr ) );

    uint16_t v = 100;

    array_add( &arr, &v );
    TEST_ASSERT_EQUAL_INT( 1, array_get_count( arr ) );
    TEST_ASSERT_EQUAL_UINT16( 100, *( uint16_t * )array_get_data( arr ) );

    // grow doubles the capacity path
    v = 200;
    array_add( &arr, &v );
    TEST_ASSERT_EQUAL_INT( 2, array_get_count( arr ) );

    array_t grown = array_grow( arr, 8 );

    TEST_ASSERT_EQUAL_INT( 2, array_get_count( grown ) );

    uint16_t *data = array_get_data( grown );

    TEST_ASSERT_EQUAL_UINT16( 100, data[0] );
    TEST_ASSERT_EQUAL_UINT16( 200, data[1] );

    array_free( grown );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_array_alloc_add_grow );

    return UNITY_END();
}
