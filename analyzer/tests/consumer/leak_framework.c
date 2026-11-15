/* Must not compile: a target linking only ursatz_analyzer cannot see a framework header. */
#include <ursatz_analyzer/analysis_result.h>
#include <standard-meter/framework.h>

int analyzer_consumer_leak_framework(void) { return 0; }
