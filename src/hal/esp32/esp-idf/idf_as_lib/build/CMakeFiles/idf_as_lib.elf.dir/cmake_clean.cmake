file(REMOVE_RECURSE
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.bin"
  "bootloader/bootloader.map"
  "CMakeFiles/idf_as_lib.elf.dir/main.c.obj"
  "idf_as_lib.elf.pdb"
  "idf_as_lib.elf"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/idf_as_lib.elf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
