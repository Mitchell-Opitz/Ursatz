#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ANALYSIS_ROMAN_NUMERAL_ADAPTER_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ANALYSIS_ROMAN_NUMERAL_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/harmony/chord_input.h"
#include "theory/tonality/roman_numeral_rules.h"
#include "theory/vocabulary/applied_chord_kind.h"

/*
 * Roman numeral Interpretations per chord segment, in the segment's local key (the standard-tonality
 * analysis adapter, step 2). The input is the StandardTonalityChordInput list of chord_input.h.
 *
 * READING. A chord is read from pitch classes alone: the root's distance above the key's tonic gives the
 * scale degree (the major scale; for minor, the natural minor scale, with the raised leading tone, a root
 * one semitone below the tonic, as degree 7, and melodic minor's raised sixth, a root nine semitones above it,
 * as degree 6); the root-relative interval set gives the quality (triads:
 * major {0,4,7} minor {0,3,7} diminished {0,3,6} augmented {0,4,8}; sevenths: major-major {0,4,7,11}
 * major-minor {0,4,7,10} minor-minor {0,3,7,10} half-diminished {0,3,6,10} fully diminished {0,3,6,9}).
 * The degree and quality must be ones the key gives (standard_tonality_roman_numeral_fits_key): in minor
 * that is the existing minor convention, V and vii-diminished from the raised seventh, a minor v from the
 * natural scale. A chord the key does not give is tried as an applied chord (V, V7, vii-diminished,
 * vii-diminished-7 of a target of the key) that identification already read (standard_tonality_read_applied_chord,
 * applied_chords.h); otherwise it has no numeral here (modal mixture, the Neapolitan and augmented sixths are out of scope).
 *
 * INVERSION comes from the bass, the lowest sounding chord tone (chord_input.h): a lower non-chord tone
 * does not make the chord root position. When no chord tone sounds, the previous segment's bass is kept
 * while it is a chord member, else the bass is unknown. The interval from the root to the bass pitch class picks the chord member
 * in the bass. A triad: root 0, third 1 (figure "6"), fifth 2 ("6/4"). A seventh chord: root 0 ("7" in the
 * numeral), third 1 ("6/5"), fifth 2 ("4/3"), seventh 3 ("4/2"). An unknown bass, or a bass that is not a
 * chord member, reads as root position.
 *
 * LABEL. The standard-tonality numeral (roman_numeral_rules.h: "V", "ii", "vii\xC2\xB0", "V7", "ii\xE2\x88\x85" "7")
 * and the figure; in a minor key the label (the claim's too) is spelled as RomanText spells it (R-031): V, v, VII, III, III+, iv,
 * IV, "viio7", "ii/o7" -- the circle is "o" and the slashed circle "/o"; an inverted seventh chord replaces its "7" by the figure: V6/5, V4/3, V4/2, "vii\xC2\xB0" "6".
 *
 * RESULT. One result per segment: a StandardTonalityRomanClaim (label, degree, quality, inversion, applied-to) and the
 * uncertainty
 *
 *     uncertainty = 1 - (1 - chord uncertainty)(1 - key uncertainty)
 *
 * kept as a double. A segment with no numeral has has_numeral false.
 */

#define STANDARD_TONALITY_ROMAN_LABEL_MAX 48

/* The structured fields of a numeral. */
typedef struct {
    char label[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    int degree;                 /* 1..7: the chord's root as a degree of the local key; for an applied chord, of
                                   the tonicized key (5 for V/x, 7 for vii-diminished/x) */
    bool seventh;               /* a seventh chord rather than a triad */
    int quality;                /* StandardTonalityTriadQuality, or StandardTonalitySeventhQuality when seventh */
    int inversion;              /* 0 root position, 1 first, 2 second, 3 third (sevenths only) */
    bool applied;               /* an applied chord (applied_chords.h) */
    int applied_kind;           /* AppliedChordKind; meaningful only when applied */
    int applied_target_degree;  /* 1..7: the degree of the local key tonicized; meaningful only when applied */
} StandardTonalityRomanClaim;

typedef struct {
    StandardTonalityRomanNumeral numeral;
    bool raised_leading_tone;   /* minor: the root is the raised leading tone (a vii-diminished) */
    bool raised_sixth;          /* minor: the root is the raised 6th of melodic minor (a vi-diminished) */
    bool romantext_spelling;    /* minor key: the label spells the circle "o" and the slashed circle "/o" (R-031) */
    int inversion;
    /* An applied chord (applied_chords.h): `numeral` is then the chord as it reads in the tonicized key (V, V7,
       vii-diminished, vii-diminished-7) and applied_target the numeral of the chord tonicized (ii, IV, V ...). */
    bool applied;
    AppliedChordKind applied_kind;
    StandardTonalityRomanNumeral applied_target;
} StandardTonalityRomanReading;

/*
 * Reads one input. False (out untouched) for a NULL argument, a segment with no numeral
 * (input->has_numeral false), or a chord the key does not give.
 */
bool standard_tonality_read_roman_numeral(const StandardTonalityChordInput *input,
                                          StandardTonalityRomanReading *out);

/* Writes the label (see LABEL; an applied chord adds "/" and the target's numeral) into buffer of `size` bytes. False for a bad reading or a buffer too small. */
bool standard_tonality_format_roman_reading(const StandardTonalityRomanReading *reading, char *buffer, size_t size);

typedef struct {
    const char *segment_id;     /* the input's, borrowed from the caller's segment */
    bool has_numeral;           /* false: no numeral, nothing below is meaningful */
    StandardTonalityRomanReading reading;
    StandardTonalityRomanClaim claim;
    double uncertainty;         /* [0, 1] */
    /* An applied chord that resolves names the segment it resolves to. An applied chord that does not resolve keeps its
       applied label and names none. */
    bool resolved;              /* applied and resolved */
    size_t resolution_index;    /* the resolution segment, an index into the inputs; meaningful when resolved */
    bool deceptive;             /* resolves to the target's submediant, not the target; meaningful when resolved */
} StandardTonalityRomanResult;


typedef enum {
    STANDARD_TONALITY_ROMAN_OK = 0,
    STANDARD_TONALITY_ROMAN_ERR_NULL_ARGUMENT,
    STANDARD_TONALITY_ROMAN_ERR_ALLOC
} StandardTonalityRomanStatus;

/* standard_tonality_build_roman_numerals_with_config, with the applied chord thresholds of a
   StandardTonalityAppliedConfig, is in applied_chords.h. */

void standard_tonality_roman_results_destroy(StandardTonalityRomanResult *results, size_t count);

#endif
