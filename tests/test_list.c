#include "unity.h"
#include "list.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

void test_list_insert_remove( void ){

    list_t list;

    list_v_init( &list );
    TEST_ASSERT_TRUE( list_b_is_empty( &list ) );

    char a = 'a', b = 'b', c = 'c';

    list_node_t na = list_ln_create_node( &a, 1 );
    list_node_t nb = list_ln_create_node( &b, 1 );
    list_node_t nc = list_ln_create_node( &c, 1 );

    list_v_insert_tail( &list, na );
    list_v_insert_tail( &list, nb );
    list_v_insert_head( &list, nc );

    TEST_ASSERT_EQUAL_UINT8( 3, list_u8_count( &list ) );

    // no sentinel: head points at the first real node
    TEST_ASSERT_EQUAL( nc, list.head );

    list_node_t n = list_ln_next( list.head );

    TEST_ASSERT_EQUAL( na, n );

    n = list_ln_next( n );
    TEST_ASSERT_EQUAL( nb, n );

    // the tail's next pointer is the -1 null node
    TEST_ASSERT_EQUAL( -1, list_ln_next( n ) );

    // data is accessible through the node
    TEST_ASSERT_EQUAL_CHAR( 'a', *( char * )list_vp_get_data( na ) );

    // remove the head, tail remains b
    list_v_remove( &list, list.head );
    list_v_release_node( list.head );
    TEST_ASSERT_EQUAL_UINT8( 2, list_u8_count( &list ) );

    list_node_t tail = list_ln_remove_tail( &list );

    TEST_ASSERT_EQUAL( nb, tail );
    list_v_release_node( tail );

    TEST_ASSERT_EQUAL_UINT8( 1, list_u8_count( &list ) );

    // index addressing
    TEST_ASSERT_EQUAL( na, list_ln_index( &list, 0 ) );

    list_v_release_node( na );
    list_v_destroy( &list );
}

void test_list_flatten( void ){

    // flatten the whole list from position 0 into a buffer
    list_t list;

    list_v_init( &list );

    uint8_t vals[3] = { 7, 8, 9 };

    for( int i = 0; i < 3; i++ ){

        list_node_t n = list_ln_create_node( &vals[i], 1 );
        list_v_insert_tail( &list, n );
    }

    uint8_t out[3] = { 0, 0, 0 };

    uint16_t n = list_u16_flatten( &list, 0, out, 3 );

    TEST_ASSERT_EQUAL_UINT16( 3, n );
    TEST_ASSERT_EQUAL_MEMORY( vals, out, 3 );

    for( int i = 0; i < 3; i++ ){

        list_v_remove( &list, list.head );
        list_v_release_node( list.head );
    }
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_list_insert_remove );
    RUN_TEST( test_list_flatten );

    return UNITY_END();
}
