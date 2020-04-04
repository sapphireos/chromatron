#include "osapi.h"
#include "user_interface.h"

#include "uart.h"
#include "init.h"
#include "hal_usart.h"
#include "sapphire.h"
#include "threading.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"

#ifndef ESP8266_UPGRADE
// manual place in irom0 section.
// if just in .irom0.text, will create a section type conflict. no idea why.
static __attribute__((section(".irom0.text.coproc"), aligned(4))) uint8_t coproc_fw[] = {
    #include "coproc_firmware.txt"
};
#endif
#endif

#ifndef BOOTLOADER

static os_timer_t ptimer;

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABBBCDDD
 *                A : rf cal
 *                B : at parameters
 *                C : rf init data
 *                D : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }
    return rf_cal_sec;
}

// user_pre_init is required from SDK v3.0.0 onwards
// It is used to register the parition map with the SDK, primarily to allow
// the app to use the SDK's OTA capability.  We don't make use of that in 
// otb-iot and therefore the only info we provide is the mandatory stuff:
// - RF calibration data
// - Physical data
// - System parameter
// The location and length of these are from the 2A SDK getting started guide
void ICACHE_FLASH_ATTR user_pre_init(void)
{
  bool rc = false;
  static const partition_item_t part_table[] = 
  {
    {SYSTEM_PARTITION_RF_CAL,
     0x3fb000,
     0x1000},
    {SYSTEM_PARTITION_PHY_DATA,
     0x3fc000,
     0x1000},
    {SYSTEM_PARTITION_SYSTEM_PARAMETER,
     0x3fd000,
     0x3000},
  };

  // This isn't an ideal approach but there's not much point moving on unless
  // or until this has succeeded cos otherwise the SDK will just barf and 
  // refuse to call user_init()
  while (!rc)
  {
    rc = system_partition_table_regist(part_table,
                       sizeof(part_table)/sizeof(part_table[0]),
                                       FLASH_SIZE_32M_MAP_512_512);
  }
}


#define LOOP_PRIO 1
#define LOOP_QUEUE_LEN 4

static os_event_t loop_q[LOOP_QUEUE_LEN];

//Main code function
static void ICACHE_FLASH_ATTR loop(os_event_t *events) {
 
    thread_core();

    system_os_post(LOOP_PRIO, 0, 0 );
}
 

void app_v_init( void ) __attribute__((weak));
void libs_v_init( void ) __attribute__((weak));

void ICACHE_FLASH_ATTR user_init(void)
{
    gpio_init();

    // disable SDK debug prints
    // NOTE this will disable ALL console prints, including ours!
    #ifdef ENABLE_COPROCESSOR
    system_set_os_print( 0 );
    #endif

    uart_init(115200, 115200);

    os_printf("\r\nESP8266 SDK version:%s\r\n", system_get_sdk_version());

// return;

    register uint32_t *sp asm("a1");
    // os_printf("0x%08x\r\n", sp);

    uint8_t *stack_start = (uint8_t *)( (uint32_t)sp - 64 );
    uint8_t *stack_end = (uint8_t *)( 0x40000000 - MEM_MAX_STACK );

    // os_printf("0x%08x -> 0x%08x\r\n", stack_start, stack_end);

    uint8_t *ptr = stack_start;
    while( ptr >= stack_end ){

        *ptr = 0x47; // canary
        ptr--;
    }

    // Disable WiFi
  	wifi_set_opmode(NULL_MODE);

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
// return;

    // os_timer_disarm(&ptimer);
    // os_timer_setfn(&ptimer, (os_timer_func_t *)blinky, NULL);
    // os_timer_arm(&ptimer, 500, 1);

    // sapphireos init
    if( sapphire_i8_init() == 0 ){
            
        if( app_v_init != 0 ){            

            app_v_init();
        }

        if( libs_v_init != 0 ){

            libs_v_init();
        }
    }
// return;

    // start OS
    // on the ESP8266 this will return
    sapphire_run();

    // turn off LED
    io_v_set_esp_led( 0 );

    #ifdef ENABLE_COPROCESSOR
    usart_v_init( 0 );
    usart_v_set_baud( 0, 4000000 );

    coproc_v_sync();

    #ifndef ESP8266_UPGRADE
    coproc_v_fw_load( coproc_fw, sizeof(coproc_fw) );
    #endif    
    #endif    

    //Start os task
    system_os_task(loop, LOOP_PRIO, loop_q, LOOP_QUEUE_LEN);
    system_os_post(LOOP_PRIO, 0, 0 );    
}

#else
// BOOTLOADER

void ICACHE_FLASH_ATTR user_pre_init(void){

}

void ICACHE_FLASH_ATTR user_init(void)
{
  
}

#endif
