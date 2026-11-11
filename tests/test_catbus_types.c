#include "unity.h"
#include "catbus_types.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

void test_type_u16_size( void ){

    // scalars
    TEST_ASSERT_EQUAL_UINT16( 0, type_u16_size( CATBUS_TYPE_NONE ) );
    TEST_ASSERT_EQUAL_UINT16( 1, type_u16_size( CATBUS_TYPE_BOOL ) );
    TEST_ASSERT_EQUAL_UINT16( 1, type_u16_size( CATBUS_TYPE_UINT8 ) );
    TEST_ASSERT_EQUAL_UINT16( 1, type_u16_size( CATBUS_TYPE_INT8 ) );
    TEST_ASSERT_EQUAL_UINT16( 2, type_u16_size( CATBUS_TYPE_UINT16 ) );
    TEST_ASSERT_EQUAL_UINT16( 2, type_u16_size( CATBUS_TYPE_INT16 ) );
    TEST_ASSERT_EQUAL_UINT16( 4, type_u16_size( CATBUS_TYPE_UINT32 ) );
    TEST_ASSERT_EQUAL_UINT16( 4, type_u16_size( CATBUS_TYPE_INT32 ) );
    TEST_ASSERT_EQUAL_UINT16( 4, type_u16_size( CATBUS_TYPE_FLOAT ) );
    TEST_ASSERT_EQUAL_UINT16( 4, type_u16_size( CATBUS_TYPE_FIXED16 ) );
    TEST_ASSERT_EQUAL_UINT16( 4, type_u16_size( CATBUS_TYPE_GFX16 ) );
    TEST_ASSERT_EQUAL_UINT16( 4, type_u16_size( CATBUS_TYPE_IPv4 ) );
    TEST_ASSERT_EQUAL_UINT16( 8, type_u16_size( CATBUS_TYPE_UINT64 ) );
    TEST_ASSERT_EQUAL_UINT16( 8, type_u16_size( CATBUS_TYPE_INT64 ) );

    // fixed-size strings / binary
    TEST_ASSERT_EQUAL_UINT16( 32, type_u16_size( CATBUS_TYPE_STRING32 ) );
    TEST_ASSERT_EQUAL_UINT16( 64, type_u16_size( CATBUS_TYPE_STRING64 ) );
    TEST_ASSERT_EQUAL_UINT16( 128, type_u16_size( CATBUS_TYPE_STRING128 ) );

    // STRING512 is artificially clamped to fit the 512 byte catbus
    // message limit (see the NOTE in catbus_types.c)
    TEST_ASSERT_EQUAL_UINT16( 488, type_u16_size( CATBUS_TYPE_STRING512 ) );

    TEST_ASSERT_EQUAL_UINT16( 6, type_u16_size( CATBUS_TYPE_MAC48 ) );
    TEST_ASSERT_EQUAL_UINT16( 8, type_u16_size( CATBUS_TYPE_MAC64 ) );
    TEST_ASSERT_EQUAL_UINT16( 16, type_u16_size( CATBUS_TYPE_KEY128 ) );

    // indirect / unknown types report SIZE_INVALID
    TEST_ASSERT_EQUAL_UINT16( CATBUS_TYPE_SIZE_INVALID,
                              type_u16_size( CATBUS_TYPE_REF ) );
    TEST_ASSERT_EQUAL_UINT16( CATBUS_TYPE_SIZE_INVALID,
                              type_u16_size( CATBUS_TYPE_INVALID ) );
}

void test_type_b_is_string( void ){

    // the string-like family
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_STRING32 ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_STRING64 ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_STRING128 ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_STRING512 ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_STRREF ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_MAC48 ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_MAC64 ) );
    TEST_ASSERT_TRUE( type_b_is_string( CATBUS_TYPE_KEY128 ) );

    // plain numeric / other types are not strings
    TEST_ASSERT_FALSE( type_b_is_string( CATBUS_TYPE_NONE ) );
    TEST_ASSERT_FALSE( type_b_is_string( CATBUS_TYPE_BOOL ) );
    TEST_ASSERT_FALSE( type_b_is_string( CATBUS_TYPE_UINT16 ) );
    TEST_ASSERT_FALSE( type_b_is_string( CATBUS_TYPE_IPv4 ) );
    TEST_ASSERT_FALSE( type_b_is_string( CATBUS_TYPE_REF ) );
    TEST_ASSERT_FALSE( type_b_is_string( CATBUS_TYPE_INVALID ) );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_type_u16_size );
    RUN_TEST( test_type_b_is_string );

    return UNITY_END();
}
