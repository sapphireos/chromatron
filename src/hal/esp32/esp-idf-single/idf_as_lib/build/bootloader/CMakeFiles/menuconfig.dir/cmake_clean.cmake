file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "CMakeFiles/menuconfig"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/menuconfig.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
