#ifndef URSATZ_IO_IMPORTED_REALIZATION_H
#define URSATZ_IO_IMPORTED_REALIZATION_H

#include "io/imported_score.h"
#include "realization/realization.h"
#include "realization/work.h"

/*
 * The realization of an imported score (R-628): what the import holds as a work of the realization group, so the notes an import
 * gives have ids a reading can name. Work `w1` (no title, no composer) holds movement `m1` (the title, or none): the file's meter map and
 * tempo map copied, its key signatures (the mode of the event's hint), no home region, no lines. One performer part for each part of the
 * import, in its order: the import's id when it is a realization id and no earlier part holds it, else `p<n>`; the instrument
 * `omt.instrument.piano` for a General MIDI program 0 to 7, `general-midi.instrument.percussion` for a program of -1 and else
 * `general-midi.instrument.program-<n>`; the import's name (`Part <n>` for none, cut to 63 characters); the number among the parts of that
 * instrument; one player; its staves (1 to 4) and program (0 when unknown). A score with no parts gets one part `p1` named `Piano`.
 *
 * Event i is a note of the part `source.part_index[i]`: the event's id when it is a realization id the part does not hold yet, else
 * `n<i+1>`; its onset, its duration, its MIDI number from `pitch_values[i]`, its spelling when `source.spelling_notated`, its staff (1
 * when the score names none) and its voice, 1 plus the place of the event's voice reference among the part's distinct ones in order of
 * first appearance (events with none share one); no derivation.
 *
 * IMPORTED_SCORE_ERR_ARGUMENT for a NULL score or out, _ALLOC, and _ERR_FORMAT for a value the realization refuses (nothing is then built).
 * On success *out is the work, owned by the caller, and, when `note_ids` is not NULL, *note_ids is an array of score->count ids (owned,
 * free it): the id of the note made from each import index.
 */
ImportedScoreResult imported_score_realization(const ImportedScore *score, const char *title, Work **out,
                                               char (**note_ids)[REALIZATION_ID_MAX]);

#endif
