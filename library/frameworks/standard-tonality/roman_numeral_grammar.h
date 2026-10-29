#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_GRAMMAR_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_GRAMMAR_H

#include <stdbool.h>
#include <stddef.h>

#include "roman_numeral_rules.h"
#include "tonal_key.h"

/*
 * One public Roman numeral grammar for standard-tonality-v1: every numeral
 * form the repos write, parsed into one value and formatted back to one
 * canonical spelling. The grammar is case-sensitive:
 *
 *   text      = [alteration] numeral [mark] [figure] ["/" target]
 *   alteration  "b" or "#" (also the UTF-8 flat and sharp signs)
 *   numeral     I II III IV V VI VII (third above the root is major) or
 *               i ii iii iv v vi vii (minor third); mixed case is rejected
 *   mark        "o" or the degree sign: diminished (lowercase only)
 *               "+": augmented (uppercase, a triad only)
 *               the o-slash, the empty set sign, "/o" or "%": half-
 *               diminished (lowercase, a seventh figure required)
 *               "M" or "maj": major seventh (uppercase, a seventh figure
 *               required: IM7, IVmaj7)
 *   figure      triads: 6 (first inversion), 64 (second)
 *               sevenths: 7 (root position), 65, 43, 42 (inversions 1 to 3);
 *               2 is also 42
 *               6/4, 6/5, 4/3 and 4/2 are accepted as 64, 65, 43 and 42
 *   target      the numeral the chord is applied to, with its own optional
 *               alteration and no mark or figure: V7/V, viio7/ii, V/bVI.
 *               Lowercase is a minor target
 *
 * Quality is set by case, mark and figure: an uppercase numeral with a
 * seventh figure and no mark is a dominant seventh (V7, and I7 as text),
 * a lowercase one a minor seventh (ii7); "o" with a seventh figure is
 * fully diminished (viio7). Rejected: a mark that contradicts the case
 * (Vo, vi+), a seventh figure on "+" (V+7), a half-diminished or major-
 * seventh mark without a seventh figure (viiø, IM), trailing text and empty
 * input.
 *
 * Canonical form (what format writes, and parse then format is stable):
 * ASCII marks "o", "+", the o-slash (UTF-8 U+00F8) and "M"; figures 6, 64,
 * 7, 65, 43, 42; the alteration "b" or "#".
 */

typedef enum {
    STANDARD_TONALITY_NUMERAL_MAJOR = 0,
    STANDARD_TONALITY_NUMERAL_MINOR,
    STANDARD_TONALITY_NUMERAL_DIMINISHED,
    STANDARD_TONALITY_NUMERAL_AUGMENTED,
    STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH,
    STANDARD_TONALITY_NUMERAL_MAJOR_SEVENTH,
    STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH,
    STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH,
    STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH,
    STANDARD_TONALITY_NUMERAL_QUALITY_COUNT
} StandardTonalityNumeralQuality;

typedef struct {
    int alteration;                         /* -1, 0, +1 semitone on the root */
    int degree;                             /* 1..7 */
    StandardTonalityNumeralQuality quality;
    int inversion;                          /* 0..3: 0..2 for a triad */
    bool has_target;                        /* an applied numeral: "/" and a target */
    int target_alteration;                  /* -1, 0, +1 */
    int target_degree;                      /* 1..7 */
    bool target_minor;                      /* the target numeral was lowercase */
} StandardTonalityNumeralText;

/* Spelling of the marks and figures a formatted numeral uses. */
typedef enum {
    STANDARD_TONALITY_NUMERAL_STYLE_CANONICAL = 0, /* viio65, viiø65 */
    STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY,       /* vii° and ii∅ (UTF-8), figures 6/5 */
    STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT      /* viio and ii/o, figures 6/5 */
} StandardTonalityNumeralStyle;

/* True for the seven-note qualities (dominant, major, minor, half-diminished, fully diminished sevenths). */
bool standard_tonality_numeral_quality_is_seventh(StandardTonalityNumeralQuality quality);

/* Reads one numeral. False (output untouched) for NULL, a form outside the grammar above, or trailing text. */
bool standard_tonality_numeral_parse(const char *text, StandardTonalityNumeralText *out);

/* Writes the canonical form (NUL-terminated). False for an invalid value, a NULL argument, or a buffer too
   small; the buffer is untouched on failure. */
bool standard_tonality_numeral_format(const StandardTonalityNumeralText *numeral, char *buffer, size_t size);

/* As format, in the given style. */
bool standard_tonality_numeral_format_styled(const StandardTonalityNumeralText *numeral,
                                             StandardTonalityNumeralStyle style, char *buffer, size_t size);

/*
 * The root's semitones above the tonic (0..11) in the mode. An applied numeral is resolved through its target:
 * the target's root in the mode, then the numeral's degree in the target's own scale (major for an uppercase
 * target, harmonic minor for a lowercase one). The alteration is added last. False (output untouched) for an
 * invalid value, an invalid mode, or NULL arguments.
 */
bool standard_tonality_numeral_root_semitone(const StandardTonalityNumeralText *numeral, StandardTonalityMode mode,
                                             int *semitones);

/*
 * The plain triad or seventh numeral (inversion, alteration and target dropped). A dominant seventh text on
 * degree 5 or 7 is the major-minor seventh and on any other degree the major-major seventh, as the existing
 * entry points read an uppercase numeral with a 7. False (output untouched) for an invalid value or NULL.
 */
bool standard_tonality_numeral_base(const StandardTonalityNumeralText *numeral, StandardTonalityRomanNumeral *out);

/*
 * The text of a plain numeral: root position, no alteration, no target. The major-major and major-minor sevenths
 * both become the dominant seventh text, since "7" after an uppercase numeral names either (see
 * standard_tonality_roman_numeral_inferred_seventh). False (output untouched) for an invalid numeral or NULL.
 */
bool standard_tonality_numeral_from_base(const StandardTonalityRomanNumeral *numeral,
                                         StandardTonalityNumeralText *out);

#endif
