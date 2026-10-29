#include "roman_numeral_analysis_rules.h"

#include <stddef.h>
#include <string.h>

#include "chord_symbol_rules.h"
#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "rule_pair.h"
#include "seventh_identification_rules.h"
#include "interval_quality_rules.h"
#include "seventh_in_key_rules.h"
#include "tonal_pitch.h"
#include "triad_identification_rules.h"
#include "triad_in_key_rules.h"
#include "pitch/pitch_arithmetic.h"

static const char *const NAMES[STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT] = {
    "roman-numeral-analysis-key",
    "roman-numeral-analysis-root",
    "roman-numeral-analysis-quality",
    "roman-numeral-analysis-raised-leading-tone",
    "roman-numeral-analysis-dominant-seventh",
    "roman-numeral-analysis-invariance"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT];

static bool valid_rule(StandardTonalityRomanNumeralAnalysisRule which) {
    return which >= 0 && which < STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT;
}

static void ensure_built(StandardTonalityRomanNumeralAnalysisRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_roman_numeral_analysis_rule(StandardTonalityRomanNumeralAnalysisRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_roman_numeral_analysis_constraint(StandardTonalityRomanNumeralAnalysisRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static bool valid_mode(StandardTonalityRomanKeyMode mode) {
    return mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR || mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
}

bool standard_tonality_roman_key_label(PitchLetter letter, int accidental, StandardTonalityRomanKeyMode mode,
                                       char *buffer, size_t size) {
    if (!valid_mode(mode)) {
        return false;
    }
    /* A key label is a chord symbol with only the root and the minor mark. */
    return standard_tonality_chord_symbol(letter, accidental,
                                          mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? STANDARD_TONALITY_TRIAD_MAJOR
                                                                                    : STANDARD_TONALITY_TRIAD_MINOR,
                                          false, PITCH_LETTER_C, 0, buffer, size);
}

bool standard_tonality_parse_roman_key_label(const char *label, PitchLetter *letter, int *accidental,
                                             StandardTonalityRomanKeyMode *mode) {
    if (label == NULL || letter == NULL || accidental == NULL || mode == NULL || label[0] == '\0') {
        return false;
    }

    /* A lowercase letter is a minor key: read it as the capital and keep
       the lowercase for the mode. */
    char text[32];
    size_t length = strlen(label);
    if (length >= sizeof(text)) {
        return false;
    }
    memcpy(text, label, length + 1);
    bool lowercase = text[0] >= 'a' && text[0] <= 'g';
    if (lowercase) {
        text[0] = (char)(text[0] - 'a' + 'A');
    }

    PitchLetter parsed_letter;
    int parsed_accidental;
    StandardTonalityTriadQuality quality;
    bool has_bass;
    PitchLetter bass_letter;
    int bass_accidental;
    if (!standard_tonality_parse_chord_symbol(text, &parsed_letter, &parsed_accidental, &quality, &has_bass,
                                              &bass_letter, &bass_accidental) ||
        has_bass) {
        return false;
    }

    StandardTonalityRomanKeyMode parsed_mode;
    if (lowercase) {
        if (quality != STANDARD_TONALITY_TRIAD_MAJOR) {
            return false; /* "mi" after a lowercase letter, or another mark */
        }
        parsed_mode = STANDARD_TONALITY_ROMAN_KEY_MINOR;
    } else if (quality == STANDARD_TONALITY_TRIAD_MAJOR) {
        parsed_mode = STANDARD_TONALITY_ROMAN_KEY_MAJOR;
    } else if (quality == STANDARD_TONALITY_TRIAD_MINOR) {
        parsed_mode = STANDARD_TONALITY_ROMAN_KEY_MINOR;
    } else {
        return false;
    }

    *letter = parsed_letter;
    *accidental = parsed_accidental;
    *mode = parsed_mode;
    return true;
}

bool standard_tonality_roman_dominant_seventh_degree(StandardTonalityRomanKeyMode mode, int degree) {
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR) {
        return degree == 5;
    }
    return mode == STANDARD_TONALITY_ROMAN_KEY_MINOR && (degree == 5 || degree == 7);
}

bool standard_tonality_roman_numeral_implies_raised_leading_tone(StandardTonalityRomanKeyMode mode,
                                                                 const StandardTonalityRomanNumeral *numeral) {
    if (mode != STANDARD_TONALITY_ROMAN_KEY_MINOR || numeral == NULL) {
        return false;
    }
    if (numeral->seventh) {
        return (numeral->degree == 5 && numeral->seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR) ||
               (numeral->degree == 7 && numeral->seventh_quality == STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED);
    }
    return (numeral->degree == 5 && numeral->triad == STANDARD_TONALITY_TRIAD_MAJOR) ||
           (numeral->degree == 7 && numeral->triad == STANDARD_TONALITY_TRIAD_DIMINISHED) ||
           (numeral->degree == 3 && numeral->triad == STANDARD_TONALITY_TRIAD_AUGMENTED);
}

typedef struct {
    PitchLetter letter;
    int accidental;
} NoteName;

/* The distinct note names (letter and accidental, octaves aside) of the
   notes; false when there are more than three. */
static bool collect_three_names(const PitchSpelling *const *notes, size_t count, NoteName names[3], int *found) {
    int n = 0;
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
        PitchLetter letter = pitch_spelling_letter(notes[i]);
        int accidental = pitch_spelling_accidental(notes[i]);
        bool seen = false;
        for (int j = 0; j < n; j++) {
            if (names[j].letter == letter && names[j].accidental == accidental) {
                seen = true;
            }
        }
        if (seen) {
            continue;
        }
        if (n == 3) {
            return false;
        }
        names[n].letter = letter;
        names[n].accidental = accidental;
        n++;
    }
    *found = n;
    return true;
}

/*
 * INVARIANCE: a seventh chord may omit its fifth (the source's V7 with the
 * fifth missing). Three note names that are a root, the letter a third
 * above, and the letter a seventh above, with no fifth, are read from the
 * third and the seventh alone: major third with major seventh is major-
 * major, with minor seventh major-minor; minor third with minor seventh is
 * minor-minor or, the fifth being unsaid, half-diminished (the key says
 * which); minor third with diminished seventh is fully diminished. The
 * qualities are returned in *first and *second (second equal to first when
 * only one reading exists).
 */
static bool identify_without_fifth(const PitchSpelling *const *notes, size_t count, PitchLetter *root_letter,
                                   int *root_accidental, StandardTonalitySeventhQuality *first,
                                   StandardTonalitySeventhQuality *second) {
    NoteName names[3];
    int found;
    if (notes == NULL || count < 3 || !collect_three_names(notes, count, names, &found) || found != 3) {
        return false;
    }

    for (int r = 0; r < 3; r++) {
        int third = -1;
        int seventh = -1;
        for (int j = 0; j < 3; j++) {
            if (j == r) {
                continue;
            }
            int step = (pitch_letter_index(names[j].letter) - pitch_letter_index(names[r].letter) + 7) % 7;
            if (step == 2) {
                third = j;
            } else if (step == 6) {
                seventh = j;
            }
        }
        if (third < 0 || seventh < 0) {
            continue;
        }

        /* Measure the intervals from the root up, in closed position. */
        PitchSpelling *root = NULL;
        PitchSpelling *third_note = NULL;
        PitchSpelling *seventh_note = NULL;
        int root_index = pitch_letter_index(names[r].letter);
        bool ok = pitch_spelling_create(names[r].letter, names[r].accidental, 4, &root) == PITCH_SPELLING_OK &&
                  pitch_spelling_create(names[third].letter, names[third].accidental, 4 + (root_index + 2) / 7,
                                        &third_note) == PITCH_SPELLING_OK &&
                  pitch_spelling_create(names[seventh].letter, names[seventh].accidental, 4 + (root_index + 6) / 7,
                                        &seventh_note) == PITCH_SPELLING_OK;
        int size;
        StandardTonalityIntervalQuality third_quality = STANDARD_TONALITY_QUALITY_MAJOR;
        StandardTonalityIntervalQuality seventh_quality = STANDARD_TONALITY_QUALITY_MAJOR;
        ok = ok && standard_tonality_interval_quality(root, third_note, &size, &third_quality) &&
             standard_tonality_interval_quality(root, seventh_note, &size, &seventh_quality);
        if (root != NULL) {
            pitch_spelling_destroy(root);
        }
        if (third_note != NULL) {
            pitch_spelling_destroy(third_note);
        }
        if (seventh_note != NULL) {
            pitch_spelling_destroy(seventh_note);
        }
        if (!ok) {
            return false;
        }

        StandardTonalitySeventhQuality a;
        StandardTonalitySeventhQuality b;
        if (third_quality == STANDARD_TONALITY_QUALITY_MAJOR && seventh_quality == STANDARD_TONALITY_QUALITY_MAJOR) {
            a = b = STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
        } else if (third_quality == STANDARD_TONALITY_QUALITY_MAJOR &&
                   seventh_quality == STANDARD_TONALITY_QUALITY_MINOR) {
            a = b = STANDARD_TONALITY_SEVENTH_MAJOR_MINOR;
        } else if (third_quality == STANDARD_TONALITY_QUALITY_MINOR &&
                   seventh_quality == STANDARD_TONALITY_QUALITY_MINOR) {
            a = STANDARD_TONALITY_SEVENTH_MINOR_MINOR;
            b = STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED;
        } else if (third_quality == STANDARD_TONALITY_QUALITY_MINOR &&
                   seventh_quality == STANDARD_TONALITY_QUALITY_DIMINISHED) {
            a = b = STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED;
        } else {
            return false;
        }
        *root_letter = names[r].letter;
        *root_accidental = names[r].accidental;
        *first = a;
        *second = b;
        return true;
    }
    return false;
}

static void destroy_scale(PitchSpelling *scale[8]) {
    for (size_t i = 0; i < 8; i++) {
        pitch_spelling_destroy(scale[i]);
    }
}

static bool same_name(const PitchSpelling *note, PitchLetter letter, int accidental) {
    return pitch_spelling_letter(note) == letter && pitch_spelling_accidental(note) == accidental;
}

bool standard_tonality_roman_root_degree(StandardTonalityRomanKeyMode mode, const PitchSpelling *tonic,
                                         PitchLetter root_letter, int root_accidental, int *degree,
                                         bool *raised_leading_tone) {
    if (tonic == NULL || degree == NULL || !valid_mode(mode)) {
        return false;
    }

    PitchSpelling *scale[8];
    bool built = mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR
                     ? standard_tonality_build_major_scale(tonic, scale)
                     : standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, scale);
    if (!built) {
        return false;
    }
    int found = 0;
    for (int i = 0; i < 7; i++) {
        if (same_name(scale[i], root_letter, root_accidental)) {
            found = i + 1;
            break;
        }
    }
    destroy_scale(scale);
    if (found != 0) {
        *degree = found;
        if (raised_leading_tone != NULL) {
            *raised_leading_tone = false;
        }
        return true;
    }
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR) {
        return false;
    }

    /* Minor: the raised leading tone is the harmonic scale's seventh. */
    PitchSpelling *harmonic[8];
    if (!standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_HARMONIC, tonic, harmonic)) {
        return false;
    }
    bool raised = same_name(harmonic[6], root_letter, root_accidental);
    destroy_scale(harmonic);
    if (!raised) {
        return false;
    }
    *degree = 7;
    if (raised_leading_tone != NULL) {
        *raised_leading_tone = true;
    }
    return true;
}

