#ifndef MUSIC_EXPORT_SCORE_CLEFS_H
#define MUSIC_EXPORT_SCORE_CLEFS_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "export/score_quantize.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"

/*
 * ScoreClefs (music-export): decides which clef each staff of a score is written in, measure by measure, from the
 * notes on it. Pure: nothing imported or analyzed is changed, the clefs are a separate result that
 * musicxml_export_write_score writes as <clef> elements.
 *
 * Per part and staff, a dynamic program over the measures picks treble or bass for each measure that has notes on the
 * staff. A note's cost under a clef is the number of ledger lines it needs there times its length in quarter notes
 * (exact; a note that crosses a barline counts in each measure for the part of it inside that measure). The ledger
 * lines come from the written pitch (letter and octave of the PitchSpelling), not the sounding one:
 *   - treble: lines E4 G4 B4 D5 F5; below E4 and above F5 a note needs one ledger line per line it reaches, a note on
 *     a ledger line or in the space just beyond it counting that line (C4 and B3 one, A3 and G3 two).
 *   - bass: lines G2 B2 D3 F3 A3; below G2 and above A3 the same way (C4 and D4 one, E4 and F4 two).
 * A change of clef costs SCORE_CLEF_CHANGE_COST (the same unit, quarter notes of one ledger line). A measure with no
 * notes on the staff keeps the clef before it, and a change is only ever written at a measure that has notes. When two
 * clefs cost the same the current one stays. Staff 1 of a part with more than one staff starts treble and the other
 * staves bass, at no cost; the one staff of a one-staff part starts in either clef, the cheaper (treble on a tie).
 */

typedef enum {
    SCORE_CLEF_TREBLE = 1, /* G clef on line 2 */
    SCORE_CLEF_BASS = 2    /* F clef on line 4 */
} ScoreClef;

/*
 * The clefs of a score. Every staff of every part has an entry at measure 1 first (parts in order, staves in order),
 * then the changes in measure order (a measure's changes in part, staff order). All arrays have count entries.
 *   - part: index into the parts of the call; staff: 1-based within the part.
 *   - measure: 1-based position among the measures written, 1 being the first measure laid out by the MeterMap (a
 *     pickup included), not the printed measure number.
 */
typedef struct {
    size_t count;
    size_t *part;
    int *staff;
    size_t *measure;
    ScoreClef *clef;
} ScoreClefChanges;

/* Frees the arrays and zeroes the struct. Safe on a zeroed struct and on NULL. */
void score_clef_changes_destroy(ScoreClefChanges *changes);

typedef enum {
    SCORE_CLEFS_OK = 0,
    SCORE_CLEFS_ERR_NULL_ARG,
    SCORE_CLEFS_ERR_ALLOC,
    SCORE_CLEFS_ERR_INCONSISTENT_INPUT /* a part or staff out of range, a missing pitch spelling, a negative position, a
                                          zero duration, an overflow */
} ScoreClefResult;

/*
 * Infers the clefs of count events.
 *   - part_index and staff: optional and parallel to events (NULL means part 0 and staff 1, as in score_quantize).
 *   - part_staff_count: one entry per part, at least 1. Required when part_count is above 0; with part_count 0 the
 *     result is one part of one staff.
 *   - pitch_values: optional, parallel to events; checked for NULL entries and not otherwise read (the written pitch
 *     is the PitchSpelling of each event, which every note must have).
 *   - meter_map: lays out the measures; NULL means a defaulted 4/4.
 *   - notated: optional, the result of score_quantize for the same events; when given, its notated times are used
 *     instead of the played ones (the clefs of a quantized score are those of what is written).
 * On OK, *out is filled and the caller calls score_clef_changes_destroy; on any other result *out is zeroed.
 */
ScoreClefResult score_clefs_infer(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                  const size_t *part_index, const int *staff, const int *part_staff_count,
                                  size_t part_count, const MeterMap *meter_map, const ScoreQuantized *notated,
                                  ScoreClefChanges *out);

#endif
