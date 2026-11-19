#ifndef URSATZ_ANALYZER_ANALYSIS_DOCUMENT_INTERNAL_H
#define URSATZ_ANALYZER_ANALYSIS_DOCUMENT_INTERNAL_H

#include <ursatz_analyzer/analysis_document.h>

#include "cJSON.h"

/* Library-private view of AnalysisDocument: renderers read the cJSON
   tree directly. */
struct AnalysisDocument {
    cJSON *root;
};

#endif
