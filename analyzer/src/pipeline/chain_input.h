#ifndef URSATZ_ANALYZER_CHAIN_INPUT_H
#define URSATZ_ANALYZER_CHAIN_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/midi_pipeline.h>

#include "semantics/analysis_key.h"
#include "semantics/analysis_note.h"

/*
 * The neutral input of the library chain (analysis_chain_run), built from imported notes: one AnalysisNote per
 * event, sorted (analysis_note_compare), and the file's key signature events as AnalysisKeyEvents (major keys, as
 * the library reads a signature).
 *
 * Borrowed from the import, which must outlive the input: events, note ids, spellings, voice ids, the meter map and
 * the key signature positions. Built here: each note's span, the notes and the key events. The spelling is the
 * score's only when source.spelling_notated; a MIDI file's notes carry none. track is the note's stream (the part).
 *
 * The chain names notes by id, the stages by ImportedNotes index: order and chain_input_import_index map one to
 * the other.
 */
typedef struct ChainInputSlot {
    const char *id;
    size_t index;
} ChainInputSlot;

typedef struct {
    AnalysisNote *notes;
    size_t *order;  /* sorted position -> ImportedNotes index */
    ChainInputSlot *by_id; /* (note id, import index) ordered by id, for chain_input_import_index */
    size_t count;                 /* notes */
    AnalysisNoteSet set;
    AnalysisKeyEvent *key_events;
    size_t key_event_count;
} ChainInput;

/* False on allocation failure or a note the library cannot take (a zero-length span, a pitch outside 0 to 127);
   *out is then empty. A piece with no notes gives a valid empty input. */
bool chain_input_build(const ImportedNotes *notes, ChainInput *out);
void chain_input_destroy(ChainInput *input);

/* The ImportedNotes index of the note with this id, or (size_t)-1. */
size_t chain_input_import_index(const ChainInput *input, const char *note_id);

#endif
