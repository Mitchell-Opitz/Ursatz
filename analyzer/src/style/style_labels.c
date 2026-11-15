#include <ursatz_analyzer/style_labels.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "standard-tonality/roman_numeral_grammar.h"
#include "standard-tonality/tonal_key.h"

static const char *const dimension_names[STYLE_DIMENSION_COUNT] = {"harmony", "rhythm", "melody", "form"};

static const struct {
    const char *category;
    StyleDimension dimension;
} category_table[] = {
    {STYLE_CATEGORY_CHORD_NUMERAL, STYLE_DIMENSION_HARMONY},
    {STYLE_CATEGORY_CHORD_PROGRESSION_2, STYLE_DIMENSION_HARMONY},
    {STYLE_CATEGORY_CHORD_PROGRESSION_3, STYLE_DIMENSION_HARMONY},
    {STYLE_CATEGORY_CADENCE_TYPE, STYLE_DIMENSION_HARMONY},
    {STYLE_CATEGORY_HARMONIC_RHYTHM, STYLE_DIMENSION_HARMONY},
    {STYLE_CATEGORY_DURATION, STYLE_DIMENSION_RHYTHM},
    {STYLE_CATEGORY_TEMPO, STYLE_DIMENSION_RHYTHM},
    {STYLE_CATEGORY_NOTE_DENSITY, STYLE_DIMENSION_RHYTHM},
    {STYLE_CATEGORY_METER, STYLE_DIMENSION_RHYTHM},
    {STYLE_CATEGORY_SYNCOPATION, STYLE_DIMENSION_RHYTHM},
    {STYLE_CATEGORY_MELODIC_INTERVAL, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_CONTOUR, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_PHRASE_RANGE, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_MOTIVE_DEGREES, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_MOTIVE_INTERVALS, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_MOTIVE_RHYTHM, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_MOTIVE_NCT_FIGURE, STYLE_DIMENSION_MELODY},
    {STYLE_CATEGORY_THEME_FORM, STYLE_DIMENSION_FORM},
    {STYLE_CATEGORY_PHRASE_LENGTH, STYLE_DIMENSION_FORM},
    {STYLE_CATEGORY_SECTION_LETTERS, STYLE_DIMENSION_FORM},
    {STYLE_CATEGORY_MODE, STYLE_DIMENSION_FORM},
};

const char *style_dimension_name(StyleDimension dimension) {
    if ((int)dimension < 0 || (int)dimension >= STYLE_DIMENSION_COUNT) {
        return NULL;
    }
    return dimension_names[dimension];
}

bool style_dimension_from_name(const char *name, StyleDimension *out) {
    if (!name || !out) {
        return false;
    }
    for (int i = 0; i < STYLE_DIMENSION_COUNT; i++) {
        if (strcmp(name, dimension_names[i]) == 0) {
            *out = (StyleDimension)i;
            return true;
        }
    }
    return false;
}

bool style_category_dimension(const char *category, StyleDimension *out) {
    if (!category || !out) {
        return false;
    }
    for (size_t i = 0; i < sizeof(category_table) / sizeof(category_table[0]); i++) {
        if (strcmp(category, category_table[i].category) == 0) {
            *out = category_table[i].dimension;
            return true;
        }
    }
    return false;
}

char *style_label_numeral(const char *numeral) {
    StandardTonalityNumeralText parsed;
    char text[32];
    if (!numeral || !standard_tonality_numeral_parse(numeral, &parsed)) {
        return NULL;
    }
    parsed.inversion = 0;
    if (!standard_tonality_numeral_format(&parsed, text, sizeof text)) {
        return NULL;
    }
    return strdup(text);
}

char *style_label_progression(const char *const *numerals, size_t count) {
    if (!numerals || count == 0) {
        return NULL;
    }
    size_t length = 0;
    for (size_t i = 0; i < count; i++) {
        if (!numerals[i]) {
            return NULL;
        }
        length += strlen(numerals[i]) + 1;
    }
    char *label = malloc(length);
    if (!label) {
        return NULL;
    }
    char *cursor = label;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            *cursor++ = '-';
        }
        size_t n = strlen(numerals[i]);
        memcpy(cursor, numerals[i], n);
        cursor += n;
    }
    *cursor = '\0';
    return label;
}

