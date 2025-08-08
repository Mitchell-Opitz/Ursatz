#ifndef URSATZ_ANALYZER_MOTIVE_PIPELINE_H
#define URSATZ_ANALYZER_MOTIVE_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/form_pipeline.h"
#include "pipeline/harmony_pipeline.h"
#include "pipeline/segmentation.h"
/* Prefixed: standard-harmony has headers of the same names. */
#include "standard-form/analysis/analysis_motive.h"

/*
 * Ursatz's standard-form motive analysis over the chain, ported from the test glue (tests/integration/motive_util.h
 * motive_soprano and motive_build_soprano, form_kernel_bridge.h form_kernel_match_windows, and
 * test_motive_mozart545.c, R-053) as a rewrite with error returns. The analyzer adds no music theory (R-018): the
 * families, their occurrences and every kind are the library's.
 *
 * The motive analysis needs a NOTE-LEVEL soprano, which the outer voice lines cannot give (they keep one note per
 * segment, so a run of sixteenths is thinned to the notes that fall on the segmentation). The soprano here is the
 * highest sounding note at each attack: at every onset, among the notes sounding then, the highest (the later attack
 * among equals); it is a soprano note when it is attacked then, so a left-hand note is the line where the right hand
 * rests. Each note carries its scale degree in the key region it starts in (a chromatic note its altered degree).
 *
 * The stage builds a form input of its own from that soprano and the chain's chord segments and cadences, and reruns
 * the phrases, ideas and themes on it with the default config: the motive analysis needs them on the same notes.
 * They are not the report's phrases, which come from the voice-segment soprano, but they tile the same cadences;
 * occurrences are linked to the report's phrases and ideas by span.
 *
 * The bass is not read: the default config has include_bass false and the report has no bass motives.
 *
 * A MotiveStage owns the form stage and the motives; it borrows the notes, the segmentation and the harmony stage,
 * which must outlive it. A piece without a meter, key regions or a soprano leaves it without motives; that is not an
 * error.
 */
typedef struct {
    FormStage form;               /* the note-level input, with its phrases, ideas and themes */
    StandardFormMotives motives;
} MotiveStage;

/* Builds the note-level input and finds the motives. Needs report's key regions (for the scale degrees). Returns false
   only on allocation failure. */
bool motive_stage_run(MotiveStage *stage, const ImportedNotes *notes, const Segmentation *segmentation,
                      const HarmonyStage *harmony, const AnalysisReport *report);

/* Copies the stage's families and occurrences into the report. Returns false only on allocation failure. */
bool motive_stage_collect(const MotiveStage *stage, AnalysisReport *report);

void motive_stage_destroy(MotiveStage *stage);

#endif
