// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>


#include "system.h"

#include "hal_cpu.h"
#include "watchdog.h"

void cpu_v_init( void ){

    cli();

    // startup default:
    // 2 MHz oscillator enabled
    // System clock on 2 MHz osc.

    // enable 32 KHz xtal
    OSC.XOSCCTRL = OSC_XOSCSEL_32KHz_gc;

    // enable xtal oscillator
    OSC.CTRL |= OSC_XOSCEN_bm;

    // wait for osc to stabilize
    while( ( OSC.STATUS & OSC_XOSCRDY_bm ) == 0 );


    // set DFLL2M compare to 2 MHz
    DFLLRC2M.COMP1 = 0xA1;
    DFLLRC2M.COMP2 = 0x07;

    // select 32 KHz xtal for DFLL2M reference
    OSC.DFLLCTRL |= OSC_RC2MCREF_bm;

    // enable DFLL for 2 MHz osc
    DFLLRC2M.CTRL = DFLL_ENABLE_bm;

    // set PLL source to 2 MHz osc, div 1, multiple by 16x
    OSC.PLLCTRL = OSC_PLLSRC_RC2M_gc | 16;

    // enable PLL
    OSC.CTRL |= OSC_PLLEN_bm;

    // wait for PLL lock
    while( ( OSC.STATUS & OSC_PLLRDY_bm ) == 0 );

    // enable configuration change
    ENABLE_CONFIG_CHANGE;

    // set CPU clock to PLL
    CLK.CTRL = CLK_SCLKSEL_PLL_gc;

    // CPU clock is now at 32 MHz

    // init 32 MHz oscillator
    OSC.CTRL |= OSC_RC32MEN_bm;

    // wait for oscillator to stabilize
    while( ( OSC.STATUS & OSC_RC32MRDY_bm ) == 0 );

    // enable failure detection
    OSC.XOSCFAIL = OSC_XOSCFDEN_bm;

    // set the DFLL calibration reference to the USB SOF
    OSC.DFLLCTRL |= OSC_RC32MCREF_USBSOF_gc;

    // set DFLL compare to 48 MHz
    DFLLRC32M.COMP1 = 0x1B;
    DFLLRC32M.COMP2 = 0xB7;

    // set DFLL calibartion
    NVM.CMD        = NVM_CMD_READ_CALIB_ROW_gc;
    DFLLRC32M.CALA = pgm_read_byte(offsetof(NVM_PROD_SIGNATURES_t, USBRCOSCA));
    DFLLRC32M.CALB = pgm_read_byte(offsetof(NVM_PROD_SIGNATURES_t, USBRCOSC));
    NVM.CMD        = 0;

    // enable DFLL
    DFLLRC32M.CTRL = DFLL_ENABLE_bm;

    // the 32 MHz oscillator is now running at 48 MHz.

    // select USB clock source and enable it
    CLK.USBCTRL = CLK_USBSRC_RC32M_gc | CLK_USBSEN_bm;

    // enable all interrupt priority levels
    PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

uint8_t cpu_u8_get_reset_source( void ){

    uint8_t temp = RST.STATUS;

    if( temp & RST_PORF_bm ){

        return RESET_SOURCE_POWER_ON;
    }
    else if( temp & RST_PDIRF_bm ){

        return RESET_SOURCE_JTAG;
    }
    else if( temp & RST_EXTRF_bm ){

        return RESET_SOURCE_EXTERNAL;
    }
    else if( temp & RST_BORF_bm ){

        return RESET_SOURCE_BROWNOUT;
    }
    else if( temp & RST_WDRF_bm ){

        return RESET_SOURCE_WATCHDOG;
    }

    return 0;
}

void cpu_v_clear_reset_source( void ){

    // clear status
    RST.STATUS = 0xff;
}

void cpu_v_remap_isrs( void ){

}

void cpu_v_sleep( void ){

    // SLEEP.CTRL = SLEEP_MODE_IDLE;
    // DISABLE_INTERRUPTS;
    // sleep_enable();
    // ENABLE_INTERRUPTS;
    // sleep_cpu();
    // sleep_disable();
}

bool cpu_b_osc_fail( void ){

    return ( OSC.XOSCFAIL & OSC_XOSCFDIF_bm ) != 0;
}

void cpu_v_set_clock_speed_low( void ){

}

void cpu_v_set_clock_speed_high( void ){

}

uint32_t cpu_u32_get_clock_speed( void ){

    return 32000000;
}

void cpu_reboot( void ){
    asm volatile("cli\n\t"          // disable interrupts
              "ldi r24, 0xD8\n\t" // value to write to CCP
              "ldi r25, 0x01\n\t" // value to write to SWRST
              "ldi r30, 0x78\n\t"  // base address of RST peripheral
              "ldi r31, 0\n\t"
              "out __CCP__, r24\n\t"
              "std Z+1, r25\n\t"  // +1 is the offset of RST.CTRL
              ::); // no clobber list because we don't return

    // ATOMIC;
    // ENABLE_CONFIG_CHANGE;

    // RST.CTRL |= RST_SWRST_bm;
    // END_ATOMIC;

    // enable watchdog timer:
    // wdg_v_enable( WATCHDOG_TIMEOUT_16MS, WATCHDOG_FLAGS_RESET );    
}

ISR(OSC_OSCF_vect){

}

#ifndef BOOTLOADER
// bad interrupt
ISR(__vector_default){

    ASSERT( 0 );
}
#endif
