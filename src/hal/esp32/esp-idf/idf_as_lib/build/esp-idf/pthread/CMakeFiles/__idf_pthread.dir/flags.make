# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# compile C with /home/jeremy/.espressif/tools/xtensa-esp32-elf/esp-2021r2-8.4.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc
C_FLAGS = -mlongcalls -Wno-frame-address   -ffunction-sections -fdata-sections -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -ggdb -Os -freorder-blocks -fmacro-prefix-map=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib=. -fmacro-prefix-map=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf=IDF -fstrict-volatile-bitfields -Wno-error=unused-but-set-variable -fno-jump-tables -fno-tree-switch-conversion -std=gnu99 -Wno-old-style-declaration -D_GNU_SOURCE -DIDF_VER=\"v4.4-6-g47b71b7e89\" -DESP_PLATFORM -D_POSIX_READER_WRITER_LOCKS

C_DEFINES = 

C_INCLUDES = -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/config -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/pthread/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/newlib/platform_include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/include/esp_additions/freertos -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/port/xtensa/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/include/esp_additions -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_hw_support/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_hw_support/include/soc -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_hw_support/include/soc/esp32 -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_hw_support/port/esp32/. -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/heap/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/log/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include/apps -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/include/apps/sntp -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/lwip/src/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/port/esp32/include/arch -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/esp32/. -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/hal/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/hal/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/hal/platform_port/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_rom/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_rom/include/esp32 -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_rom/esp32 -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_common/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_system/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_system/port/soc -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_system/port/public_compat -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/xtensa/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/xtensa/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/driver/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/driver/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_pm/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_ringbuf/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/vfs/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_wifi/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_event/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_netif/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/tcpip_adapter/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_phy/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_phy/esp32/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_ipc/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_trace/include -I/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_timer/include 

