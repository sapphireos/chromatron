# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# compile ASM with /home/jeremy/.espressif/tools/xtensa-esp32-elf/1.22.0-97-gc752ad5-5.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc
# compile C with /home/jeremy/.espressif/tools/xtensa-esp32-elf/1.22.0-97-gc752ad5-5.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc
ASM_FLAGS =   -mlongcalls -Os -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -nostdlib -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -ggdb

ASM_DEFINES = -DESP_PLATFORM -DGCC_NOT_5_2_0=0 -DHAVE_CONFIG_H -DIDF_VER=\"v3.3.5-16-g03e1e5bba\" -DMBEDTLS_CONFIG_FILE=\"mbedtls/esp_config.h\"

ASM_INCLUDES = -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/config -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/driver/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_ringbuf/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/tcpip_adapter/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include/apps -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include/apps/sntp -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/lwip/src/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/include/arch -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include_compat -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/tcp_isn -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/vfs/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_event/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/log/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_trace/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_update/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/spi_flash/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/bootloader_support/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/ethernet/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/mbedtls/port/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/mbedtls/mbedtls/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/newlib/platform_include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/newlib/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/heap/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/nvs_flash/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/pthread/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/smartconfig_ack/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/wpa_supplicant/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/wpa_supplicant/port/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/xtensa-debug-module/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/espcoredump/include 

C_FLAGS = -mlongcalls   -mlongcalls -Os -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -nostdlib -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -ggdb -std=gnu99 -Wno-old-style-declaration

C_DEFINES = -DESP_PLATFORM -DGCC_NOT_5_2_0=0 -DHAVE_CONFIG_H -DIDF_VER=\"v3.3.5-16-g03e1e5bba\" -DMBEDTLS_CONFIG_FILE=\"mbedtls/esp_config.h\"

C_INCLUDES = -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/config -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/driver/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_ringbuf/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/tcpip_adapter/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include/apps -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include/apps/sntp -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/lwip/src/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/include/arch -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include_compat -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/tcp_isn -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/vfs/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_event/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/log/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_trace/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_update/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/spi_flash/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/bootloader_support/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/ethernet/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/mbedtls/port/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/mbedtls/mbedtls/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/newlib/platform_include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/newlib/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/heap/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/nvs_flash/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/pthread/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/smartconfig_ack/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/wpa_supplicant/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/wpa_supplicant/port/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/xtensa-debug-module/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/espcoredump/include 

# Custom flags: esp-idf/esp32/CMakeFiles/idf_component_esp32.dir/cpu_start.c.obj_FLAGS = -fno-stack-protector

# Custom flags: esp-idf/esp32/CMakeFiles/idf_component_esp32.dir/stack_check.c.obj_FLAGS = -fno-stack-protector

