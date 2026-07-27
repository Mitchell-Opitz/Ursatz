#ifndef MUSIC_EXPORT_SCORE_STAVES_H
#define MUSIC_EXPORT_SCORE_STAVES_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"

/*
 * ScoreStaves (music-export): decides which instruments are written on a grand staff (treble and bass) and, for
 * those, which staff each note belongs to. Pure: nothing imported or analyzed is changed, the staff numbers are a
 * separate result that the caller passes on as MusicXmlExportScore.staff.
 */

/*
 * True when a General MIDI program (0-based) is written on two staves: the pianos, harpsichord and clavinet (0 to 7),
 * celesta (8), the organs and accordion (16 to 21) and harp (46). False for every other program and for -1 (none).
 */
bool score_program_uses_grand_staff(int program);

typedef enum {
    SCORE_STAVES_OK = 0,
    SCORE_STAVES_ERR_NULL_ARG,
    SCORE_STAVES_ERR_ALLOC,
    SCORE_STAVES_ERR_INCONSISTENT_INPUT /* a part index out of range, a staff count other than 1 or 2, a missing
                                           pitch value, a negative position, a zero duration, an overflow */
} ScoreStavesResult;

/*
 * Writes the staff (1 treble, 2 bass) of each of count events to out_staff, parallel to events.
 *   - part_index: parallel to events, the part of each note; NULL means every note is in part 0.
 *   - part_staff_count: one entry per part, 1 or 2. Every note of a part with staff count 1 gets staff 1.
 *   - pitch_values: parallel to events, the MIDI note number of each note as midi_importer writes it (position
 *     numerator over denominator 1, middle C is 60). Only read for parts with staff count 2.
 * For a part with staff count 2 the split point is middle C. Notes that start together and span more than an octave
 * across middle C split at their largest adjacent interval (the lower side is the bass). Any other note at or above
 * 60 is treble and below is bass, except from 55 to 64: there the note takes the staff of the nearer pitch of the
 * last treble note and the last bass note that ended within the last whole note before it; with no such note, or a
 * tie, the rule above applies.
 */
ScoreStavesResult score_staves_assign(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                      const size_t *part_index, const int *part_staff_count, size_t part_count,
                                      int *out_staff);

#endif
