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

# Include any dependencies generated for this target.
include esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/depend.make

# Include the progress variables for this target.
include esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/progress.make

# Include the compile flags for this target's objects.
include esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/flags.make

esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.obj: esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/flags.make
esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.obj: ../../../../../components/micro-ecc/micro-ecc/uECC.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.obj"
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc && /home/jeremy/.espressif/tools/xtensa-esp32-elf/1.22.0-80-g6c4433a-5.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.obj   -c /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/micro-ecc/micro-ecc/uECC.c

esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.i"
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc && /home/jeremy/.espressif/tools/xtensa-esp32-elf/1.22.0-80-g6c4433a-5.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/micro-ecc/micro-ecc/uECC.c > CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.i

esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.s"
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc && /home/jeremy/.espressif/tools/xtensa-esp32-elf/1.22.0-80-g6c4433a-5.2.0/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/micro-ecc/micro-ecc/uECC.c -o CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.s

# Object files for target idf_component_micro-ecc
idf_component_micro__ecc_OBJECTS = \
"CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.obj"

# External object files for target idf_component_micro-ecc
idf_component_micro__ecc_EXTERNAL_OBJECTS =

esp-idf/micro-ecc/libmicro-ecc.a: esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/micro-ecc/uECC.c.obj
esp-idf/micro-ecc/libmicro-ecc.a: esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/build.make
esp-idf/micro-ecc/libmicro-ecc.a: esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C static library libmicro-ecc.a"
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc && $(CMAKE_COMMAND) -P CMakeFiles/idf_component_micro-ecc.dir/cmake_clean_target.cmake
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/idf_component_micro-ecc.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/build: esp-idf/micro-ecc/libmicro-ecc.a

.PHONY : esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/build

esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/clean:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc && $(CMAKE_COMMAND) -P CMakeFiles/idf_component_micro-ecc.dir/cmake_clean.cmake
.PHONY : esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/clean

esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/depend:
	cd /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/micro-ecc /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc /home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/examples/build_system/cmake/idf_as_lib/build/esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : esp-idf/micro-ecc/CMakeFiles/idf_component_micro-ecc.dir/depend

