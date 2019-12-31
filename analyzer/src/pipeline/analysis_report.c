#include <ursatz_analyzer/analysis_report.h>

#include <stdlib.h>

void analysis_report_destroy(AnalysisReport *report) {
    if (!report) {
        return;
    }
    free(report->entries);
    report->entries = NULL;
    report->entry_count = 0;
    report->key_found = false;
}
