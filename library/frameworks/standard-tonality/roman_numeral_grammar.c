#include "roman_numeral_grammar.h"

#include <stdio.h>
#include <string.h>
#include "pitch/midi_pitch.h"

#define FLAT_SIGN "\xE2\x99\xAD"
#define SHARP_SIGN "\xE2\x99\xAF"
#define DEGREE_SIGN "\xC2\xB0"
#define O_SLASH "\xC3\xB8"
#define EMPTY_SET "\xE2\x88\x85"

typedef enum { FIGURE_NONE, FIGURE_TRIAD, FIGURE_SEVENTH } FigureKind;

typedef struct {
    const char *text;
    FigureKind kind;
    int inversion;
} FigureSpelling;

/* Longest spelling first, so "6/4" is not "6" and "65" is not "6" then "5". */
static const FigureSpelling FIGURES[] = {
    {"6/4", FIGURE_TRIAD, 2},   {"6/5", FIGURE_SEVENTH, 1}, {"4/3", FIGURE_SEVENTH, 2}, {"4/2", FIGURE_SEVENTH, 3},
    {"64", FIGURE_TRIAD, 2},    {"65", FIGURE_SEVENTH, 1},  {"43", FIGURE_SEVENTH, 2},  {"42", FIGURE_SEVENTH, 3},
    {"6", FIGURE_TRIAD, 1},     {"7", FIGURE_SEVENTH, 0},   {"2", FIGURE_SEVENTH, 3}};

typedef enum { MARK_NONE, MARK_DIMINISHED, MARK_AUGMENTED, MARK_HALF_DIMINISHED, MARK_MAJOR_SEVENTH } Mark;

bool standard_tonality_numeral_quality_is_seventh(StandardTonalityNumeralQuality quality) {
    return quality >= STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH && quality < STANDARD_TONALITY_NUMERAL_QUALITY_COUNT;
}

static bool quality_valid(StandardTonalityNumeralQuality quality) {
    return quality >= 0 && quality < STANDARD_TONALITY_NUMERAL_QUALITY_COUNT;
}

static bool quality_uppercase(StandardTonalityNumeralQuality quality) {
    return quality == STANDARD_TONALITY_NUMERAL_MAJOR || quality == STANDARD_TONALITY_NUMERAL_AUGMENTED ||
           quality == STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH || quality == STANDARD_TONALITY_NUMERAL_MAJOR_SEVENTH;
}

static bool valid_numeral(const StandardTonalityNumeralText *numeral) {
    if (numeral == NULL || numeral->degree < 1 || numeral->degree > 7 || numeral->alteration < -1 ||
        numeral->alteration > 1 || !quality_valid(numeral->quality) || numeral->inversion < 0) {
        return false;
    }
    if (numeral->inversion > (standard_tonality_numeral_quality_is_seventh(numeral->quality) ? 3 : 2)) {
        return false;
    }
    return !numeral->has_target || (numeral->target_degree >= 1 && numeral->target_degree <= 7 &&
                                    numeral->target_alteration >= -1 && numeral->target_alteration <= 1);
}

/* Reads the numeral letters: the degree and whether they were uppercase. Longest first, so VII is not VI then I.
   Returns the bytes taken, or 0. */
static size_t parse_degree(const char *text, int *degree, bool *uppercase) {
    for (int d = 7; d >= 1; d--) {
        const char *upper = standard_tonality_roman_numeral_degree(d, true);
        const char *lower = standard_tonality_roman_numeral_degree(d, false);
        if (strncmp(text, upper, strlen(upper)) == 0) {
            *degree = d;
            *uppercase = true;
            return strlen(upper);
        }
        if (strncmp(text, lower, strlen(lower)) == 0) {
            *degree = d;
            *uppercase = false;
            return strlen(lower);
        }
    }
    return 0;
}

static size_t parse_alteration(const char *text, int *alteration) {
    *alteration = 0;
    if (text[0] == 'b') {
        *alteration = -1;
        return 1;
    }
    if (text[0] == '#') {
        *alteration = 1;
        return 1;
    }
    if (strncmp(text, FLAT_SIGN, strlen(FLAT_SIGN)) == 0) {
        *alteration = -1;
        return strlen(FLAT_SIGN);
    }
    if (strncmp(text, SHARP_SIGN, strlen(SHARP_SIGN)) == 0) {
        *alteration = 1;
        return strlen(SHARP_SIGN);
    }
    return 0;
}

