#include <ursatz_analyzer/style_labels.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool is_roman_letter(char c) {
    return c != '\0' && strchr("IViv", c) != NULL;
}

/* Length of the numeral part of one segment: accidentals, Roman letters, then quality marks. 0 if there are no
   Roman letters. */
static size_t numeral_prefix_length(const char *segment) {
    size_t i = 0;
    while (segment[i] == '#' || segment[i] == 'b') {
        i++;
    }
    size_t letters = 0;
    while (is_roman_letter(segment[i])) {
        i++;
        letters++;
    }
    if (letters == 0) {
        return 0;
    }
    /* Quality marks: '+', 'o', and the UTF-8 degree sign (C2 B0) and o-slash (C3 B8). */
    while (segment[i] == '+' || segment[i] == 'o' || (unsigned char)segment[i] == 0xC2 ||
           (unsigned char)segment[i] == 0xB0 || (unsigned char)segment[i] == 0xC3 || (unsigned char)segment[i] == 0xB8) {
        i++;
    }
    return i;
}

static bool figure_is(const char *figure, const char *const *list) {
    for (size_t i = 0; list[i]; i++) {
        if (strcmp(figure, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

char *style_label_numeral(const char *numeral) {
    static const char *const triad_figures[] = {"", "6", "6/4", "64", "63", NULL};
    static const char *const seventh_figures[] = {"7", "6/5", "65", "4/3", "43", "4/2", "42", "2", NULL};

    if (!numeral || !numeral[0]) {
        return NULL;
    }
    /* The first segment ends at a '/' followed by a letter or accidental (an applied target); a '/' followed by a
       digit belongs to the figure. */
    size_t first_end = 0;
    while (numeral[first_end] && !(numeral[first_end] == '/' && !isdigit((unsigned char)numeral[first_end + 1]))) {
        first_end++;
    }
    size_t prefix = numeral_prefix_length(numeral);
    if (prefix == 0 || prefix > first_end) {
        return NULL;
    }
    size_t figure_length = first_end - prefix;
    char figure[16];
    if (figure_length >= sizeof figure) {
        return NULL;
    }
    memcpy(figure, numeral + prefix, figure_length);
    figure[figure_length] = '\0';

    const char *kept;
    if (figure_is(figure, triad_figures)) {
        kept = "";
    } else if (figure_is(figure, seventh_figures)) {
        kept = "7";
    } else {
        kept = figure;
    }
    /* Every applied target must itself be a numeral. */
    const char *rest = numeral + first_end;
    const char *target = rest;
    while (*target == '/') {
        target++;
        size_t target_prefix = numeral_prefix_length(target);
        if (target_prefix == 0) {
            return NULL;
        }
        target += target_prefix;
        while (isdigit((unsigned char)*target)) {
            target++;
        }
    }
    if (*target != '\0') {
        return NULL;
    }
    size_t length = prefix + strlen(kept) + strlen(rest);
    char *label = malloc(length + 1);
    if (!label) {
        return NULL;
    }
    memcpy(label, numeral, prefix);
    strcpy(label + prefix, kept);
    strcat(label, rest);
    return label;
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

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b) {
        unsigned long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

void style_label_duration(unsigned long numerator, unsigned long denominator, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    if (denominator == 0 || numerator == 0) {
        snprintf(buffer, size, "0/1");
        return;
    }
    unsigned long divisor = gcd(numerator, denominator);
    snprintf(buffer, size, "%lu/%lu", numerator / divisor, denominator / divisor);
}
