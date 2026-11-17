#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "analysis/notes/analysis_note.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

/*
 * The standard-meter rhythm and meter adapter (L3.8): syncopation, hemiola, hypermeter and per-phrase rhythmic density read
 * from real music. standard-meter includes no other framework and no kernel analysis (the architecture test enforces both):
 * the input is the neutral structs below, which the caller fills (tests/integration/ builds them from the import, the
 * standard-form phrases and the harmonic segments). The rules this adapter applies live in the framework proper:
 * syncopation_rules.h, hemiola_rules.h and hypermeter_rules.h.
 *
 * THE INPUT is built with the functions below: init with the MeterMap and the AnalysisNotes (borrowed), add phrases and chord
 * changes, run an analysis, destroy. Ids of phrases and chord changes are copied. TimeSpans, MusicalTimes and the MeterMap are BORROWED: they must outlive the input and every
 * result computed from it. Times are exact; nothing here uses floating point for a decision.
 *
 *   note          an AnalysisNote: note_id; its TimeSpan (onset = start, end = start of the next attack or release, as the caller
 *                 read it; a tied note is the caller's to merge into one note, since a tie is a long note here); and the
 *                 `line` it belongs to (its voice: any whole number the caller picks, 0 for the soprano and 1 for the bass,
 *                 say; none reads as 0). Notes need not be sorted
 *   phrase        id; its TimeSpan; the TimeSpan of its cadence arrival, or NULL for none; an expansion kind and the number
 *                 of measures it adds (internal, external) or removes (contraction), as the form adapter reports them; and
 *                 whether the cadence is elided with the next phrase (its arrival is the next phrase's first measure).
 *                 Phrases are given in time order
 *   chord change  id; the MusicalTime at which the harmony changes. Optional: with none, the hemiola evidence from the
 *                 harmony is not available (hemiola_rules.h)
 */

#define STANDARD_METER_ID_MAX 64

typedef enum {
    STANDARD_METER_ANALYSIS_OK = 0,
    STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT,
    STANDARD_METER_ANALYSIS_ERR_INVALID,
    STANDARD_METER_ANALYSIS_ERR_ALLOC,
    STANDARD_METER_ANALYSIS_ERR_OVERFLOW /* an exact time does not fit 64 bits; nothing is wrapped */
} StandardMeterAnalysisStatus;

typedef enum {
    STANDARD_METER_EXPANSION_NONE = 0,
    STANDARD_METER_EXPANSION_INTERNAL,
    STANDARD_METER_EXPANSION_EXTERNAL,
    STANDARD_METER_EXPANSION_CONTRACTION
} StandardMeterExpansionKind;

typedef struct {
    char id[STANDARD_METER_ID_MAX];
    TimeSpan span;
    bool has_cadence_arrival;
    TimeSpan cadence_arrival;        /* meaningful when has_cadence_arrival */
    StandardMeterExpansionKind expansion;
    int expansion_measures; /* measures added (internal, external) or removed (contraction); 0 with no expansion */
    bool elided_with_next;
} StandardMeterPhrase;

typedef struct {
    char id[STANDARD_METER_ID_MAX];
    MusicalTime time;
} StandardMeterChordChange;

/*
 * Configuration. `onset_grid_denominator`, `onset_tolerance_denominator` and `release_tolerance_denominator` are the snap: an
 * attack time within 1/onset_tolerance_denominator of a whole note of a multiple of 1/onset_grid_denominator is read as that
 * multiple, and so is a release time within 1/release_tolerance_denominator of one. It carries a performed file's small timing
 * differences into the metric grid without moving a time that is on another grid (a triplet). Releases get the wider tolerance
 * because a performer lets a note run on into the next (a legato overlap) or lifts early (staccato), by more than an attack is
 * ever late, and neither changes where the written note ends. The release tolerance may not be wider than half a grid step
 * (denominator at least twice the grid's), so no time has two nearest multiples. Exact input is unchanged by the snap.
 */
typedef struct {
    bool syncopation_all_notes;   /* false: each line is read on its own; true: all notes as one stream */
    unsigned hemiola_min_evidence; /* evidence types that must agree for a hemiola (default 2) */
    unsigned onset_grid_denominator;
    unsigned onset_tolerance_denominator;
    unsigned release_tolerance_denominator;
} StandardMeterConfig;

typedef struct {
    const MeterMap *map;
    const AnalysisNote *notes; /* borrowed, in any order */
    size_t note_count;
    StandardMeterPhrase *phrases;
    size_t phrase_count;
    size_t phrase_capacity;
    StandardMeterChordChange *chord_changes;
    size_t chord_change_count;
    size_t chord_change_capacity;
} StandardMeterInput;

/* The defaults: per line, 2 evidence types, a 1/32 grid with a 1/256 tolerance for attacks and 1/64 for releases. */
StandardMeterConfig standard_meter_config_default(void);
bool standard_meter_config_valid(const StandardMeterConfig *config);
/* Snaps *time per the configuration (an attack, or a release with the wider tolerance); *changed (when not NULL) is set when it
   moved. False when the snap does not fit a Rational; *time is then untouched. */
bool standard_meter_snap_time(const StandardMeterConfig *config, Rational *time, bool release, bool *changed);

/* The line a note belongs to: its voice, or 0 when the voice is ANALYSIS_INDEX_NONE (any whole number the caller picks: 0 for
   the soprano and 1 for the bass, say). */
static inline int standard_meter_note_line(const AnalysisNote *note) {
    return note->voice == ANALYSIS_INDEX_NONE ? 0 : (int)note->voice;
}

/* An input over the notes, which are borrowed (and with their ids, spans and the map must outlive the input and every result);
   phrases and chord changes are added after and owned. ERR_INVALID for a note with an empty or too long id or a span
   that is not ahead (end after start); ERR_NULL_ARGUMENT for a NULL input or map, or notes NULL with a count. */
StandardMeterAnalysisStatus standard_meter_input_init(StandardMeterInput *input, const MeterMap *map,
                                                      const AnalysisNote *notes, size_t note_count);

/* Adds a phrase; the spans are copied (cadence_arrival is NULL for a phrase with none). ERR_INVALID for a bad id or span, a negative measure count, or a measure count with no expansion. */
StandardMeterAnalysisStatus standard_meter_input_add_phrase(StandardMeterInput *input, const char *id, TimeSpan span,
                                                            const TimeSpan *cadence_arrival,
                                                            StandardMeterExpansionKind expansion, int expansion_measures,
                                                            bool elided_with_next);

StandardMeterAnalysisStatus standard_meter_input_add_chord_change(StandardMeterInput *input, const char *id,
                                                                  MusicalTime time);

void standard_meter_input_destroy(StandardMeterInput *input);

#endif
