file(REMOVE_RECURSE
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "CMakeFiles/idf_as_lib.elf.dir/main.c.obj"
  "idf_as_lib.elf"
  "idf_as_lib.elf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/idf_as_lib.elf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
