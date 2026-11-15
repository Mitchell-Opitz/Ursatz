#ifndef URSATZ_ANALYZER_COUNTERPOINT_PIPELINE_H
#define URSATZ_ANALYZER_COUNTERPOINT_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>

/*
 * The counterpoint check (--check-counterpoint), Backlog A6.3: two-voice input checked against one species of Ursatz's
 * standard-counterpoint. The analyzer adds no music theory (R-018): it finds the two voices and the lines the species
 * need, and every verdict is the library's.
 *
 * The two voices are the input's own streams (ImportedNotes.streams, a MIDI track or a MusicXML part) when there are
 * exactly two, the higher-sounding one the upper voice; for a single stream they are the kernel's voice separation
 * (analysis/voice_separation.h), which must find exactly two voices with no note joined to another. Any other input has
 * no counterpoint check.
 */

typedef enum {
    COUNTERPOINT_CHECK_OK = 0,
    COUNTERPOINT_CHECK_NOT_TWO_VOICES, /* the input is not two voices; nothing is written to the report */
    COUNTERPOINT_CHECK_ERR_ALLOC
} CounterpointCheckResult;

/* Runs the check of species (1 to 5) with the cantus firmus in the upper or the lower voice and fills
   report->counterpoint_check. The report must already hold the analysis (the home key gives the tonic). */
CounterpointCheckResult counterpoint_check_collect(const ImportedNotes *notes, int species, bool cantus_upper,
                                                   AnalysisReport *report);

#endif
