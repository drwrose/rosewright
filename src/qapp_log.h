#ifndef __app_log_h
#define __app_log_h

#include <pebble.h>
#include "../resources/generated_config.h"

#ifdef NDEBUG
  // In a production build, eliminate log calls from even appearing in
  // the code.
  #define qapp_log(...)
#else
  // Otherwise, it maps to app_log().
  #define qapp_log app_log
#endif  // NDBEUG

#endif  // __assert_h
