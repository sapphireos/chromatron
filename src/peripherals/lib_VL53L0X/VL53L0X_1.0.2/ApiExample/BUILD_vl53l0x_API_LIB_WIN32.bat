rem Prepare the environment.
rem Clean objects and includes.
del object\*.o
del object\libVL53L0_Win32.a

set Debug=-O0 -g3
set gccInclude=-D_WINDOWS_ -DWIN32 -I../Api/core/inc -I../Api/platform/inc -I/serial_comms_includes/Include -Wall -c -fmessage-length=0


gcc %Debug% %gccInclude% -oobject/vl53l0x_api_calibration.o ../Api/core/src/vl53l0x_api_calibration.c
gcc %Debug% %gccInclude% -oobject/vl53l0x_api_core.o ../Api/core/src/vl53l0x_api_core.c
gcc %Debug% %gccInclude% -oobject/vl53l0x_api_ranging.o ../Api/core/src/vl53l0x_api_ranging.c
gcc %Debug% %gccInclude% -oobject/vl53l0x_api_strings.o ../Api/core/src/vl53l0x_api_strings.c

gcc %Debug% %gccInclude% -oobject/vl53l0x_api.o ../Api/core/src/vl53l0x_api.c

gcc %Debug% %gccInclude% -oobject/vl53l0x_platform_log.o ../Api/platform/src/vl53l0x_platform_log.c
gcc %Debug% %gccInclude% -oobject/vl53l0x_i2c_platform.o -Iserial_comms_includes/include ../Api/platform/src/vl53l0x_i2c_win_serial_comms.c
gcc %Debug% %gccInclude% -oobject/vl53l0x_platform.o ../Api/platform/src/vl53l0x_platform.c

ar -r object/libVL53L0X_Win32.a object/vl53l0x_api.o object/vl53l0x_api_calibration.o object/vl53l0x_api_core.o object/vl53l0x_api_ranging.o object/vl53l0x_api_strings.o object/vl53l0x_platform_log.o object/vl53l0x_i2c_platform.o object/vl53l0x_platform.o

cd 

@pause
