file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "CMakeFiles/ldgen"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/ldgen.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
