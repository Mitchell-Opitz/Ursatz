#include <ursatz_analyzer/analysis_pipeline.h>

#include <stdlib.h>
#include <string.h>

#include "pipeline/chain.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/* The ends of all but the last phrase of the pass, as times for the next pass's key regions (R-039). Returns the
   count, with *out an owned array of owned times (NULL with none). False on allocation failure. */
static bool phrase_boundaries(const AnalysisChain *chain, MusicalTime ***out, size_t *count) {
    *out = NULL;
    *count = 0;
    size_t phrases = chain->form.phrases.count;
    if (phrases < 2) {
        return true;
    }
    MusicalTime **times = calloc(phrases - 1, sizeof(MusicalTime *));
    if (!times) {
        return false;
    }
    for (size_t k = 0; k + 1 < phrases; k++) {
        const MusicalTime *end = time_span_end(chain->form.phrases.items[k].span);
        if (musical_time_create(musical_time_numerator(end), musical_time_denominator(end), &times[k]) !=
            MUSICAL_TIME_OK) {
            for (size_t j = 0; j < k; j++) {
                musical_time_destroy(times[j]);
            }
            free(times);
            return false;
        }
    }
    *out = times;
    *count = phrases - 1;
    return true;
}

static void boundaries_destroy(MusicalTime **times, size_t count) {
    for (size_t i = 0; i < count; i++) {
        musical_time_destroy(times[i]);
    }
    free(times);
}

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration_denominator = 1;

    /* Pass 1 reads the key regions from the pitch classes alone. Its chords are then the functional evidence, and
       the ends of its phrases the boundaries, for the key regions of pass 2, and every stage after the key regions
       re-runs on that answer (R-039, R-043). Pass 2 runs once: no further iteration. */
    AnalysisChain chain;
    MusicalTime **boundaries = NULL;
    size_t boundary_count = 0;
    if (!chain_init(&chain, notes) || !chain_pass(&chain, NULL, 0) || !chain_keep_chord_evidence(&chain) ||
        !phrase_boundaries(&chain, &boundaries, &boundary_count)) {
        chain_destroy(&chain);
        return false;
    }
    chain_pass_destroy(&chain);
    bool ok = chain_pass(&chain, (const MusicalTime *const *)boundaries, boundary_count);
    boundaries_destroy(boundaries, boundary_count);
    if (!ok) {
        chain_destroy(&chain);
        return false;
    }
    *report = chain.report;
    memset(&chain.report, 0, sizeof(chain.report));
    chain_destroy(&chain);
    return true;
}
