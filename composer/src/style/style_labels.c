#include <ursatz_composer/style.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *category;
    char *label;
} Label;

struct ComposerStyleLabels {
    Label *items;
    size_t count;
    size_t capacity;
};

ComposerStyleLabels *composer_style_labels_create(void) {
    return calloc(1, sizeof(ComposerStyleLabels));
}

void composer_style_labels_destroy(ComposerStyleLabels *labels) {
    if (!labels) {
        return;
    }
    for (size_t i = 0; i < labels->count; i++) {
        free(labels->items[i].category);
        free(labels->items[i].label);
    }
    free(labels->items);
    free(labels);
}

size_t composer_style_labels_count(const ComposerStyleLabels *labels) {
    return labels ? labels->count : 0;
}

bool composer_style_labels_at(const ComposerStyleLabels *labels, size_t i, const char **category, const char **label) {
    if (!labels || i >= labels->count) {
        return false;
    }
    if (category) {
        *category = labels->items[i].category;
    }
    if (label) {
        *label = labels->items[i].label;
    }
    return true;
}

bool composer_style_labels_add(ComposerStyleLabels *labels, const char *category, const char *label) {
    if (!labels || !category || !label) {
        return false;
    }
    if (labels->count == labels->capacity) {
        size_t capacity = labels->capacity ? labels->capacity * 2 : 32;
        Label *grown = realloc(labels->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        labels->items = grown;
        labels->capacity = capacity;
    }
    Label *item = &labels->items[labels->count];
    item->category = strdup(category);
    item->label = strdup(label);
    if (!item->category || !item->label) {
        free(item->category);
        free(item->label);
        return false;
    }
    labels->count++;
    return true;
}

/* ------------------------------------------------------------- harmony */

bool composer_style_labels_add_harmony(ComposerStyleLabels *labels, const char *previous, const char *const *numerals,
                                       size_t count) {
    if (!labels || (count > 0 && !numerals)) {
        return false;
    }
    /* The last two numerals of the run, collapsed; owned. */
    char *last = previous ? style_label_numeral(previous) : NULL;
    char *before_last = NULL;
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        char *numeral = numerals[i] ? style_label_numeral(numerals[i]) : NULL;
        if (!numeral) {
            continue;
        }
        ok = composer_style_labels_add(labels, STYLE_CATEGORY_CHORD_NUMERAL, numeral);
        if (ok && !(last && strcmp(last, numeral) == 0)) {
            if (last) {
                const char *pair[2] = {last, numeral};
                char *label = style_label_progression(pair, 2);
                ok = label && composer_style_labels_add(labels, STYLE_CATEGORY_CHORD_PROGRESSION_2, label);
                free(label);
            }
            if (ok && last && before_last) {
                const char *triple[3] = {before_last, last, numeral};
                char *label = style_label_progression(triple, 3);
                ok = label && composer_style_labels_add(labels, STYLE_CATEGORY_CHORD_PROGRESSION_3, label);
                free(label);
            }
            free(before_last);
            before_last = last;
            last = numeral;
        } else {
            free(numeral);
        }
    }
    free(last);
    free(before_last);
    return ok;
}

/* -------------------------------------------------------------- melody */

bool composer_style_labels_add_melody(ComposerStyleLabels *labels, const int *before, const ComposerStyleNote *notes,
                                      size_t count) {
    if (!labels || (count > 0 && !notes)) {
        return false;
    }
    bool have_previous = before != NULL;
    int previous = before ? *before : 0;
    for (size_t i = 0; i < count; i++) {
        char label[48];
        style_label_duration(notes[i].duration_numerator, notes[i].duration_denominator, label, sizeof label);
        if (!composer_style_labels_add(labels, STYLE_CATEGORY_DURATION, label)) {
            return false;
        }
        if (have_previous) {
            int step = notes[i].pitch - previous;
            style_label_interval(step, label, sizeof label);
            if (!composer_style_labels_add(labels, STYLE_CATEGORY_MELODIC_INTERVAL, label) ||
                !composer_style_labels_add(labels, STYLE_CATEGORY_CONTOUR, style_label_contour(step))) {
                return false;
            }
        }
        have_previous = true;
        previous = notes[i].pitch;
    }
    return true;
}

