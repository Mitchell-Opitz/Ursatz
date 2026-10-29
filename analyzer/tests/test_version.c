#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/version.h>

/* The public version header: the functions return the macros, the version is the one in project(), and the Ursatz
   commit is never empty. */

static int failures;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

int main(void) {
    check(strcmp(ursatz_analyzer_version(), URSATZ_ANALYZER_VERSION) == 0, "version function equals the macro");
    check(strcmp(ursatz_analyzer_ursatz_commit(), URSATZ_ANALYZER_URSATZ_COMMIT) == 0,
          "commit function equals the macro");
    check(strcmp(ursatz_analyzer_version(), "0.3.0") == 0, "version is 0.3.0");
    check(ursatz_analyzer_ursatz_commit()[0] != '\0', "commit is not empty");
    return failures ? 1 : 0;
}
