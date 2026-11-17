/* Must not compile: a target linking only ursatz_analyzer cannot see a framework header. */
#include <ursatz_analyzer/analysis_result.h>
#include <standard-meter/analysis/meter_analysis_input.h>

int analyzer_consumer_leak_framework(void) { return 0; }
