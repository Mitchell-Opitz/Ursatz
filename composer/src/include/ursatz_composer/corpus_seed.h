#ifndef URSATZ_COMPOSER_CORPUS_SEED_H
#define URSATZ_COMPOSER_CORPUS_SEED_H

#include <stddef.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Seeding from a corpus (docs/style.md): the material a piece starts from is taken from a passage of an analyzed
 * corpus (the output directory of `ursatz-analyzer --corpus`) that looks like a passage the user gives. The search is
 * the Analyzer's (similar_material.h) and reads only the analysis documents (R-079). A seed says where it came from
 * and how well it scored; a score is a count of agreement, not a claim that the passage is the one asked for.
 */

#define CORPUS_SEED_PIECE_MAX 512
#define CORPUS_SEED_MAX_NUMERALS 64
#define CORPUS_SEED_NUMERAL_MAX 24

typedef enum {
    CORPUS_SEED_OK = 0,
    CORPUS_SEED_ERR_ARGUMENT,
    CORPUS_SEED_ERR_QUERY,    /* the passage or the numerals could not be read as a query */
    CORPUS_SEED_ERR_CORPUS,   /* the corpus output directory could not be read */
    CORPUS_SEED_ERR_NO_MATCH, /* no passage scores at CORPUS_SEED_MINIMUM or more */
    CORPUS_SEED_ERR_ALLOC
} CorpusSeedResult;

/* A passage scoring less than 1/2 is not a match. */
#define CORPUS_SEED_MINIMUM_NUMERATOR 1UL
#define CORPUS_SEED_MINIMUM_DENOMINATOR 2UL

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
 * The melody of a passage of the corpus that looks like the one in the MIDI or MusicXML file at passage_path. Among
 * the matches (the best ten) with a score within 1/10 of the best, one is picked by the seeded generator (fill_rng.h),
 * so the same seed picks the same one. Its notes are taken from the passage start for one bar of the meter (a note
 * across the barline is shortened to it) and moved by one interval so the first note keeps its scale degree in the
 * requested key, the key of the source being the key its analysis heard at the passage. A match with fewer than two
 * notes in the bar is passed over for the next.
 *
 * out is untouched on any result but CORPUS_SEED_OK.
 */
CorpusSeedResult corpus_seed_motive(const char *corpus_output_dir, const char *passage_path, const PlanMeter *meter,
                                    const AnalysisKey *key, unsigned long seed, CorpusSeed *out);

/*
 * The chords of the passage of the corpus that best matches the numerals in text (separated by spaces or '-', as the
 * Analyzer reads them): the numerals the passage has, ready for plan_give_progression. The Analyzer writes a passage in
 * the notation of its own key, and the plan reads numerals in the mode of the key they are placed in, so the passage
 * is the best match (the best hundred are looked through) whose every chord the plan can place under a key of mode.
 * There is no seed: the best match is the first the Analyzer ranks. out->motive is not set.
 */
CorpusSeedResult corpus_seed_progression(const char *corpus_output_dir, const char *numerals_text, KeyStructureMode mode,
                                         CorpusSeed *out);

#endif
