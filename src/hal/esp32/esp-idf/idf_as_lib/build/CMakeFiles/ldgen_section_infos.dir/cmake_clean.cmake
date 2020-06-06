file(REMOVE_RECURSE
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.bin"
  "bootloader/bootloader.map"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/ldgen_section_infos.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
