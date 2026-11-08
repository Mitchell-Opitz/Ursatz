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
 * One note stream as the caller numbers it: one per MIDI track and channel. program is the General MIDI program
 * (0-based, -1 for none) and note_count the number of notes of the stream among the events passed alongside.
 */
typedef struct {
    int program;
    size_t note_count;
} ScoreStream;

/*
 * Decides which note streams are the two hands of one piano. Of the streams whose programs both pass
 * score_program_uses_grand_staff, two are joined into one part when they sound together (the notes of one overlap a
 * note of the other) for at least half of the shorter stream's notes and their median pitches differ; the stream with
 * the higher median gets staff 1 and the other staff 2. With three or more such streams only the pair with the most
 * overlapping notes (both streams counted) is joined; an equal count goes to the pair with the lower stream indexes.
 * Every other stream is its own part with staff 0, which means "decide per note" (score_staves_assign_hinted).
 *   - stream_index: parallel to events, the stream (index into streams) of each note.
 *   - streams: one entry per stream, note_count must equal the notes the events give it.
 *   - out_part_of_stream and out_staff_of_stream: parallel to streams. Parts are numbered in order of each part's first
 *     stream index. out_part_count receives the number of parts.
 */
ScoreStavesResult score_staves_pair_hands(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                          size_t count, const size_t *stream_index, const ScoreStream *streams,
                                          size_t stream_count, size_t *out_part_of_stream, int *out_staff_of_stream,
                                          size_t *out_part_count);

/*
 * Writes the staff (1 treble, 2 bass) of each of count events to out_staff, parallel to events.
 *   - part_index: parallel to events, the part of each note; NULL means every note is in part 0.
 *   - part_staff_count: one entry per part, 1 or 2. Every note of a part with staff count 1 gets staff 1.
 *   - pitch_values: parallel to events, the MIDI note number of each note as midi_importer writes it (position
 *     numerator over denominator 1, middle C is 60). Only read for parts with staff count 2.
 *   - staff_hint: optional, parallel to events. 1 or 2 fixes the note's staff (in a part with staff count 2), 0 lets
 *     the rule decide. NULL means no hints.
 * For a note without a hint in a part with staff count 2:
 *   - The notes sounding at its onset (starting then, or started earlier and still held) are sorted by pitch and split
 *     at their largest adjacent interval, a tie going to the interval nearest middle C; the lower side is bass. A
 *     hinted note counts on its fixed staff: only a split with every hinted bass note below it and every hinted
 *     treble note above it is used.
 *   - A note sounding alone takes the staff of the nearer pitch of the last treble note and the last bass note that
 *     ended within the last whole note before it. With only one of the two, it takes that staff when within an octave
 *     of it. With a tie, neither, or a note further than that, 60 and above is treble and below is bass.
 * Notes starting together get their staffs before later notes look back at them. There is no fixed split pitch: a
 * bass line above middle C stays on the bass staff while the other hand sounds above it.
 */
ScoreStavesResult score_staves_assign_hinted(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                             size_t count, const size_t *part_index, const int *part_staff_count,
                                             size_t part_count, const int *staff_hint, int *out_staff);

/* score_staves_assign_hinted without hints. */
ScoreStavesResult score_staves_assign(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                      const size_t *part_index, const int *part_staff_count, size_t part_count,
                                      int *out_staff);

#endif
