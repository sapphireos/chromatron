file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "dummy_main_src.c"
  "CMakeFiles/bootloader.elf.dir/dummy_main_src.c.obj"
  "bootloader.elf.pdb"
  "bootloader.elf"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/bootloader.elf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
