//**** ATMEL AVR - A P P L I C A T I O N   N O T E  ************************
//*
//* Title:		AVR061 - STK500 Communication Protocol
//* Filename:		devices.h
//* Version:		1.0
//* Last updated:	09.09.2002
//*
//* Support E-mail:	avr@atmel.com
//*
//**************************************************************************

#ifndef __DEVICES_INCLUDED
#define __DEVICES_INCLUDED

/* 0x00-0x0F reserved */

/* 0x10-0x1F ATtiny 1K devices */

#define ATTINY10    0x10
#define ATTINY11    0x11
#define ATTINY12    0x12
#define ATTINY15    0x13

/* 0x20-0x2F ATtiny 2K devices */

#define ATTINY22    0x20
#define ATTINY26    0x21
#define ATTINY28    0x22

/* 0x30-0x3F ATclassic 1K devices */

#define AT90S1200   0x33

/* 0x40-0x4F ATclassic 2K devices */

#define AT90S2313   0x40
#define AT90S2323   0x41
#define AT90S2333   0x42
#define AT90S2343   0x43

/* 0x50-0x5F ATclassic 4K devices */

#define AT90S4414   0x50
#define AT90S4433   0x51
#define AT90S4434   0x52

/* 0x60-0x6F ATclassic 8K devices */

#define AT90S8515   0x60
#define AT90S8535   0x61
#define AT90C8534   0x62
#define ATMEGA8515  0x63
#define ATMEGA8535  0x64

/* 0x70-0x7F ATmega 8K devices */

#define ATMEGA8     0x70

/* 0x80-0x8F ATmega 16K devices */

#define ATMEGA161   0x80
#define ATMEGA163   0x81
#define ATMEGA16    0x82
#define ATMEGA162   0x83
#define ATMEGA169   0x84

/* 0x90-0x9F ATmega 32K devices */

#define ATMEGA323   0x90
#define ATMEGA32    0x91

/* 0xA0-0xAF ATmega 64K devices */

/* 0xB0-0xBF ATmega 128K devices */

#define ATMEGA103   0xB1
#define ATMEGA128   0xB2

/* 0xC0-0xCF not used */

/* 0xD0-0xDF Other devices */
#define AT86RF401   0xD0

/* 0xE0-0xEF AT89 devices */
#define AT89START   0xE0
#define AT89S51	    0xE0
#define AT89S52	    0xE1

/* 0xF0-0xFF reserved */

#define DEFAULTDEVICE	AT90S8515


struct Device
{
	UCHAR ucId;			// Device ID
	UCHAR ucRev;			// Device revision

	DWORD dwFlashSize;		// FLASH memory
	WORD  wEepromSize;		// EEPROM memory

	UCHAR ucFuseBytes;		// Number of fuse bytes
	UCHAR ucLockBytes;		// Number of lock bytes

	UCHAR ucSerFProg;		// (internal) Serial fuse programming support
	UCHAR ucSerLProg;		// (internal) Serial lockbit programming support
	UCHAR ucSerFLRead;		// (internal) Serial fuse/lockbit reading support
	UCHAR ucCommonLFR;		// (internal) Indicates if lockbits and fuses are combined on read
	UCHAR ucSerMemProg;		// (internal) Serial memory progr. support

	WORD  wPageSize;		// Flash page size
	UCHAR ucEepromPageSize;		// Eeprom page size (extended parameter)

	UCHAR ucSelfTimed;		// True if all instructions are self timed
	UCHAR ucFullPar;		// True if part has full paralell interface
	UCHAR ucPolling;		// True if polling can be used during SPI access

	UCHAR ucFPol;			// Flash poll value
	UCHAR ucEPol1;			// Eeprom poll value 1
	UCHAR ucEPol2;			// Eeprom poll value 2

	LPCTSTR name;			// Device name

	UCHAR ucSignalPAGEL;		// Position of PAGEL signal (0xD7 by default)
	UCHAR ucSignalBS2;		// Position of BS2 signal (0xA0 by default)
};


