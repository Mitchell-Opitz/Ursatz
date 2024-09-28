#ifndef URSATZ_TESTS_ACCURACY_ROMANTEXT_H
#define URSATZ_TESTS_ACCURACY_ROMANTEXT_H

#include <stdbool.h>
#include <stddef.h>

#include "time/meter_map.h"
#include "time/musical_time.h"

/*
 * RomanText loader (test infrastructure, not shipped library code). RomanText (.rntxt) is the Roman numeral
 * analysis format of the "When in Rome" meta-corpus. This loader reads the subset below into a timeline of
 * key changes and chords in MusicalTime, laid on the MeterMap of the piece's score.
 *
 * SUPPORTED
 *   Metadata lines  "Composer:", "Title:", "Time Signature:" (others are ignored; a second, different "Time
 *                   Signature:" after the first measure is counted as unsupported).
 *   Measure lines   "m<N> <token> ...". N may be 0 (a pickup). A "b<beat>" token sets the beat for what follows
 *                   (default beat 1); a beat may be fractional ("b2.5", up to three decimals). The beat unit is
 *                   the denominator note of the signature (a quarter in 3/4, a half in 2/2), or the dotted
 *                   beat of a compound signature (6/8, 9/8, 12/8: three eighths).
 *   Key tokens      "G:", "a:", "Bb:", "f#:" (uppercase major, lowercase minor; flats written "b" or "-").
 *   Chord tokens    degree I..VII (case gives major or minor), an optional leading "b", "-" or "#" for a
 *                   lowered or raised degree, a quality mark ("o" diminished, "%" half-diminished, "+" augmented),
 *                   figures (6, 64, 7, 65, 43, 42, 2, and the slashed 6/4, 6/5, 4/3, 4/2), and one applied target
 *                   ("V7/V", "viio6/ii", "V65/bVI").
 *   Ignored         barline and repeat marks ("||", ":||", "||:", "|"), "var..." and "Phrase..." tags, and lines
 *                   that start with "Note:", "Phrase" or "Pedal" (those carry no chord).
 *
 * UNSUPPORTED (kept in the timeline as written, flagged, and counted; never an error)
 *   Ger65, Fr43, It6 and other augmented-sixth chords, the Neapolitan "N", "Cad64", bracketed additions or
 *   omissions ("[no5]", "[add4]"), figures beyond sevenths (9, 11, 13), nested applied targets ("V/V/V"),
 *   measure repeats ("m5-6 = m1-2"), measure lines whose number carries a letter ("m7a"), and any token the
 *   grammar above does not read.
 */

#define ROMANTEXT_TEXT_MAX 24
#define ROMANTEXT_META_MAX 128

typedef struct {
    MusicalTime *time;      /* owned: where the key holds from */
    int measure;
    int tonic_pitch_class;  /* 0..11 */
    bool minor;
} RomanTextKey;

typedef struct {
    MusicalTime *time;      /* owned: where the chord starts; it sounds until the next chord's start */
    int measure;
    char text[ROMANTEXT_TEXT_MAX]; /* the token as written */
    bool supported;         /* false: the fields below are not meaningful */
    int degree;             /* 1..7 */
    int alteration;         /* -1 lowered, 0, +1 raised degree */
    bool upper;             /* written in capitals (major) */
    char marker;            /* 0, 'o', '%' or '+' */
    bool seventh;
    int inversion;          /* 0..3 (3 for sevenths only) */
    bool applied;
    int target_degree;      /* 1..7; meaningful when applied */
    int target_alteration;
    int tonic_pitch_class;  /* the key in force at the chord */
    bool minor;
    size_t key_index;       /* an index into keys, or SIZE_MAX when no key was given before the chord */
} RomanTextChord;

typedef struct {
    char composer[ROMANTEXT_META_MAX];
    char title[ROMANTEXT_META_MAX];
    int time_signature_top;     /* 0 when not given */
    int time_signature_bottom;
    RomanTextKey *keys;         /* owned; in time order */
    size_t key_count;
    RomanTextChord *chords;     /* owned; in time order */
    size_t chord_count;
    size_t unsupported_count;   /* chords not supported, plus other unsupported tokens and lines */
    size_t unsupported_chords;
    int last_measure;           /* the last measure with a token */
    char error_message[160];
} RomanTextAnalysis;

typedef enum {
    ROMANTEXT_OK = 0,
    ROMANTEXT_ERR_NULL_ARGUMENT,
    ROMANTEXT_ERR_IO,
    ROMANTEXT_ERR_BAD_LINE,       /* a measure line or key token that cannot be read; the message names the line */
    ROMANTEXT_ERR_BAD_POSITION,   /* a measure or beat outside the MeterMap's measures; the message names the line */
    ROMANTEXT_ERR_ALLOC
} RomanTextStatus;

/* Parses text (NUL-terminated) against the score's MeterMap. On OK, *out owns its arrays (romantext_destroy);
   otherwise it holds only error_message. */
RomanTextStatus romantext_parse(const char *text, const MeterMap *map, RomanTextAnalysis *out);

/* As romantext_parse, reading the file at path. */
RomanTextStatus romantext_load(const char *path, const MeterMap *map, RomanTextAnalysis *out);

void romantext_destroy(RomanTextAnalysis *analysis);

/* The metadata of text alone, with no MeterMap: composer, title and time signature. For building the MeterMap. */
RomanTextStatus romantext_parse_metadata(const char *text, RomanTextAnalysis *out);

#endif
