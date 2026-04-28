#ifndef URSATZ_ANALYZER_SIMILAR_MATERIAL_H
#define URSATZ_ANALYZER_SIMILAR_MATERIAL_H

#include <stdbool.h>
#include <stddef.h>

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

#endif
