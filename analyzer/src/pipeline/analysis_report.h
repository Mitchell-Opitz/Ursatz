#ifndef URSATZ_ANALYZER_ANALYSIS_REPORT_H
#define URSATZ_ANALYZER_ANALYSIS_REPORT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    long time_numerator;
    unsigned long time_denominator;
    char chord_label[32]; /* "no match" when nothing was identified */
    char key_label[32];   /* "no match" when nothing was identified */
} AnalysisReportEntry;

typedef struct {
    AnalysisReportEntry *entries;
    size_t entry_count;
    bool key_found;
} AnalysisReport;

void analysis_report_destroy(AnalysisReport *report);

#endif
