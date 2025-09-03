#ifndef URSATZ_TESTS_ACCURACY_CHORALE_CHAIN_H
#define URSATZ_TESTS_ACCURACY_CHORALE_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "romantext.h"
#include "score_layout.h"

/*
 * The full chain over one annotated chorale (test infrastructure): MusicXML import, the measure layout, adaptive
 * segments on the score's own barlines, weights, local key regions, weighted chord identification in each region's
 * key, and the standard-tonality Roman numeral adapter; then the comparison with the RomanText analysis. Every
 * default is the library's. Nothing is tuned to the corpus.
 *
 * Wiring the library does not fix, chosen here and written down in docs/testing/accuracy.md:
 *   - Chord identification reads each key region's segments in that region's key. The weighted identifier has
 *     only a major-key vocabulary, so a minor region is read in its RELATIVE MAJOR's vocabulary (the nearest the
 *     chain offers); a minor dominant (V) is then not a candidate, and the metrics show it.
 *   - The score's own key signature events are passed to the key regions (the importer reads them; the key
 *     regions use them as a restriction on the first region).
 *   - The numeral is read twice: in the annotation's local key (the "numeral" metric, which isolates the chord
 *     reading from the key estimate) and in the chain's own local key ("numeral_chain_key").
 *
 * Time-weighted metrics: elementary intervals are cut at every segment boundary and every annotated chord or key
 * change; each interval counts for its duration in whole notes. Annotated chords the loader marks unsupported are
 * left out of the chord denominators and counted in unsupported_time.
 */

typedef struct {
    double annotated_time;     /* whole notes from the first annotated chord to the end of the score */
    double chord_time;         /* of that, under a supported annotated chord (the chord denominators) */
    double unsupported_time;   /* under an unsupported annotated chord */
    double root_right;
    double quality_right;
    double numeral_right;      /* in the annotation's local key */
    double numeral_chain_key_right; /* in the chain's own local key */
    double key_right;          /* over annotated_time */
    size_t segments;
    size_t annotated_chords;
    size_t unsupported_chords;
    size_t key_regions;        /* the chain's */
} ChoraleScore;

typedef enum {
    CHORALE_CHAIN_OK = 0,
    CHORALE_CHAIN_ERR_IO,
    CHORALE_CHAIN_ERR_IMPORT,
    CHORALE_CHAIN_ERR_LAYOUT,
    CHORALE_CHAIN_ERR_ANNOTATION,
    CHORALE_CHAIN_ERR_CHAIN
} ChoraleChainStatus;

/* Runs one piece from its directory (score.musicxml and analysis.rntxt). On a failure, message says where. */
ChoraleChainStatus chorale_chain_run(const char *piece_dir, ChoraleScore *out, char *message, size_t message_size);

#endif
