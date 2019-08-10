
#include "c_types.h"

void sdk_init_done_cb(void){
/* You app Initialization here */
}
 
void user_init() {
	system_init_done_cb(sdk_init_done_cb);
}

void ICACHE_RAM_ATTR app_entry (void)
{
  
}

uint32_t user_rf_cal_sector_set(void)
{
    // spoof_init_data = true;
    // return flashchip->chip_size/SPI_FLASH_SEC_SIZE - 4;
    return 0;
}