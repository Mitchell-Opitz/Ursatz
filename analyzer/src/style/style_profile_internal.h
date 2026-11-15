#ifndef URSATZ_ANALYZER_STYLE_PROFILE_INTERNAL_H
#define URSATZ_ANALYZER_STYLE_PROFILE_INTERNAL_H

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/style_profile.h>

/*
 * Counts one piece into the profile from an analysis result, without rendering or parsing document text. The statistics
 * are those style_profile_add_document_json gives on the rendered document of the same result. STYLE_PROFILE_ERR_ARGUMENT
 * for a NULL argument or a result that did not run every chain stage.
 */
StyleProfileResult style_profile_add_result(StyleProfile *profile, const AnalyzerResult *result);

#endif
