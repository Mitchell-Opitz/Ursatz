#include <string.h>

#include <ursatz_analyzer_internal/producer_key.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/version.h>

#include "ursatz_test.h"

/* The public version header: the functions return the macros, the version is the one in project(), and the producer
   key is 16 lowercase hex digits. */

int main(void) {
    CHECKF(strcmp(analyzer_version(), URSATZ_ANALYZER_VERSION) == 0, "version function equals the macro");
    CHECKF(strcmp(analyzer_producer_key(), URSATZ_ANALYZER_PRODUCER_KEY) == 0,
           "producer key function equals the macro");
    CHECKF(strcmp(analyzer_version(), URSATZ_EXPECTED_ANALYZER_VERSION) == 0, "version is the one project() sets (%s)", URSATZ_EXPECTED_ANALYZER_VERSION);
    const char *key = analyzer_producer_key();
    CHECKF(strlen(key) == 16, "producer key is 16 characters");
    for (size_t i = 0; key[i] != '\0'; i++) {
        CHECKF((key[i] >= '0' && key[i] <= '9') || (key[i] >= 'a' && key[i] <= 'f'), "producer key is lowercase hex");
    }
    return test_finish("test_version");
}
