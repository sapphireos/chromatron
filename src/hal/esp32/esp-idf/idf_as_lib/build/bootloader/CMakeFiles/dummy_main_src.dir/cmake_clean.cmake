file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "CMakeFiles/dummy_main_src"
  "dummy_main_src.c"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/dummy_main_src.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
