#ifndef URSATZ_ANALYZER_SIMILAR_MATERIAL_H
#define URSATZ_ANALYZER_SIMILAR_MATERIAL_H

#include <stdbool.h>
#include <stddef.h>

#include "query/similarity_scoring.h"

/*
 * Similar material: passages of an analyzed corpus that look like a motive or a progression. A motive is a short
 * run of soprano notes and a progression a run of root-position Roman numerals. Like the style profile, it reads
 * only the analysis documents of the corpus, never the scores (R-079). A match says how much two passages agree on
 * the dimensions that were scored; it is not a claim that the passage is the motive.
 */

typedef enum {
    SIMILAR_OK = 0,
    SIMILAR_ERR_ARGUMENT,
    SIMILAR_ERR_QUERY,  /* a query with fewer than two notes or numerals, or a text that is not numerals */
    SIMILAR_ERR_IO,     /* a file could not be read or imported */
    SIMILAR_ERR_FORMAT, /* not an analysis document of schema version 9, or not a corpus index */
    SIMILAR_ERR_ALLOC
} SimilarResult;

/* One soprano note. onset is measured from the start of the passage it belongs to; a query's onset is not read. */
typedef struct {
    int pitch; /* MIDI note number */
    unsigned long onset_numerator, onset_denominator;       /* whole notes, in lowest terms */
    unsigned long duration_numerator, duration_denominator; /* whole notes, in lowest terms */
} SimilarNote;

typedef enum {
    SIMILAR_QUERY_MOTIVE = 0,
    SIMILAR_QUERY_PROGRESSION
} SimilarQueryKind;

typedef struct {
    SimilarQueryKind kind;
    SimilarNote *notes; /* a motive: at least 2 */
    size_t note_count;
    char **numerals;    /* a progression: at least 2, each as style_label_numeral prints it */
    size_t numeral_count;
} SimilarQuery;

/*
 * A motive from the highest line of the MIDI or MusicXML file at path: the file is analyzed and its soprano notes
 * (a note held across several chord segments is one note) are the query. On any result other than SIMILAR_OK
 * *query is untouched; SIMILAR_ERR_IO also writes a message to stderr.
 */
SimilarResult similar_query_from_file(const char *path, SimilarQuery *query);

/*
 * A progression from numerals separated by spaces or '-' ("I IV V7 I", "ii-V7-I"). Each token must be a Roman
 * numeral the Analyzer can print (style_label_numeral); an inversion figure is dropped, so "V6/5" reads "V7".
 */
SimilarResult similar_query_from_numerals(const char *text, SimilarQuery *query);

/* Frees what the query holds; safe on a query that was never filled. */
void similar_query_destroy(SimilarQuery *query);

/*
 * How a passage is scored. A motive is scored on its intervals (the semitone steps between soprano notes, so a
 * transposed motive scores the same), its contour and its rhythm (the durations as ratios, so an augmented rhythm
 * scores the same). A progression is scored on the semitone steps between chord roots and on rhythm; it has no
 * contour. A progression query carries no durations, so its chords are taken to be of equal length. A dimension
 * with weight 0 is not scored.
 */
typedef struct {
    unsigned long interval, contour, rhythm;
} SimilarWeights;

typedef struct {
    unsigned long minimum_numerator, minimum_denominator; /* a passage scoring less than this is not reported */
    size_t max_results;
    SimilarWeights weights;
} SimilarOptions;

/* Minimum 1/2, 10 results, weights interval 2, contour 1, rhythm 1. */
SimilarOptions similar_options_default(void);

typedef struct {
    char *piece;    /* the piece's path in the corpus, as the corpus index lists it */
    char *document; /* its analysis document, relative to the corpus output directory */
    int start_measure;
    double start_beat; /* the beat plus the fraction of it: 2.5 is half way through beat 2 */
    int end_measure;
    double end_beat;   /* where the last note or chord ends */
    SimilarityBreakdown score;
    size_t numeral_matches; /* progression: how many chords carry the numeral the query has at that place; else 0 */
    SimilarNote *notes;     /* the soprano notes of the passage, onsets from its start; NULL when it has none */
    size_t note_count;
} SimilarMatch;

typedef struct {
    SimilarMatch *matches; /* sorted by score, highest first, then piece path, then start; NULL when there are none */
    size_t count;
    size_t pieces_searched;
} SimilarResults;

/*
 * Searches the pieces "<corpus_output_dir>/corpus-index.json" lists as analyzed. Every window of the query's length
 * is scored; in each piece the best window of any run of overlapping ones is kept, and the highest-scoring
 * max_results over all pieces are returned. options may be NULL for the defaults. No match is not an error.
 */
SimilarResult similar_material_find(const char *corpus_output_dir, const SimilarQuery *query, const SimilarOptions *options,
                                    SimilarResults *results);

/* Frees the matches; safe on a result that was never filled. */
void similar_results_destroy(SimilarResults *results);

#endif