/* QUALITY: the triad quality the key gives the degree. In minor a root on
   a natural degree takes the natural quality or, below the seventh, the
   one the raised leading tone gives (the harmonic minor's); a raised
   leading tone root takes only the harmonic minor's seventh-degree
   quality. */
static bool triad_in_key(StandardTonalityRomanKeyMode mode, int degree, bool raised,
                         StandardTonalityTriadQuality quality) {
    StandardTonalityTriadQuality table;
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR) {
        return standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MAJOR, degree, &table) &&
               table == quality;
    }
    if (raised) {
        return standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, degree, &table) &&
               table == quality;
    }
    if (standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, degree, &table) &&
        table == quality) {
        return true;
    }
    return degree < 7 &&
           standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, degree, &table) &&
           table == quality;
}

static bool seventh_in_key(StandardTonalityRomanKeyMode mode, int degree, bool raised,
                           StandardTonalitySeventhQuality quality) {
    StandardTonalitySeventhQuality table;
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR) {
        return standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MAJOR, degree, &table) &&
               table == quality;
    }
    if (raised) {
        return standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE, degree,
                                                        &table) &&
               table == quality;
    }
    if (standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL, degree, &table) &&
        table == quality) {
        return true;
    }
    return degree < 7 &&
           standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE, degree,
                                                    &table) &&
           table == quality;
}

