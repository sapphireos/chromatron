
#include "sapphire.h"

#include "vm4.h"
#include "sequencer.h"
#include "sync4.h"
#include "gfx_lib.h"


void serialize_pixels( void ){

	uint16_t pix_count = gfx_u16_get_pix_count();
	uint16_t array_size = sizeof(uint16_t) * pix_count;

	uint16_t *h_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE );
	uint16_t *s_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT );	
	uint16_t *v_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT );	
	uint16_t *hsfade_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HS_FADE );	
	uint16_t *vfade_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_V_FADE );	
	uint16_t *h_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE_STEP );
	uint16_t *s_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT_STEP );	
	uint16_t *v_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_VAL_STEP );

	fs_v_delete_fname_P( PSTR("_pixel.f4b") );

	file_t f = fs_f_open_P( PSTR("_pixel.f4b"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if(f <= 0){

        return;
    }

    uint32_t magic = SYNC4_PROTOCOL_MAGIC;
    fs_i16_write( f, (uint8_t *)&magic, sizeof(magic) );
    fs_i16_write( f, (uint8_t *)&pix_count, sizeof(pix_count) );
    fs_i16_write( f, (uint8_t *)&array_size, sizeof(array_size) );

	fs_i16_write( f, (uint8_t *)h_ptr, array_size );
	fs_i16_write( f, (uint8_t *)s_ptr, array_size );
	fs_i16_write( f, (uint8_t *)v_ptr, array_size );
	fs_i16_write( f, (uint8_t *)hsfade_ptr, array_size );
	fs_i16_write( f, (uint8_t *)vfade_ptr, array_size );
	fs_i16_write( f, (uint8_t *)h_step_ptr, array_size );
	fs_i16_write( f, (uint8_t *)s_step_ptr, array_size );
	fs_i16_write( f, (uint8_t *)v_step_ptr, array_size );
	
	
}

void sync4_v_init( void ){

	log_v_debug_P( PSTR("Sync4 init") );

	serialize_pixels();
}



