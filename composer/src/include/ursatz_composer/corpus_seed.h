#ifndef URSATZ_COMPOSER_CORPUS_SEED_H
#define URSATZ_COMPOSER_CORPUS_SEED_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <model/core/rational.h>
#include <theory/harmony/harmony_chord.h>
#include <theory/knowledge/knowledge_snapshot.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Seeding from a corpus (docs/style.md): the material a piece starts from is taken from a passage of an analyzed
 * corpus (the output directory of `ursatz-analyzer --corpus`) that looks like a passage the user gives. The Composer does not
 * search: the request gives the passages inline (`corpus_passages`, R-619), found by the Analyzer (analyzer_corpus_passages_like,
 * analyzer_corpus_passages_numerals), ranked best first. A seed says where it came from and how well it scored; a score is a
 * count of agreement, not a claim that the passage is the one asked for.
 */

#define CORPUS_SEED_PIECE_MAX 512
#define CORPUS_SEED_MAX_NUMERALS 64
#define CORPUS_SEED_NUMERAL_MAX 24

typedef enum {
    CORPUS_SEED_OK = 0,
    CORPUS_SEED_ERR_ARGUMENT,
    CORPUS_SEED_ERR_NO_MATCH, /* no passage is given, or none can be placed */
    CORPUS_SEED_ERR_ALLOC
} CorpusSeedResult;

#define CORPUS_PASSAGE_KEY_MAX 24

/* One soprano note of a passage: a MIDI note number, onset from the passage's start and duration, in whole notes. */
typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} CorpusPassageNote;

/* One chord of the passage's piece: the root-position numeral in the Analyzer's words, where it starts (a measure and the beat
   plus the fraction of it) and the key label of its entry ("C major"; "" when it has none). */
typedef struct {
    char numeral[CORPUS_SEED_NUMERAL_MAX];
    int measure;
    double beat;
    char key_label[CORPUS_PASSAGE_KEY_MAX];
} CorpusPassageChord;

/* One passage the request gives (the `corpus_passages` member, schema/api/corpus-passages.schema.json). notes and chords are owned. */
typedef struct {
    char piece[CORPUS_SEED_PIECE_MAX];
    int start_measure;
    double start_beat;
    int end_measure;
    double end_beat;
    Rational similarity; /* in [0, 1], 1 is identical */
    CorpusPassageNote *notes;
    size_t note_count;
    CorpusPassageChord *chords;
    size_t chord_count;
    bool stale; /* the document it was read from was analyzed by another version; it is used all the same */
} CorpusPassage;

typedef struct {
    CorpusPassage *items;
    size_t count;
} CorpusPassages;

/*
 * Reads a `corpus_passages` array. On false *out is empty and path (a JSON pointer such as "/corpus_passages/2/notes/0") and
 * message say what is wrong; either may be NULL. An empty array is a list with no passages.
 */
bool corpus_passages_from_json(const cJSON *array, CorpusPassages *out, char *path, size_t path_size, char *message,
                               size_t message_size);

/* Frees the passages and clears *passages. Safe on a value that was never filled. */
void corpus_passages_destroy(CorpusPassages *passages);

typedef struct {
    char piece[CORPUS_SEED_PIECE_MAX]; /* the piece's path in the corpus, as the corpus index lists it */
    int start_measure;
    double start_beat; /* the beat plus the fraction of it */
    int end_measure;
    double end_beat;
    unsigned long score_numerator, score_denominator; /* the Analyzer's similarity, 1 is identical */
    /* A motive seed: one bar of the matched melody, in the requested key (key_source is MOTIVE_SKETCH_KEY_GIVEN). */
    Motive motive;
    /* A progression seed: the matched passage's chords, as the Analyzer prints numerals. */
    char numerals[CORPUS_SEED_MAX_NUMERALS][CORPUS_SEED_NUMERAL_MAX];
    size_t numeral_count;
    /* How many of the documents read for this seed are not current (analyzed by another version); they are read all the same. */
    size_t stale_documents;
} CorpusSeed;

const char *corpus_seed_result_name(CorpusSeedResult result);

/*
 * A motive from a passage of the corpus: the passages are ranked best first. Among them (all that score within 1/10 of the best
 * one's similarity) one is picked by the seeded generator (fill_rng.h), so the same seed picks the same one. Its notes are taken
 * from the passage start for one bar of the meter (a note across the barline is shortened to it) and moved by one interval so
 * the first note keeps its scale degree in the requested key, the key of the source being the key its analysis heard at the
 * passage (the key label of the last chord that starts at or before it). A passage with fewer than two notes in the bar is
 * passed over for the next.
 *
 * knowledge reads the key labels (the call context's); CORPUS_SEED_ERR_ARGUMENT for NULL. CORPUS_SEED_ERR_NO_MATCH with
 * no passage or none that gives a motive. out is untouched on any result but CORPUS_SEED_OK.
 */
CorpusSeedResult corpus_seed_motive(const KnowledgeSnapshot *knowledge, const CorpusPassage *passages, size_t count,
                                    const PlanMeter *meter, const SpelledKey *key, unsigned long seed, CorpusSeed *out);

/*
 * The chords of the first passage, best first, whose `wanted` chords from the one that starts where the passage does the plan
 * can place under a key of mode: the numerals, ready for plan_give_progression (up to CORPUS_SEED_MAX_NUMERALS of them, each
 * shorter than CORPUS_SEED_NUMERAL_MAX). The Analyzer writes a passage in the notation of its own key, and the plan reads
 * numerals in the mode of the key they are placed in. There is no seed: the first passage that works is used. out->motive
 * is not set. CORPUS_SEED_ERR_ARGUMENT for a NULL argument or a wanted outside 1 to CORPUS_SEED_MAX_NUMERALS.
 */
CorpusSeedResult corpus_seed_progression(const KnowledgeSnapshot *knowledge, const CorpusPassage *passages, size_t count,
                                         size_t wanted, HarmonyMode mode, CorpusSeed *out);

#endif
