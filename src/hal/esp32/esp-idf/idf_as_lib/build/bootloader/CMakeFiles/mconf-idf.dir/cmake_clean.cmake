file(REMOVE_RECURSE
  "bootloader.map"
  "config/sdkconfig.h"
  "config/sdkconfig.cmake"
  "CMakeFiles/mconf-idf"
  "CMakeFiles/mconf-idf-complete"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-install"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-mkdir"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-download"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-update"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-patch"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-configure"
  "mconf-idf-prefix/src/mconf-idf-stamp/mconf-idf-build"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/mconf-idf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