bool standard_tonality_roman_numeral_fits_key(StandardTonalityRomanKeyMode mode, int degree, bool raised_leading_tone,
                                              const StandardTonalityRomanNumeral *numeral) {
    if (numeral == NULL || !valid_mode(mode) || degree < 1 || degree > 7 || numeral->degree != degree) {
        return false;
    }
    return numeral->seventh ? seventh_in_key(mode, degree, raised_leading_tone, numeral->seventh_quality)
                            : triad_in_key(mode, degree, raised_leading_tone, numeral->triad);
}

bool standard_tonality_identify_roman_numeral(StandardTonalityRomanKeyMode mode, const PitchSpelling *tonic,
                                              const PitchSpelling *const *notes, size_t count,
                                              StandardTonalityRomanNumeral *out) {
    if (tonic == NULL || notes == NULL || out == NULL || !valid_mode(mode)) {
        return false;
    }

    PitchLetter root_letter;
    int root_accidental;
    bool is_seventh = false;
    bool without_fifth = false;
    StandardTonalitySeventhQuality seventh_quality = STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
    StandardTonalitySeventhQuality alternate_quality = STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
    StandardTonalityTriadQuality triad_quality = STANDARD_TONALITY_TRIAD_MAJOR;
    if (standard_tonality_identify_seventh(notes, count, &root_letter, &root_accidental, &seventh_quality)) {
        is_seventh = true;
    } else if (standard_tonality_identify_triad(notes, count, &root_letter, &root_accidental, &triad_quality)) {
        is_seventh = false;
    } else if (identify_without_fifth(notes, count, &root_letter, &root_accidental, &seventh_quality,
                                      &alternate_quality)) {
        is_seventh = true;
        without_fifth = true;
    } else {
        return false;
    }

    int degree;
    bool raised;
    if (!standard_tonality_roman_root_degree(mode, tonic, root_letter, root_accidental, &degree, &raised)) {
        return false;
    }

    /* With the fifth unsaid, the key picks between the readings left open. */
    if (without_fifth && !seventh_in_key(mode, degree, raised, seventh_quality) &&
        seventh_in_key(mode, degree, raised, alternate_quality)) {
        seventh_quality = alternate_quality;
    }

    StandardTonalityRomanNumeral numeral;
    if (is_seventh) {
        if (!seventh_in_key(mode, degree, raised, seventh_quality) ||
            !standard_tonality_roman_numeral_seventh_chord(degree, seventh_quality, &numeral)) {
            return false;
        }
    } else if (!triad_in_key(mode, degree, raised, triad_quality) ||
               !standard_tonality_roman_numeral_triad(degree, triad_quality, &numeral)) {
        return false;
    }
    if (!standard_tonality_roman_numeral_is_unambiguous(&numeral)) {
        return false;
    }
    *out = numeral;
    return true;
}
