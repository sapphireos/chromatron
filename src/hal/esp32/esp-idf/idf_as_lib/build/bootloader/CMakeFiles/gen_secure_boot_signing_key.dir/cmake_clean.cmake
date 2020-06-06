file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/gen_secure_boot_signing_key.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