static size_t starts_with(const char *text, const char *prefix) {
    size_t length = strlen(prefix);
    return strncmp(text, prefix, length) == 0 ? length : 0;
}

/* The mark after the numeral. Uppercase takes "+" and "M"/"maj"; lowercase the circle and the slashed circle. */
static size_t parse_mark(const char *text, bool uppercase, Mark *mark) {
    size_t taken;
    *mark = MARK_NONE;
    if (uppercase) {
        if (text[0] == '+') {
            *mark = MARK_AUGMENTED;
            return 1;
        }
        if ((taken = starts_with(text, "maj")) != 0 || (taken = starts_with(text, "M")) != 0) {
            *mark = MARK_MAJOR_SEVENTH;
            return taken;
        }
        return 0;
    }
    if ((taken = starts_with(text, "o")) != 0 || (taken = starts_with(text, DEGREE_SIGN)) != 0) {
        *mark = MARK_DIMINISHED;
        return taken;
    }
    if ((taken = starts_with(text, O_SLASH)) != 0 || (taken = starts_with(text, EMPTY_SET)) != 0 ||
        (taken = starts_with(text, "/o")) != 0 || (taken = starts_with(text, "%")) != 0) {
        *mark = MARK_HALF_DIMINISHED;
        return taken;
    }
    return 0;
}

static bool parse_figure(const char *text, FigureKind *kind, int *inversion, size_t *taken) {
    *kind = FIGURE_NONE;
    *inversion = 0;
    *taken = 0;
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        size_t length = starts_with(text, FIGURES[i].text);
        if (length != 0) {
            *kind = FIGURES[i].kind;
            *inversion = FIGURES[i].inversion;
            *taken = length;
            return true;
        }
    }
    return true;
}

/* The quality the case, mark and figure give, or false when they contradict each other. */
static bool resolve_quality(bool uppercase, Mark mark, FigureKind figure, StandardTonalityNumeralQuality *quality) {
    bool seventh = figure == FIGURE_SEVENTH;
    if (uppercase) {
        switch (mark) {
            case MARK_NONE:
                *quality = seventh ? STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH : STANDARD_TONALITY_NUMERAL_MAJOR;
                return true;
            case MARK_AUGMENTED:
                *quality = STANDARD_TONALITY_NUMERAL_AUGMENTED;
                return !seventh;
            case MARK_MAJOR_SEVENTH:
                *quality = STANDARD_TONALITY_NUMERAL_MAJOR_SEVENTH;
                return seventh;
            default:
                return false;
        }
    }
    switch (mark) {
        case MARK_NONE:
            *quality = seventh ? STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH : STANDARD_TONALITY_NUMERAL_MINOR;
            return true;
        case MARK_DIMINISHED:
            *quality = seventh ? STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH
                               : STANDARD_TONALITY_NUMERAL_DIMINISHED;
            return true;
        case MARK_HALF_DIMINISHED:
            *quality = STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH;
            return seventh;
        default:
            return false;
    }
}

bool standard_tonality_numeral_parse(const char *text, StandardTonalityNumeralText *out) {
    if (text == NULL || out == NULL) {
        return false;
    }
    StandardTonalityNumeralText parsed;
    memset(&parsed, 0, sizeof(parsed));

    const char *p = text + parse_alteration(text, &parsed.alteration);
    bool uppercase;
    size_t taken = parse_degree(p, &parsed.degree, &uppercase);
    if (taken == 0) {
        return false;
    }
    p += taken;

    Mark mark;
    p += parse_mark(p, uppercase, &mark);

    FigureKind figure;
    parse_figure(p, &figure, &parsed.inversion, &taken);
    p += taken;
    if (!resolve_quality(uppercase, mark, figure, &parsed.quality)) {
        return false;
    }

    if (*p == '/') {
        p++;
        p += parse_alteration(p, &parsed.target_alteration);
        bool target_uppercase;
        taken = parse_degree(p, &parsed.target_degree, &target_uppercase);
        if (taken == 0) {
            return false;
        }
        p += taken;
        parsed.has_target = true;
        parsed.target_minor = !target_uppercase;
    }
    if (*p != '\0') {
        return false;
    }
    *out = parsed;
    return true;
}

