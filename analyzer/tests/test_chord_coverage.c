#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "events/note_event.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Chord coverage: the fraction of the piece's sounding notes, by duration,
 * that the analysis accounts for. A note sounding in a segment is accounted
 * for when the segment has an identified chord and the note is either one of
 * the chord's tones or a non-chord tone the library classified. A note in a
 * segment with no chord, or one the library left unclassified, is not.
 *
 * Each segment counts a note for the time the note sounds inside the segment,
 * so a note held across segments counts in each, and the total is the
 * sounding time of every note in every segment.
 *
 * John Field, Nocturne No. 1 in E-flat major must be above 90%. The other
 * four reference pieces must still produce chords. The files are Ursatz's
 * framework-v1 corpus (URSATZ_ANALYZER_FIXTURE_DIR/framework-v1-corpus).
 */

#define COVERAGE_TARGET 0.90

typedef struct {
    const char *file;
    bool coverage_asserted;
} Piece;

static const Piece PIECES[] = {
    {"Field 1.mid", true}, {"Bach 114.mid", false}, {"Clementi 36.mid", false},
    {"Field 5.mid", false}, {"Mozart 545.mid", false},
};

static double as_double(long numerator, unsigned long denominator) {
    return (double)numerator / (double)denominator;
}

/* Time the note sounds inside [start, end), in whole notes. */
static double sounding_inside(const NoteEvent *note, double start, double end) {
    const MusicalTime *onset_time = temporal_anchor_position(note_event_temporal_anchor(note));
    const Duration *length = note_event_duration(note);
    double onset = as_double(musical_time_numerator(onset_time), musical_time_denominator(onset_time));
    double release = onset + as_double((long)duration_numerator(length), duration_denominator(length));
    double from = onset > start ? onset : start;
    double to = release < end ? release : end;
    return to > from ? to - from : 0.0;
}

static bool is_unclassified_tone(const AnalysisReport *report, size_t chord_index, size_t note_id) {
    for (size_t i = 0; i < report->non_chord_tone_count; i++) {
        const AnalysisNonChordTone *tone = &report->non_chord_tones[i];
        if (tone->chord_index == chord_index && tone->note_id == note_id && tone->type == NULL) {
            return true;
        }
    }
    return false;
}

static double coverage(const ImportedNotes *notes, const AnalysisReport *report, size_t *matched_chords) {
    double total = 0.0;
    double covered = 0.0;
    *matched_chords = 0;
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        bool matched = entry->chord.label != NULL;
        if (matched) {
            (*matched_chords)++;
        }
        double start = as_double(entry->time_numerator, entry->time_denominator);
        double end = as_double(entry->end_numerator, entry->end_denominator);
        for (size_t n = 0; n < entry->note_id_count; n++) {
            double sounding = sounding_inside(notes->events[entry->note_ids[n]], start, end);
            total += sounding;
            if (matched && !is_unclassified_tone(report, i, entry->note_ids[n])) {
                covered += sounding;
            }
        }
    }
    return total > 0.0 ? covered / total : 0.0;
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, PIECES[i].file);

        ImportedNotes notes;
        assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
        AnalysisReport report;
        assert(run_analysis_pipeline(&notes, &report));

        size_t matched_chords = 0;
        double fraction = coverage(&notes, &report, &matched_chords);
        printf("%s: %zu of %zu segments have a chord, chord coverage %.1f%%\n", PIECES[i].file, matched_chords,
               report.entry_count, fraction * 100.0);

        assert(matched_chords > 0);
        if (PIECES[i].coverage_asserted) {
            if (!(fraction > COVERAGE_TARGET)) {
                /* A plain exit, not an abort, so the test can be marked WILL_FAIL. */
                fprintf(stderr, "%s: chord coverage %.1f%% is not above %.0f%%\n", PIECES[i].file,
                        fraction * 100.0, COVERAGE_TARGET * 100.0);
                return 1;
            }
        }

        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }

    printf("test_chord_coverage: ok\n");
    return 0;
}
