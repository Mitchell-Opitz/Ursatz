#ifndef URSATZ_CHAIN_CHAIN_SCORE_INPUT_H
#define URSATZ_CHAIN_CHAIN_SCORE_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <io/imported_score.h>

#include "theory/tonality/spelled_key.h"
#include "analysis/notes/analysis_note.h"
#include "analysis/notes/analysis_note_build.h"
#include "theory/tonality/common_practice_materials.h"

/*
 * The neutral input of the library chain (analysis_chain_run), built from imported notes: one AnalysisNote per
 * event, sorted (analysis_note_compare), and the file's key signature events as AnalysisKeyEvents (major keys, as
 * the library reads a signature).
 *
 * Borrowed from the import, which must outlive the input: events, note ids, spellings, voice ids, the meter map and
 * the key signature positions. Built here: each note's span, the notes and the key events. The spelling is the
 * score's only when source.spelling_notated; a MIDI file's notes carry none. track is the note's stream (the part).
 *
 * The chain names notes by id, the stages by ImportedScore index: order and chain_score_input_import_index map one to
 * the other.
 */
typedef struct ChainScoreInputSlot {
    const char *id;
    size_t index;
} ChainScoreInputSlot;

typedef struct {
    size_t *order;  /* sorted position -> ImportedScore index */
    ChainScoreInputSlot *by_id; /* (note id, import index) ordered by id, for chain_score_input_import_index */
    size_t count;                 /* notes */
    AnalysisNoteSet set;
    SpelledKeyEvent *key_events;
    size_t key_event_count;
} ChainScoreInput;

/* The key events are made with the major material of `materials` (the file's mode hint never reached the chain). False on allocation failure or a note the library cannot take (a zero-length span, a pitch outside 0 to 127);
   *out is then empty. A piece with no notes gives a valid empty input. */
bool chain_score_input_build(const ImportedScore *notes, const CommonPracticeMaterials *materials, ChainScoreInput *out);
void chain_score_input_destroy(ChainScoreInput *input);

/* The ImportedScore index of the note with this id, or (size_t)-1. */
size_t chain_score_input_import_index(const ChainScoreInput *input, const char *note_id);

#endif