const struct Device g_deviceTable[] =
{
//	 Id			 Rv FSize   ESize FB LB SerFP  SerLP  SerFLR CLFR	SerMP  PS   EPS  SelfT  FulPar Poll   FPoll EPol1 EPol2	Name
	{ATTINY10,   1, 1024,   0,    1, 1, FALSE, FALSE, FALSE, FALSE, FALSE, 0,   0,  FALSE, FALSE, FALSE, 0x00, 0x00, 0x00, "ATtiny10",    0xD7, 0xA0},
	{ATTINY11,   1, 1024,   0,    1, 1, FALSE, FALSE, FALSE, FALSE, FALSE, 0,   0,  FALSE, FALSE, FALSE, 0x00, 0x00, 0x00, "ATtiny11",    0xD7, 0xA0},
	{ATTINY12,   1, 1024,   64,   1, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  0,   0,  TRUE,  FALSE, TRUE,  0xFF, 0xFF, 0xFF, "ATtiny12",    0xD7, 0xA0},
	{ATTINY15,   1, 1024,   64,   1, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  0,   0,  TRUE,  FALSE, TRUE,  0xFF, 0xFF, 0xFF, "ATtiny15",    0xD7, 0xA0},

	{ATTINY22,   1, 2048,   128,  1, 1, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  0,   0,  FALSE, FALSE, TRUE,  0xFF, 0x00, 0xFF, "ATtiny22",    0xD7, 0xA0},
	{ATTINY26,   1, 2048,   128,  2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  32,  4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATtiny26",    0xD7, 0xA0},
	{ATTINY28,   1, 2048,   0,    1, 1, FALSE, FALSE, FALSE, FALSE, FALSE, 0,   0,  TRUE,  TRUE,  FALSE, 0x00, 0x00, 0x00, "ATtiny28",    0xD7, 0xA0},

	{AT90S1200,  4, 1024,   64,   1, 1, FALSE, TRUE,  FALSE, TRUE,  TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0xFF, 0x00, 0xFF, "AT90S1200",   0xD7, 0xA0},

	{AT90S2313,  1, 2048,   128,  1, 1, FALSE, TRUE,  FALSE, TRUE,  TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0x7F, 0x80, 0x7F, "AT90S2313",   0xD7, 0xA0},
	{AT90S2323,  1, 2048,   128,  1, 1, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  0,   0,  FALSE, FALSE, TRUE,  0xFF, 0x00, 0xFF, "AT90S2323",   0xD7, 0xA0},
	{AT90S2333,  1, 2048,   128,  1, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0xFF, 0x00, 0xFF, "AT90S2333",   0xD7, 0xA0},
	{AT90S2343,  1, 2048,   128,  1, 1, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  0,   0,  FALSE, FALSE, TRUE,  0xFF, 0x00, 0xFF, "AT90S2343",   0xD7, 0xA0},

	{AT90S4414,  1, 4096,   256,  1, 1, FALSE, TRUE,  FALSE, TRUE,  TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0x7F, 0x80, 0x7F, "AT90S4414",   0xD7, 0xA0},
	{AT90S4433,  1, 4096,   128,  1, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0xFF, 0x00, 0xFF, "AT90S4433",   0xD7, 0xA0},
	{AT90S4434,  1, 4096,   256,  1, 1, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0xFF, 0x00, 0xFF, "AT90S4434",   0xD7, 0xA0},

	{AT90S8515,  1, 8192,   256,  1, 1, FALSE, TRUE,  FALSE, TRUE,  TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0x7F, 0x80, 0x7F, "AT90S8515",   0xD7, 0xA0},
	{AT90S8535,  1, 8192,   512,  1, 1, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  0,   0,  FALSE, TRUE,  TRUE,  0xFF, 0x00, 0xFF, "AT90S8535",   0xD7, 0xA0},
	{AT90C8534,  1, 8192,   512,  0, 1, FALSE, FALSE, FALSE, TRUE,  FALSE, 0,   0,  FALSE, TRUE,  FALSE, 0x00, 0x00, 0x00, "AT90C8534",   0xD7, 0xA0},

	{ATMEGA8,    1, 8192,   512,  2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  64,  4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega8",     0xD7, 0xA0},
	{ATMEGA8515, 1, 8192,   512,  2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  64,  4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega8515",  0xD7, 0xA0},
	{ATMEGA8535, 1, 8192,   512,  2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  64,  4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega8535",  0xD7, 0xA0},

	{ATMEGA161,  1, 16384,  512,  1, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 0,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega161",   0xD7, 0xA0},
	{ATMEGA162,  1, 16384,  512,  3, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega162",   0xD7, 0xA0},
	{ATMEGA163,  1, 16384,  512,  2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 0,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega163",   0xD7, 0xA0},
	{ATMEGA16,   1, 16384,  512,  2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega16",    0xD7, 0xA0},
	{ATMEGA169,  1, 16384,  512,  3, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega169",   0xD7, 0xA0},

	{ATMEGA323,  1, 32768,  1024, 2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 0,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega323",   0xD7, 0xA0},
	{ATMEGA32,   1, 32768,  1024, 2, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  128, 4,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega32",    0xD7, 0xA0},

	{ATMEGA103,  1, 131072, 4096, 1, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  256, 0,  FALSE, TRUE,  FALSE, 0x00, 0x00, 0x00, "ATmega103",   0xA0, 0xD7},
	{ATMEGA128,  1, 131072, 4096, 3, 1, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  256, 8,  TRUE,  TRUE,  TRUE,  0xFF, 0xFF, 0xFF, "ATmega128",   0xD7, 0xA0},

	{AT86RF401,  1, 2048,   128,  0, 1, FALSE, TRUE,  TRUE,  FALSE, TRUE,  0,   0,  FALSE, FALSE, FALSE, 0x00, 0x00, 0x00, "AT86RF401",   0xD7, 0xA0},

	{AT89S51,    1, 4096,   0,    0, 1, FALSE, TRUE,  TRUE,  FALSE, TRUE,  0,   0,  FALSE, FALSE, FALSE, 0x00, 0x00, 0x00, "AT89S51",     0xD7, 0xA0},
	{AT89S52,    1, 8192,   0,    0, 1, FALSE, TRUE,  TRUE,  FALSE, TRUE,  0,   0,  FALSE, FALSE, FALSE, 0x00, 0x00, 0x00, "AT89S52",     0xD7, 0xA0},

	{0}
};


#endif