#ifndef URSATZ_ANALYZER_CORPUS_PASSAGES_H
#define URSATZ_ANALYZER_CORPUS_PASSAGES_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * Passages of an analyzed corpus, ready to give to the Composer (R-619). The Composer API seeds a motive or a progression from
 * passages given inline in a request (`corpus_passages`); this is how an orchestrator gets them: the search is the Analyzer's
 * (similar_material.h, the same ranking), and each match's document is read once, for the chords and for whether it is current.
 * Only the analysis documents of the corpus are read, never the scores (R-079).
 */

typedef enum {
    ANALYZER_PASSAGES_OK = 0,
    ANALYZER_PASSAGES_ERR_ARGUMENT,
    ANALYZER_PASSAGES_ERR_QUERY,  /* the passage or the numerals cannot be read as a query */
    ANALYZER_PASSAGES_ERR_CORPUS, /* the corpus output directory or one of its documents cannot be read */
    ANALYZER_PASSAGES_ERR_ALLOC
} AnalyzerPassagesStatus;

/* One soprano note of a passage: a MIDI note number, the onset from the passage's start and the duration, in whole notes. */
typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} AnalyzerPassageNote;

/* One chord of the matched piece, as its analysis document joins them (analyzer_chord_sequence_from_document): the
   root-position numeral in the Analyzer's words, where it starts (the beat plus the fraction of it) and the key label of the
   entry it starts at ("C major", "" when the entry has none). */
typedef struct {
    char *numeral;
    int measure;
    double beat;
    char *key_label;
} AnalyzerPassageChord;

/*
 * One match. piece is the piece's path in the corpus as the corpus index lists it. The place is where the passage starts and ends
 * (a measure and a beat, the beat plus the fraction of it: 2.5 is half way through beat 2). similarity is the Analyzer's score in
 * [0, 1], 1 is identical. notes are the passage's soprano notes (NULL with note_count 0 when it has none or one does not fit a
 * Rational); chords are every chord of the piece in time order (NULL with chord_count 0 when none), so a reader finds the
 * chords from the one that starts where the passage does. stale is true when the document was analyzed by another version
 * (staleness.h); it is read all the same.
 */
typedef struct {
    char *piece;
    int start_measure;
    double start_beat;
    int end_measure;
    double end_beat;
    Rational similarity;
    AnalyzerPassageNote *notes;
    size_t note_count;
    AnalyzerPassageChord *chords;
    size_t chord_count;
    bool stale;
} AnalyzerCorpusPassage;

/* Matches, best first, then by piece path and start (similar_material_find's order). */
typedef struct {
    AnalyzerCorpusPassage *passages;
    size_t count;
} AnalyzerCorpusPassages;

/*
 * The passages of the corpus (the output directory of `ursatz-analyzer --corpus`) that look like the melody of the MIDI or
 * MusicXML file at passage_path: the best `max` with a similarity of 1/2 or more. No match is ANALYZER_PASSAGES_OK with
 * count 0. `knowledge` reads the passage and the documents (ANALYZER_PASSAGES_ERR_ARGUMENT without it). out is filled only on
 * ANALYZER_PASSAGES_OK and released with analyzer_corpus_passages_destroy; on any other status it is empty.
 */
AnalyzerPassagesStatus analyzer_corpus_passages_like(const char *corpus_dir, const KnowledgeSnapshot *knowledge,
                                                     const char *passage_path, size_t max, AnalyzerCorpusPassages *out);

/*
 * The passages that look like the progression `numerals` (Roman numerals separated by spaces or '-', as
 * similar_query_from_numerals reads them): the best `max` with a similarity of 1/2 or more. Otherwise as
 * analyzer_corpus_passages_like.
 */
AnalyzerPassagesStatus analyzer_corpus_passages_numerals(const char *corpus_dir, const KnowledgeSnapshot *knowledge,
                                                         const char *numerals, size_t max, AnalyzerCorpusPassages *out);

/* Frees the passages and clears *passages. Safe on a value that was never filled. */
void analyzer_corpus_passages_destroy(AnalyzerCorpusPassages *passages);

/*
 * The passages as the Composer API's `corpus_passages` request member (schema composer/schema/api/corpus-passages.schema.json):
 * a JSON array text, each passage {"piece", "start": {"measure", "beat"}, "end": {"measure", "beat"}, "similarity": {"num",
 * "den"}, "stale", "notes": [{"midi", "onset": {"num", "den"}, "duration": {"num", "den"}}], "chords": [{"numeral", "measure",
 * "beat", "key"}]}. A heap string the caller frees; NULL on allocation failure or a NULL argument. An empty list is "[]".
 */
char *analyzer_corpus_passages_to_json(const AnalyzerCorpusPassages *passages);

#endif
