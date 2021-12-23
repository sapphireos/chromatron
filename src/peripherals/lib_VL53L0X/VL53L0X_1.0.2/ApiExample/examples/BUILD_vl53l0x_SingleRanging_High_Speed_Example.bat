del object\*.o

gcc -DWIN32 -D_WINDOWS_ -I. -I../ -O3 -Wall -c -fmessage-length=0 -oobject/get_nucleo_serial_comm.o src/get_nucleo_serial_comm.c
gcc -DWIN32 -D_WINDOWS_  -I. -I../serial_comms_includes/include  -I../../Api/core/inc -I../../Api/platform/inc -O3 -Wall -c -fmessage-length=0 -oobject/vl53l0x_SingleRanging_High_Speed_Example.o src/vl53l0x_SingleRanging_High_Speed_Example.c

g++ -DWIN32 -D_WINDOWS_  -L. -L../object -ovl53l0x_SingleRanging_High_Speed_Example.exe object/get_nucleo_serial_comm.o object/vl53l0x_SingleRanging_High_Speed_Example.o -lVL53L0X_Win32 -lserial_comms -static-libgcc

@pause