void style_label_interval(int semitones, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    int magnitude = semitones < 0 ? -semitones : semitones;
    if (semitones == 0) {
        snprintf(buffer, size, "0");
    } else if (magnitude > 12) {
        snprintf(buffer, size, "%s13+", semitones < 0 ? "-" : "+");
    } else {
        snprintf(buffer, size, "%s%d", semitones < 0 ? "-" : "+", magnitude);
    }
}

const char *style_label_contour(int semitones) {
    return semitones > 0 ? "up" : (semitones < 0 ? "down" : "same");
}

void style_label_duration(unsigned long numerator, unsigned long denominator, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    if (denominator == 0 || numerator == 0) {
        snprintf(buffer, size, "0/1");
        return;
    }
    unsigned long divisor = (unsigned long)rational_gcd_unsigned(numerator, denominator);
    snprintf(buffer, size, "%lu/%lu", numerator / divisor, denominator / divisor);
}

const char *style_label_degree(int pitch_class, int tonic_pc) {
    return standard_tonality_chromatic_degree_label(pitch_class - tonic_pc);
}

char *style_label_join(const char *const *parts, size_t count) {
    if (!parts || count == 0) {
        return NULL;
    }
    size_t length = 1;
    for (size_t i = 0; i < count; i++) {
        length += strlen(parts[i]) + 1;
    }
    char *joined = malloc(length);
    if (!joined) {
        return NULL;
    }
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        size_t part = strlen(parts[i]);
        if (i > 0) {
            joined[at++] = ' ';
        }
        memcpy(joined + at, parts[i], part);
        at += part;
    }
    joined[at] = '\0';
    return joined;
}

char *style_label_motive_intervals(const int *semitones, size_t count) {
    if (!semitones || count == 0) {
        return NULL;
    }
    size_t capacity = count * 13 + 1;
    char *joined = malloc(capacity);
    if (!joined) {
        return NULL;
    }
    size_t length = 0;
    for (size_t i = 0; i < count; i++) {
        int wrote = semitones[i] == 0 ? snprintf(joined + length, capacity - length, "%s0", length ? " " : "")
                                      : snprintf(joined + length, capacity - length, "%s%+d", length ? " " : "",
                                                 semitones[i]);
        if (wrote < 0 || (size_t)wrote >= capacity - length) {
            free(joined);
            return NULL;
        }
        length += (size_t)wrote;
    }
    return joined;
}

bool style_label_nct_figure(const char *previous, const char *degree, const char *type, const char *next,
                            char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (!degree || !type) {
        return false;
    }
    int wrote = snprintf(buffer, size, "%s %s:%s %s", previous ? previous : "-", degree, type, next ? next : "-");
    if (wrote < 0 || (size_t)wrote >= size) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

void style_label_tempo(double quarters_per_minute, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    if (!(quarters_per_minute >= 40.0)) {
        snprintf(buffer, size, "<40");
    } else if (quarters_per_minute >= 220.0) {
        snprintf(buffer, size, "220+");
    } else {
        int low = 40 + 20 * (int)((quarters_per_minute - 40.0) / 20.0);
        snprintf(buffer, size, "%d-%d", low, low + 19);
    }
}

void style_label_density(double attacks_per_beat, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    double halves = attacks_per_beat > 0.0 ? attacks_per_beat * 2.0 + 0.5 : 0.0;
    if (halves >= 8.0) {
        snprintf(buffer, size, "4+");
        return;
    }
    int whole_halves = (int)halves;
    if (whole_halves % 2 == 0) {
        snprintf(buffer, size, "%d", whole_halves / 2);
    } else {
        snprintf(buffer, size, "%d/2", whole_halves);
    }
}
