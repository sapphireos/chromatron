# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib/build

# Utility rule file for confserver.

# Include the progress variables for this target.
include CMakeFiles/confserver.dir/progress.make

CMakeFiles/confserver:
	/usr/bin/cmake -E env "COMPONENT_KCONFIGS= /home/jeremy/JEREMY/esp8266/esp-idf/components/app_trace/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/driver/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/efuse/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/esp32/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/esp_event/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/espcoredump/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/ethernet/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/freertos/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/heap/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/log/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/lwip/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/mbedtls/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/nvs_flash/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/pthread/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/spi_flash/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/tcpip_adapter/Kconfig /home/jeremy/JEREMY/esp8266/esp-idf/components/vfs/Kconfig" "COMPONENT_KCONFIGS_PROJBUILD= /home/jeremy/JEREMY/esp8266/esp-idf/components/app_update/Kconfig.projbuild /home/jeremy/JEREMY/esp8266/esp-idf/components/bootloader/Kconfig.projbuild /home/jeremy/JEREMY/esp8266/esp-idf/components/esptool_py/Kconfig.projbuild /home/jeremy/JEREMY/esp8266/esp-idf/components/partition_table/Kconfig.projbuild" python /home/jeremy/JEREMY/esp8266/esp-idf/tools/kconfig_new/confserver.py --kconfig /home/jeremy/JEREMY/esp8266/esp-idf/Kconfig --config /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib/sdkconfig

confserver: CMakeFiles/confserver
confserver: CMakeFiles/confserver.dir/build.make

.PHONY : confserver

# Rule to build all files generated by this target.
CMakeFiles/confserver.dir/build: confserver

.PHONY : CMakeFiles/confserver.dir/build

CMakeFiles/confserver.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/confserver.dir/cmake_clean.cmake
.PHONY : CMakeFiles/confserver.dir/clean

CMakeFiles/confserver.dir/depend:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib/build /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib/build /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp32/esp-idf/idf_as_lib/build/CMakeFiles/confserver.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/confserver.dir/depend

