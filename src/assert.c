#include "wright.h"
#include "assert.h"
#include "qapp_log.h"

#ifndef NDEBUG
void assert_failure(const char *condition, const char *filename, int line_number) {
  qapp_log(APP_LOG_LEVEL_ERROR, filename, line_number, "assertion failed: %s", condition);

  // Force a crash.
  char *null_ptr = 0;
  (*null_ptr) = 0;
}
#endif  // NDEBUG
