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

# Utility rule file for idf_component_esp_ringbuf_sections_info.

# Include the progress variables for this target.
include esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/progress.make

esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info: esp-idf/esp_ringbuf/idf_component_esp_ringbuf.sections_info


esp-idf/esp_ringbuf/idf_component_esp_ringbuf.sections_info: esp-idf/esp_ringbuf/libesp_ringbuf.a
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating idf_component_esp_ringbuf.sections_info"
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp_ringbuf && /home/jeremy/.espressif/tools/xtensa-esp32-elf/1.22.0-80-g6c4433a-5.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-objdump /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp_ringbuf/libesp_ringbuf.a -h > /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp_ringbuf/idf_component_esp_ringbuf.sections_info

idf_component_esp_ringbuf_sections_info: esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info
idf_component_esp_ringbuf_sections_info: esp-idf/esp_ringbuf/idf_component_esp_ringbuf.sections_info
idf_component_esp_ringbuf_sections_info: esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/build.make

.PHONY : idf_component_esp_ringbuf_sections_info

# Rule to build all files generated by this target.
esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/build: idf_component_esp_ringbuf_sections_info

.PHONY : esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/build

esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/clean:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp_ringbuf && $(CMAKE_COMMAND) -P CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/cmake_clean.cmake
.PHONY : esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/clean

esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/depend:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib /home/jeremy/JEREMY/SAPPHIRE/espressif/esp-idf/components/esp_ringbuf /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp_ringbuf /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : esp-idf/esp_ringbuf/CMakeFiles/idf_component_esp_ringbuf_sections_info.dir/depend

