#include "assert.h"

void assert_failure(const char *condition, const char *filename, int line_number) {
  app_log(APP_LOG_LEVEL_ERROR, filename, line_number, "assertion failed: %s", condition);

  // Force a crash.
  char *null_ptr = 0;
  (*null_ptr) = 0;
}


