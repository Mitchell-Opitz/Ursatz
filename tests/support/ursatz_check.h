#ifndef URSATZ_GUI_TEST_GUI_CHECK_H
#define URSATZ_GUI_TEST_GUI_CHECK_H

/* The two check forms the GUI backend tests use, on the shared support (ursatz_test.h). check(cond, what) prints
 * "PASS: <what>" when it holds; check_quiet(cond, what) prints nothing then. Both count and print the failure with the
 * caller's file and line. */

#include "ursatz_test.h"

#define check(cond, what)                \
  do {                                   \
    if (CHECKF((cond), "%s", (what))) {  \
      test_pass(what);                   \
    }                                    \
  } while (0)

#define check_quiet(cond, what) ((void)CHECKF((cond), "%s", (what)))

#endif /* URSATZ_GUI_TEST_GUI_CHECK_H */
