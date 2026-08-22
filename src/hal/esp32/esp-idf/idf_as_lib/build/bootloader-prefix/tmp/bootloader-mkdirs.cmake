# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/components/bootloader/subproject"
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader"
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix"
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix/tmp"
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix/src/bootloader-stamp"
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix/src"
  "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jeremy/JEREMY/SAPPHIRE/chromatron/src/hal/esp-idf/chromatron_lib/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
