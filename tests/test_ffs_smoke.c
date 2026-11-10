// ffs smoke suite: de-risk linking the flash file system stack on the
// host. Real code under test: ffs_page.c, ffs_block.c, flash25.c (SUT),
// backed by the file-based sim flash HAL (sim_flash.bin in the working
// directory). Deeper ffs behavior tests land in M3.

#include <stdint.h>

#include "unity.h"
#include "ffs_page.h"
#include "ffs_block.h"
#include "flash25.h"
#include "crc.h"
#include "memory.h"

// this Unity build calls setUp/tearDown unconditionally
void setUp( void ){}
void tearDown( void ){}

static int initialized;

static void init_stack( void ){

    if( initialized ){
        return;
    }

    // the init sequence init.c uses for the flash file system (mem2 first:
    // ffs_block allocates its block table from the mem2 pool)
    mem2_v_init();
    crc_v_init();
    flash25_v_init();
    ffs_block_v_init();
    ffs_page_v_init();

    initialized = 1;
}

// a fresh (erased) flash must expose a positive page count derived from
// the emulated chip capacity
void test_total_pages_positive( void ){

    init_stack();

    TEST_ASSERT_TRUE( ffs_page_u16_total_pages() > 0 );
}

// brand new flash has no files
void test_fresh_flash_empty( void ){

    init_stack();

    TEST_ASSERT_EQUAL_UINT8( 0, ffs_page_u8_count_files() );
}

// stats counters reset to zero
void test_stats_reset( void ){

    init_stack();

    ffs_page_v_stats_reset();

    TEST_ASSERT_EQUAL_UINT32( 0, ffs_page_u32_get_block_copies() );
    TEST_ASSERT_EQUAL_UINT32( 0, ffs_page_u32_get_page_allocs() );
    TEST_ASSERT_EQUAL_UINT32( 0, ffs_page_u32_get_cache_hits() );
    TEST_ASSERT_EQUAL_UINT32( 0, ffs_page_u32_get_cache_misses() );
}

int main( void ){

    UNITY_BEGIN();

    RUN_TEST( test_total_pages_positive );
    RUN_TEST( test_fresh_flash_empty );
    RUN_TEST( test_stats_reset );

    return UNITY_END();
}