/* ----------------------------------------------------------- closeness */

__extension__ typedef unsigned __int128 Wide;

static Wide wide_gcd(Wide a, Wide b) {
    while (b) {
        Wide rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

/* Reduces a fraction of 128-bit parts into 64-bit parts, rounding to 30 bits when it does not fit. */
static ComposerRatio narrow(Wide numerator, Wide denominator) {
    ComposerRatio out = {0, 1};
    Wide divisor = wide_gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    if (numerator <= ULONG_MAX && denominator <= ULONG_MAX) {
        out.numerator = (unsigned long)numerator;
        out.denominator = (unsigned long)denominator;
        return out;
    }
    /* numerator <= denominator for a closeness: scale to 2^30. */
    Wide scale = (Wide)1 << 30;
    Wide shift = 0;
    while ((denominator >> shift) > ((Wide)1 << 64) / scale - 1) {
        shift++;
    }
    out.numerator = (unsigned long)(((numerator >> shift) * scale + ((denominator >> shift) / 2)) / (denominator >> shift));
    out.denominator = (unsigned long)scale;
    return out;
}

bool composer_style_labels_add_form(ComposerStyleLabels *labels, const char *form, const unsigned *phrase_bars,
                                    size_t phrase_count) {
    if (!labels || (phrase_count > 0 && !phrase_bars)) {
        return false;
    }
    if (form && !composer_style_labels_add(labels, STYLE_CATEGORY_THEME_FORM, form)) {
        return false;
    }
    for (size_t i = 0; i < phrase_count; i++) {
        char text[32];
        snprintf(text, sizeof text, "%u", phrase_bars[i]);
        if (!composer_style_labels_add(labels, STYLE_CATEGORY_PHRASE_LENGTH, text)) {
            return false;
        }
    }
    return true;
}

bool composer_style_closeness(const ComposerStyle *style, const ComposerStyleLabels *labels, ComposerRatio *out) {
    if (!style || !labels || !out) {
        return false;
    }
    /* The sum of count/largest over the labels, kept as a reduced fraction while it fits in 128 bits. */
    Wide numerator = 0;
    Wide denominator = 1;
    bool exact = true;
    long double approximate = 0.0L;
    size_t used = 0;
    for (size_t i = 0; i < labels->count; i++) {
        ComposerStyleShare share;
        if (!composer_style_share(style, labels->items[i].category, labels->items[i].label, &share) ||
            share.largest == 0) {
            continue;
        }
        used++;
        approximate += (long double)share.count / (long double)share.largest;
        if (!exact) {
            continue;
        }
        Wide scaled_sum;
        Wide scaled_term;
        Wide next_denominator;
        if (__builtin_mul_overflow(numerator, (Wide)share.largest, &scaled_sum) ||
            __builtin_mul_overflow(denominator, (Wide)share.count, &scaled_term) ||
            __builtin_mul_overflow(denominator, (Wide)share.largest, &next_denominator) ||
            __builtin_add_overflow(scaled_sum, scaled_term, &numerator)) {
            exact = false;
            continue;
        }
        denominator = next_denominator;
        Wide divisor = wide_gcd(numerator, denominator);
        if (divisor > 1) {
            numerator /= divisor;
            denominator /= divisor;
        }
    }
    if (used == 0) {
        out->numerator = 1;
        out->denominator = 1;
        return true;
    }
    if (exact && !__builtin_mul_overflow(denominator, (Wide)used, &denominator)) {
        *out = narrow(numerator, denominator);
        return true;
    }
    /* Past 128 bits: the mean to 30 bits. */
    long double mean = approximate / (long double)used;
    unsigned long scale = 1UL << 30;
    unsigned long rounded = (unsigned long)(mean * (long double)scale + 0.5L);
    out->numerator = rounded > scale ? scale : rounded;
    out->denominator = scale;
    return true;
}
