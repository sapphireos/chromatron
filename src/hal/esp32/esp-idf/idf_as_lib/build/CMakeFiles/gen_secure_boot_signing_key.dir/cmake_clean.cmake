file(REMOVE_RECURSE
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.bin"
  "bootloader/bootloader.map"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/gen_secure_boot_signing_key.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
