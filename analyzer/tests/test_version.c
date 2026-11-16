#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/producer_key.h>
#include <ursatz_analyzer/version.h>

/* The public version header: the functions return the macros, the version is the one in project(), and the producer
   key is 16 lowercase hex digits. */

static int failures;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

int main(void) {
    check(strcmp(ursatz_analyzer_version(), URSATZ_ANALYZER_VERSION) == 0, "version function equals the macro");
    check(strcmp(ursatz_analyzer_producer_key(), URSATZ_ANALYZER_PRODUCER_KEY) == 0,
          "producer key function equals the macro");
    check(strcmp(ursatz_analyzer_version(), "0.7.0") == 0, "version is 0.7.0");
    const char *key = ursatz_analyzer_producer_key();
    check(strlen(key) == 16, "producer key is 16 characters");
    for (size_t i = 0; key[i] != '\0'; i++) {
        check((key[i] >= '0' && key[i] <= '9') || (key[i] >= 'a' && key[i] <= 'f'), "producer key is lowercase hex");
    }
    return failures ? 1 : 0;
}
