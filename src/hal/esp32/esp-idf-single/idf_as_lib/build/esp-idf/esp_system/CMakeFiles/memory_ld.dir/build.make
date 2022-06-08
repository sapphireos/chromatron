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
CMAKE_SOURCE_DIR = /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build

# Utility rule file for memory_ld.

# Include the progress variables for this target.
include esp-idf/esp_system/CMakeFiles/memory_ld.dir/progress.make

esp-idf/esp_system/CMakeFiles/memory_ld: esp-idf/esp_system/ld/memory.ld


esp-idf/esp_system/ld/memory.ld: config/sdkconfig.h
esp-idf/esp_system/ld/memory.ld: ../../components/esp_system/ld/esp32/memory.ld.in
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating memory.ld linker script..."
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/esp-idf/esp_system && /home/jeremy/.espressif/tools/xtensa-esp32-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc -C -P -x c -E -o /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/esp-idf/esp_system/ld/memory.ld -I /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/config -I /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_system/ld /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_system/ld/esp32/memory.ld.in

memory_ld: esp-idf/esp_system/CMakeFiles/memory_ld
memory_ld: esp-idf/esp_system/ld/memory.ld
memory_ld: esp-idf/esp_system/CMakeFiles/memory_ld.dir/build.make

.PHONY : memory_ld

# Rule to build all files generated by this target.
esp-idf/esp_system/CMakeFiles/memory_ld.dir/build: memory_ld

.PHONY : esp-idf/esp_system/CMakeFiles/memory_ld.dir/build

esp-idf/esp_system/CMakeFiles/memory_ld.dir/clean:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/esp-idf/esp_system && $(CMAKE_COMMAND) -P CMakeFiles/memory_ld.dir/cmake_clean.cmake
.PHONY : esp-idf/esp_system/CMakeFiles/memory_ld.dir/clean

esp-idf/esp_system/CMakeFiles/memory_ld.dir/depend:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/esp_system /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/esp-idf/esp_system /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/esp-idf/esp_system/CMakeFiles/memory_ld.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : esp-idf/esp_system/CMakeFiles/memory_ld.dir/depend
