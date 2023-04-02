#include <assert.h>
#include <string.h>

#include <ursatz_composer/composer.h>
#include <ursatz_analyzer/analysis_pipeline.h>

/*
 * The library links and reports its version, and the Ursatz and
 * Ursatz-Analyzer submodules resolve through it.
 */

int main(void) {
    assert(strlen(composer_version()) > 0);

    AnalysisReport report = {0};
    analysis_report_destroy(&report);
    return 0;
}
