file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "CMakeFiles/bootloader"
  "bootloader.bin"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/bootloader.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