static const char *mark_text(StandardTonalityNumeralQuality quality, StandardTonalityNumeralStyle style) {
    switch (quality) {
        case STANDARD_TONALITY_NUMERAL_DIMINISHED:
        case STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH:
            return style == STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY ? DEGREE_SIGN : "o";
        case STANDARD_TONALITY_NUMERAL_AUGMENTED:
            return "+";
        case STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH:
            if (style == STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY) {
                return EMPTY_SET;
            }
            return style == STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT ? "/o" : O_SLASH;
        case STANDARD_TONALITY_NUMERAL_MAJOR_SEVENTH:
            return "M";
        default:
            return "";
    }
}

static const char *figure_text(bool seventh, int inversion, StandardTonalityNumeralStyle style) {
    static const char *const TRIAD[3] = {"", "6", "64"};
    static const char *const TRIAD_SLASHED[3] = {"", "6", "6/4"};
    static const char *const SEVENTH[4] = {"7", "65", "43", "42"};
    static const char *const SEVENTH_SLASHED[4] = {"7", "6/5", "4/3", "4/2"};
    bool slashed = style != STANDARD_TONALITY_NUMERAL_STYLE_CANONICAL;
    if (seventh) {
        return slashed ? SEVENTH_SLASHED[inversion] : SEVENTH[inversion];
    }
    return slashed ? TRIAD_SLASHED[inversion] : TRIAD[inversion];
}

static const char *alteration_text(int alteration) {
    return alteration < 0 ? "b" : (alteration > 0 ? "#" : "");
}

bool standard_tonality_numeral_format_styled(const StandardTonalityNumeralText *numeral,
                                             StandardTonalityNumeralStyle style, char *buffer, size_t size) {
    if (buffer == NULL || size == 0 || !valid_numeral(numeral) ||
        (style != STANDARD_TONALITY_NUMERAL_STYLE_CANONICAL && style != STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY &&
         style != STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT)) {
        return false;
    }
    bool seventh = standard_tonality_numeral_quality_is_seventh(numeral->quality);
    /* A dominant seventh has no mark; the figure "7" is the whole seventh sign. */
    char text[64];
    int length = snprintf(text, sizeof(text), "%s%s%s%s", alteration_text(numeral->alteration),
                          standard_tonality_roman_numeral_degree(numeral->degree, quality_uppercase(numeral->quality)),
                          mark_text(numeral->quality, style), figure_text(seventh, numeral->inversion, style));
    if (length < 0 || (size_t)length >= sizeof(text)) {
        return false;
    }
    if (numeral->has_target) {
        int more = snprintf(text + length, sizeof(text) - (size_t)length, "/%s%s",
                            alteration_text(numeral->target_alteration),
                            standard_tonality_roman_numeral_degree(numeral->target_degree, !numeral->target_minor));
        if (more < 0 || (size_t)(length + more) >= sizeof(text)) {
            return false;
        }
        length += more;
    }
    if ((size_t)length >= size) {
        return false;
    }
    memcpy(buffer, text, (size_t)length + 1);
    return true;
}

bool standard_tonality_numeral_format(const StandardTonalityNumeralText *numeral, char *buffer, size_t size) {
    return standard_tonality_numeral_format_styled(numeral, STANDARD_TONALITY_NUMERAL_STYLE_CANONICAL, buffer, size);
}

