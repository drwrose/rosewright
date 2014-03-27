#ifndef __assert_h
#define __assert_h

#include <pebble.h>
#include "../resources/generated_config.h"

#ifndef NDEBUG
  // Our own poor-man's assert() function, since Pebble doesn't provide one.
  #define assert(condition) { \
    if (!(condition)) {				    \
      assert_failure(#condition, __FILE__, __LINE__);	\
    }							\
   }

void assert_failure(const char *condition, const char *filename, int line_number);

#else  // NDEBUG
  #define assert(condition)

#endif  // NDEBUG

#endif  // __assert_h
