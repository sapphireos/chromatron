# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

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
CMAKE_SOURCE_DIR = /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build

# Utility rule file for ldgen_esp32.project.ld_script.

# Include the progress variables for this target.
include esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/progress.make

esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script: esp-idf/esp32/esp32.project.ld


esp-idf/esp32/esp32.project.ld: ../../../../../components/esp32/ld/esp32.project.ld.in
esp-idf/esp32/esp32.project.ld: ../../../../../components/soc/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/heap/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/xtensa-debug-module/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/app_trace/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/freertos/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/esp_ringbuf/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/spi_flash/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/lwip/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/espcoredump/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/esp32/linker.lf
esp-idf/esp32/esp32.project.ld: ../../../../../components/esp32/ld/esp32_fragments.lf
esp-idf/esp32/esp32.project.ld: ../sdkconfig
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating esp32.project.ld"
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp32 && python /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/tools/ldgen/ldgen.py --config /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/sdkconfig --fragments /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/soc/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/heap/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/xtensa-debug-module/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_trace/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_ringbuf/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/spi_flash/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/espcoredump/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/linker.lf	/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/ld/esp32_fragments.lf --input /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/ld/esp32.project.ld.in --output /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp32/esp32.project.ld --sections /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/ldgen.section_infos --kconfig /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/Kconfig --env COMPONENT_KCONFIGS=\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_trace/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/driver/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/efuse/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_event/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/espcoredump/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/ethernet/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/freertos/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/heap/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/log/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/lwip/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/mbedtls/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/nvs_flash/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/pthread/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/spi_flash/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/tcpip_adapter/Kconfig\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/vfs/Kconfig --env COMPONENT_KCONFIGS_PROJBUILD=\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/app_update/Kconfig.projbuild\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/bootloader/Kconfig.projbuild\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esptool_py/Kconfig.projbuild\ /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/partition_table/Kconfig.projbuild --env IDF_CMAKE=y --env IDF_PATH=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf --env IDF_TARGET=esp32

ldgen_esp32.project.ld_script: esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script
ldgen_esp32.project.ld_script: esp-idf/esp32/esp32.project.ld
ldgen_esp32.project.ld_script: esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/build.make

.PHONY : ldgen_esp32.project.ld_script

# Rule to build all files generated by this target.
esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/build: ldgen_esp32.project.ld_script

.PHONY : esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/build

esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/clean:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp32 && $(CMAKE_COMMAND) -P CMakeFiles/ldgen_esp32.project.ld_script.dir/cmake_clean.cmake
.PHONY : esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/clean

esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/depend:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp32 /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp32 /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : esp-idf/esp32/CMakeFiles/ldgen_esp32.project.ld_script.dir/depend
