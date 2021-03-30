file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "CMakeFiles/mconf-idf"
  "CMakeFiles/mconf-idf-complete"
  "kconfig_bin/mconf-idf"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-build"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-configure"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-download"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-install"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-mkdir"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-patch"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-update"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/mconf-idf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
