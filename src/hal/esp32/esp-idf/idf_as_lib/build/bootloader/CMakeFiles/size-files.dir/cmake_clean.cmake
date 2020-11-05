file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "CMakeFiles/size-files"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/size-files.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
