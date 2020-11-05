file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/ldgen_section_infos.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
