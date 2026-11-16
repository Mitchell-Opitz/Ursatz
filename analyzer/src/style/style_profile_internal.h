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

/*
 * A tempo in quarter notes per minute as a band of 20 from 40: "<40", "40-59", "60-79", ..., "200-219", "220+". A
 * value on a boundary goes to the upper band, so 140 reads "140-159". Writes at most size bytes, always terminated.
 */
void style_label_tempo(double quarters_per_minute, char *buffer, size_t size);

/*
 * Attacks per beat rounded to the nearest 1/2 (halves up) and written as a reduced fraction: "1/2", "1", "3/2",
 * "2"; 0 reads "0" and a rounded value of 4 or more reads "4+". Writes at most size bytes, always terminated.
 */
void style_label_density(double attacks_per_beat, char *buffer, size_t size);

#endif
