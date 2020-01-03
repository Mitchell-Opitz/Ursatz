#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/markdown_report.h>

/*
 * Labels are owned strings, not fixed 32-byte buffers: a report built
 * directly through the report API with labels longer than 31
 * characters renders them intact, and analysis_report_destroy frees
 * them.
 */

static char *copy_string(const char *s) {
    size_t size = strlen(s) + 1;
    char *copy = malloc(size);
    assert(copy != NULL);
    memcpy(copy, s, size);
    return copy;
}

int main(void) {
    const char *key_label = "C major (long key label beyond thirty-one characters)";
    const char *chord_label = "I (long chord label beyond thirty-one characters)";
    assert(strlen(key_label) > 31);
    assert(strlen(chord_label) > 31);

    AnalysisReport report;
    report.entries = malloc(sizeof(AnalysisReportEntry));
    assert(report.entries != NULL);
    report.entry_count = 1;
    report.key_found = true;
    report.entries[0].time_numerator = 0;
    report.entries[0].time_denominator = 1;
    report.entries[0].key_label = copy_string(key_label);
    report.entries[0].chord_label = copy_string(chord_label);

    char *markdown = markdown_report_render("long_labels.mid", &report);
    assert(markdown != NULL);
    assert(strstr(markdown, "**Detected key:** C major (long key label beyond thirty-one characters)\n") != NULL);
    assert(strstr(markdown, "- Beat 0.00: I (long chord label beyond thirty-one characters)\n") != NULL);

    free(markdown);
    analysis_report_destroy(&report);
    assert(report.entries == NULL);
    assert(report.entry_count == 0);

    printf("test_long_labels: ok\n");
    return 0;
}
