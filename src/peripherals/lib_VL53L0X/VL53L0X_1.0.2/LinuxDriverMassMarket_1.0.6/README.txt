------------------------------------------------------------
----                                             		----
----    VL53L0X Linux Driver Package   	----
----                                             		----
------------------------------------------------------------

kernel/drivers/input/misc/vl53l0x: 
	VL53L0X Driver source code to be integrated in Linux kernel: support both Generic I2C and Camera CCI Bus.
		- FEAURE_USE_CCI defined to true in Makefile to enable CCI support on QCLM 8994.. platforms.
		- FEAURE_USE_CCI defined to false in Makefile to enable generic I2C support
			- if STM_TEST is defined in Makefile, it's for STM own testing board
			- if STM_TEST is not defined in Makefile, it's for QCLM 8994...platforms.
    Codes are compatible with Linux kernel version 3.2.0+ to 3.10.0+	

kernel/test/vl53l0x_test: 2 executable: vl53l0x_test and vl53l0x_reg
	Basic native test application for Kernel driver with calibration sample codes.
         - Code to be placed in android SDK's hardware folder of android native test code compilation



