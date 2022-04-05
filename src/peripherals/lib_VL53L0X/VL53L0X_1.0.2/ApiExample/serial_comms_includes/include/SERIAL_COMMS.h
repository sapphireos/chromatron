#ifdef COMMS_EXPORTS
	#define COMMS_API __declspec(dllexport)
#else
	#define COMMS_API 
#endif

#ifdef __cplusplus
extern "C"
{
#endif


//////////////////////////////////////////////////////////////////////////////
//
// Comms type-inspecific functions
//

// Error text
COMMS_API DWORD SERIAL_COMMS_Get_Error_Text(
	char*	strError);

// Display name
COMMS_API DWORD SERIAL_COMMS_Get_Display_String(
	char*	strDisplay);

// Comms types
COMMS_API DWORD SERIAL_COMMS_Get_Num_Comms_Types();

COMMS_API DWORD SERIAL_COMMS_Get_Comms_Type_String(
	DWORD	dwIndex,
	char*	strCommsType);

// Enum ST USB devices
COMMS_API DWORD SERIAL_COMMS_Enum_Devices(
	DWORD	dwDeviceIDArraySize,
	BYTE*	pcDeviceIDArray,
	DWORD*	pdwNumDevicesFound);


//////////////////////////////////////////////////////////////////////////////
//
// UBOOT comms type-specific functions
//

// UBOOT init
COMMS_API DWORD SERIAL_COMMS_Init_UBOOT(
	DWORD	dwDeviceID,
	DWORD	argc,		// Number of strings in array argv
	char*	argv[]);	// Array of command-line argument strings

// UBOOT fini
COMMS_API DWORD SERIAL_COMMS_Fini_UBOOT();

// UBOOT read - 16 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Read_UBOOT(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);

// UBOOT write - 16 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Write_UBOOT(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);



COMMS_API DWORD SERIAL_COMMS_Init_UART_PRAGUE(
	DWORD	dwDeviceID,
	DWORD	argc,		// Number of strings in array argv
	char*	argv[]);	// Array of command-line argument strings

// UBOOT fini
COMMS_API DWORD SERIAL_COMMS_Fini_UART_PRAGUE();

// UBOOT read - 16 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Read_UART_PRAGUE(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);

// UBOOT write - 16 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Write_UART_PRAGUE(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);


//////////////////////////////////////////////////////////////////////////////
//
// UBOOT_CCI comms type-specific functions
//

// UBOOT_CCI init
COMMS_API DWORD SERIAL_COMMS_Init_UBOOT_CCI(
	DWORD	dwDeviceID,
	DWORD	argc,		// Number of strings in array argv
	char*	argv[]);	// Array of command-line argument strings

// UBOOT_CCI fini
COMMS_API DWORD SERIAL_COMMS_Fini_UBOOT_CCI();

// UBOOT_CCI read - 16 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Read_UBOOT_CCI(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);

// UBOOT_CCI write - 16 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Write_UBOOT_CCI(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);


//////////////////////////////////////////////////////////////////////////////
//
// UBOOT_V2W comms type-specific functions
//

// UBOOT_V2W init
COMMS_API DWORD SERIAL_COMMS_Init_UBOOT_V2W(
	DWORD	dwDeviceID,
	DWORD	argc,		// Number of strings in array argv
	char*	argv[]);	// Array of command-line argument strings

// UBOOT_V2W fini
COMMS_API DWORD SERIAL_COMMS_Fini_UBOOT_V2W();

// UBOOT_V2W read - 8 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Read_UBOOT_V2W(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);

// UBOOT_V2W write - 8 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Write_UBOOT_V2W(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);


//////////////////////////////////////////////////////////////////////////////
//
// UBOOT_I2C comms type-specific functions
//

// UBOOT_I2C init
COMMS_API DWORD SERIAL_COMMS_Init_UBOOT_I2C(
	DWORD	dwDeviceID,
	DWORD	argc,		// Number of strings in array argv
	char*	argv[]);	// Array of command-line argument strings

// UBOOT_I2C fini
COMMS_API DWORD SERIAL_COMMS_Fini_UBOOT_I2C();

// UBOOT_I2C read - 8 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Read_UBOOT_I2C(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);

// UBOOT_I2C write - 8 bit index, variable length data
COMMS_API DWORD SERIAL_COMMS_Write_UBOOT_I2C(
	DWORD	dwAddress,
	DWORD	dwIndexHi,
	DWORD	dwIndexLo,
	BYTE*	pcValues,
	DWORD	dwBufferSize);


#ifdef __cplusplus
}
#endif
