#ifndef URSATZ_ANALYSIS_FORM_ANALYSIS_INPUT_INTERNAL_H
#define URSATZ_ANALYSIS_FORM_ANALYSIS_INPUT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/notes/analysis_note.h"

/*
 * Internal to the form adapter: how a line of notes reads from one note to the next. True when note `index + 1` exists and starts
 * at or before the end of note `index` (no rest between them), with *semitones (when not NULL) the pitch change to it; false at the
 * end of the line and across a rest. The notes must have passed standard_form_input_finish.
 */
bool standard_form_next_note_step(const AnalysisNote *notes, size_t count, size_t index, long *semitones);

#endif