bool standard_tonality_numeral_root_semitone(const StandardTonalityNumeralText *numeral, StandardTonalityMode mode,
                                             int *semitones) {
    if (semitones == NULL || !valid_numeral(numeral)) {
        return false;
    }
    int root;
    if (numeral->has_target) {
        int target = standard_tonality_degree_semitone(mode, numeral->target_degree);
        StandardTonalityMode local = numeral->target_minor ? STANDARD_TONALITY_MODE_HARMONIC_MINOR
                                                           : STANDARD_TONALITY_MODE_MAJOR;
        int within = standard_tonality_degree_semitone(local, numeral->degree);
        if (target < 0 || within < 0) {
            return false;
        }
        root = target + numeral->target_alteration + within + numeral->alteration;
    } else {
        int degree = standard_tonality_degree_semitone(mode, numeral->degree);
        if (degree < 0) {
            return false;
        }
        root = degree + numeral->alteration;
    }
    *semitones = pitch_class_of_midi(root);
    return true;
}

bool standard_tonality_numeral_base(const StandardTonalityNumeralText *numeral, StandardTonalityRomanNumeral *out) {
    if (out == NULL || !valid_numeral(numeral)) {
        return false;
    }
    StandardTonalitySeventhQuality seventh;
    switch (numeral->quality) {
        case STANDARD_TONALITY_NUMERAL_MAJOR:
            return standard_tonality_roman_numeral_triad(numeral->degree, STANDARD_TONALITY_TRIAD_MAJOR, out);
        case STANDARD_TONALITY_NUMERAL_MINOR:
            return standard_tonality_roman_numeral_triad(numeral->degree, STANDARD_TONALITY_TRIAD_MINOR, out);
        case STANDARD_TONALITY_NUMERAL_DIMINISHED:
            return standard_tonality_roman_numeral_triad(numeral->degree, STANDARD_TONALITY_TRIAD_DIMINISHED, out);
        case STANDARD_TONALITY_NUMERAL_AUGMENTED:
            return standard_tonality_roman_numeral_triad(numeral->degree, STANDARD_TONALITY_TRIAD_AUGMENTED, out);
        case STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH:
            return standard_tonality_roman_numeral_inferred_seventh(numeral->degree, &seventh) &&
                   standard_tonality_roman_numeral_seventh_chord(numeral->degree, seventh, out);
        case STANDARD_TONALITY_NUMERAL_MAJOR_SEVENTH:
            return standard_tonality_roman_numeral_seventh_chord(numeral->degree, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR,
                                                                 out);
        case STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH:
            return standard_tonality_roman_numeral_seventh_chord(numeral->degree, STANDARD_TONALITY_SEVENTH_MINOR_MINOR,
                                                                 out);
        case STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH:
            return standard_tonality_roman_numeral_seventh_chord(
                numeral->degree, STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED, out);
        case STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH:
            return standard_tonality_roman_numeral_seventh_chord(
                numeral->degree, STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED, out);
        default:
            return false;
    }
}

bool standard_tonality_numeral_from_base(const StandardTonalityRomanNumeral *numeral,
                                         StandardTonalityNumeralText *out) {
    if (numeral == NULL || out == NULL || numeral->degree < 1 || numeral->degree > 7) {
        return false;
    }
    StandardTonalityNumeralQuality quality;
    if (numeral->seventh) {
        switch (numeral->seventh_quality) {
            case STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR:
            case STANDARD_TONALITY_SEVENTH_MAJOR_MINOR:
                quality = STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH;
                break;
            case STANDARD_TONALITY_SEVENTH_MINOR_MINOR:
                quality = STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH;
                break;
            case STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED:
                quality = STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH;
                break;
            case STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED:
                quality = STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH;
                break;
            default:
                return false;
        }
    } else {
        switch (numeral->triad) {
            case STANDARD_TONALITY_TRIAD_MAJOR:
                quality = STANDARD_TONALITY_NUMERAL_MAJOR;
                break;
            case STANDARD_TONALITY_TRIAD_MINOR:
                quality = STANDARD_TONALITY_NUMERAL_MINOR;
                break;
            case STANDARD_TONALITY_TRIAD_DIMINISHED:
                quality = STANDARD_TONALITY_NUMERAL_DIMINISHED;
                break;
            case STANDARD_TONALITY_TRIAD_AUGMENTED:
                quality = STANDARD_TONALITY_NUMERAL_AUGMENTED;
                break;
            default:
                return false;
        }
    }
    memset(out, 0, sizeof(*out));
    out->degree = numeral->degree;
    out->quality = quality;
    return true;
}
