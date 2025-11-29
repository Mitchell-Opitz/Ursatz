#ifndef MUSIC_THEORY_STYLE_FRACTION_H
#define MUSIC_THEORY_STYLE_FRACTION_H

/*
 * Exact-rational arithmetic for style frequencies (numerator/denominator over
 * unsigned long, never float/double -- Law 19). Results are reduced to
 * lowest terms with a positive denominator. Nothing wraps around: an
 * operation whose exact result does not fit returns
 * STYLE_FRACTION_ERR_OVERFLOW and leaves its output untouched; a fraction
 * with a zero denominator is STYLE_FRACTION_ERR_ZERO_DENOMINATOR.
 *
 * theory/ keeps its own small copy rather than reusing rules/
 * preference_weight.h: theory (L7) cannot depend on rules.
 */

typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} StyleFraction;

typedef enum {
    STYLE_FRACTION_OK = 0,
    STYLE_FRACTION_ERR_NULL_ARGUMENT,
    STYLE_FRACTION_ERR_ZERO_DENOMINATOR,
    STYLE_FRACTION_ERR_OVERFLOW
} StyleFractionResult;

StyleFractionResult style_fraction_reduce(const StyleFraction *fraction, StyleFraction *out);
StyleFractionResult style_fraction_add(const StyleFraction *a, const StyleFraction *b, StyleFraction *out);
StyleFractionResult style_fraction_multiply(const StyleFraction *a, const StyleFraction *b, StyleFraction *out);

#endif
