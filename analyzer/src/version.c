#include <ursatz_analyzer_internal/producer_key.h>
#include <ursatz_analyzer_internal/version.h>

const char *analyzer_version(void) { return URSATZ_ANALYZER_VERSION; }

const char *analyzer_producer_key(void) { return URSATZ_ANALYZER_PRODUCER_KEY; }
