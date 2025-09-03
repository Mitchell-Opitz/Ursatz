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
 *   measure repeats ("m5-6 = m1-2"), measure lines whose number carries a letter ("m7a") or a variant tag
 *   ("m11var1", an alternative reading of m11: skipped, so the main reading stands), and any token the
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
    bool target_upper;      /* the target is written in capitals (a major key) */
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

/*
 * Where a measure and a beat fall in the score. time_at gives the MusicalTime of beat `beat_thousandths` (1000 is
 * beat 1) of measure `measure`, or false when the score has no such place. The measure grid may be a MeterMap
 * (romantext_meter_map_grid) or the measure layout read from a score (score_layout.h), whose measures need not
 * be regular: a split measure, a pickup or a fermata break all sit where the score put them.
 */
typedef bool (*RomanTextTimeAt)(const void *context, int measure, long beat_thousandths, MusicalTime **out_time);

typedef struct {
    RomanTextTimeAt time_at;
    const void *context;
} RomanTextGrid;

/* The grid of a MeterMap: its measures, laid by its time signatures. */
RomanTextGrid romantext_meter_map_grid(const MeterMap *map);

/* The offset from the start of a measure of `beat_thousandths` in a signature top/bottom, as a fraction of a
   whole note (the beat is the bottom note, or a dotted note in a compound signature). False for a beat below 1. */
bool romantext_beat_offset(unsigned top, unsigned bottom, long beat_thousandths, long *numerator,
                           unsigned long *denominator);

/* Parses text (NUL-terminated) on the score's grid. On OK, *out owns its arrays (romantext_destroy);
   otherwise it holds only error_message. */
RomanTextStatus romantext_parse(const char *text, const RomanTextGrid *grid, RomanTextAnalysis *out);

/* As romantext_parse, reading the file at path. */
RomanTextStatus romantext_load(const char *path, const RomanTextGrid *grid, RomanTextAnalysis *out);

void romantext_destroy(RomanTextAnalysis *analysis);

/*
 * The chord a supported annotation names, in the key in force at it: its root pitch class and its root-relative
 * interval set (3 or 4 semitone values, ascending), the count returned. The scale is the major scale, or the natural
 * minor; a lowercase vii in minor is the leading tone chord; a capital seventh chord not on the dominant takes the
 * seventh its key's scale gives (a major seventh above I, IV, and in minor III and VI); V and V7/x are dominant
 * sevenths; "o" is diminished (fully diminished with a 7), "%" half-diminished, "+" augmented; an applied chord is
 * read in the key of its target (V/x is the dominant of x, vii/x its leading tone chord).
 */
size_t romantext_expected_chord(const RomanTextChord *chord, int *root, int intervals[4]);

/* The metadata of text alone, with no MeterMap: composer, title and time signature. For building the MeterMap. */
RomanTextStatus romantext_parse_metadata(const char *text, RomanTextAnalysis *out);

#endif
