#include "analysis_motive_match.h"

#include <string.h>

static bool comparable(const StandardFormMotiveShape *a, const StandardFormMotiveShape *b) {
    return a != NULL && b != NULL && a->generic == b->generic && a->intervals == b->intervals && a->intervals > 0;
}

static void full(StandardFormMotiveMatch *out, StandardFormMotiveKind kind, size_t intervals) {
    out->kind = (int)kind;
    out->mismatches = 0;
    out->intervals = intervals;
    out->rhythm_factor = 1.0;
    out->score = 1.0;
    out->fragment_part = -1;
    out->fragment_intervals = 0;
}

static bool same_rhythm(const StandardFormMotiveShape *a, const StandardFormMotiveShape *b) {
    for (size_t i = 0; i < a->intervals; i++) {
        if (standard_form_q_compare(a->ioi[i], b->ioi[i]) != 0) {
            return false;
        }
    }
    return true;
}

bool standard_form_motive_match_pattern(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                        StandardFormMotiveMatch *out) {
    if (out == NULL || !comparable(prototype, candidate) ||
        memcmp(prototype->steps, candidate->steps, prototype->intervals * sizeof(int)) != 0 || !same_rhythm(prototype, candidate)) {
        return false;
    }
    StandardFormMotiveKind kind;
    if (memcmp(prototype->semitones, candidate->semitones, prototype->intervals * sizeof(int)) != 0) {
        kind = STANDARD_FORM_MOTIVE_TONAL;
    } else {
        kind = prototype->first_semitone == candidate->first_semitone ? STANDARD_FORM_MOTIVE_EXACT : STANDARD_FORM_MOTIVE_REAL;
    }
    full(out, kind, prototype->intervals);
    return true;
}

bool standard_form_motive_match_inversion(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                          StandardFormMotiveMatch *out) {
    if (out == NULL || !comparable(prototype, candidate) || !same_rhythm(prototype, candidate)) {
        return false;
    }
    for (size_t i = 0; i < prototype->intervals; i++) {
        if (candidate->steps[i] != -prototype->steps[i]) {
            return false;
        }
    }
    full(out, STANDARD_FORM_MOTIVE_INVERTED, prototype->intervals);
    return true;
}

bool standard_form_motive_match_retrograde(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                           StandardFormMotiveMatch *out) {
    if (out == NULL || !comparable(prototype, candidate)) {
        return false;
    }
    size_t n = prototype->intervals;
    for (size_t i = 0; i < n; i++) {
        if (candidate->steps[i] != -prototype->steps[n - 1 - i] ||
            standard_form_q_compare(candidate->ioi[i], prototype->ioi[n - 1 - i]) != 0) {
            return false;
        }
    }
    full(out, STANDARD_FORM_MOTIVE_RETROGRADE, n);
    return true;
}

bool standard_form_motive_match_scaled(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                       long long numerator, long long denominator, StandardFormMotiveMatch *out) {
    if (out == NULL || !comparable(prototype, candidate) || numerator <= 0 || denominator <= 0 || numerator == denominator ||
        memcmp(prototype->steps, candidate->steps, prototype->intervals * sizeof(int)) != 0) {
        return false;
    }
    StandardFormQ factor = standard_form_q(numerator, denominator);
    for (size_t i = 0; i < prototype->intervals; i++) {
        StandardFormQ scaled = standard_form_q(prototype->ioi[i].n * factor.n, prototype->ioi[i].d * factor.d);
        if (standard_form_q_compare(scaled, candidate->ioi[i]) != 0) {
            return false;
        }
    }
    full(out, factor.n > factor.d ? STANDARD_FORM_MOTIVE_AUGMENTED : STANDARD_FORM_MOTIVE_DIMINISHED, prototype->intervals);
    return true;
}

bool standard_form_motive_match_augmentation(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                             StandardFormMotiveMatch *out) {
    return standard_form_motive_match_scaled(prototype, candidate, 2, 1, out);
}

bool standard_form_motive_match_diminution(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                           StandardFormMotiveMatch *out) {
    return standard_form_motive_match_scaled(prototype, candidate, 1, 2, out);
}

bool standard_form_motive_match_varied(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                       size_t max_mismatch, StandardFormMotiveMatch *out) {
    if (out == NULL || !comparable(prototype, candidate) || prototype->intervals + 1 < 5) {
        return false;
    }
    size_t mismatches = 0;
    for (size_t i = 0; i < prototype->intervals; i++) {
        mismatches += prototype->steps[i] != candidate->steps[i] ? 1 : 0;
    }
    bool rhythm = same_rhythm(prototype, candidate);
    if (mismatches > max_mismatch || (mismatches == 0 && rhythm)) {
        return false; /* a full match is a pattern match, not a variation */
    }
    out->kind = (int)STANDARD_FORM_MOTIVE_VARIED;
    out->mismatches = (int)mismatches;
    out->intervals = prototype->intervals;
    out->rhythm_factor = rhythm ? 1.0 : 0.8;
    out->score = (1.0 - (double)mismatches / (double)prototype->intervals) * out->rhythm_factor;
    out->fragment_part = -1;
    out->fragment_intervals = 0;
    return true;
}

size_t standard_form_motive_fragment_min_intervals(size_t intervals) {
    size_t half = (intervals + 1) / 2;
    return half > 2 ? half : 2;
}

bool standard_form_motive_match_fragment(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                         StandardFormMotiveMatch *out) {
    if (out == NULL || prototype == NULL || candidate == NULL || prototype->generic != candidate->generic) {
        return false;
    }
    size_t whole = prototype->intervals;
    size_t length = candidate->intervals;
    if (length >= whole || length < standard_form_motive_fragment_min_intervals(whole)) {
        return false;
    }
    for (int part = 0; part < 2; part++) {
        size_t offset = part == 0 ? 0 : whole - length;
        bool same = true;
        for (size_t i = 0; same && i < length; i++) {
            same = candidate->steps[i] == prototype->steps[offset + i] &&
                   standard_form_q_compare(candidate->ioi[i], prototype->ioi[offset + i]) == 0;
        }
        if (same) {
            full(out, STANDARD_FORM_MOTIVE_FRAGMENTED, length);
            out->fragment_part = part;
            out->fragment_intervals = length;
            return true;
        }
    }
    return false;
}

bool standard_form_motive_classify_with(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                        size_t varied_max_mismatch, StandardFormRelationMatcher matcher, void *context,
                                        StandardFormMotiveMatch *out) {
    if (out == NULL) {
        return false;
    }
    /* the order is the precedence: the first that applies wins */
    if (matcher != NULL) {
        StandardFormMotiveKind kind;
        double score = 0.0;
        if (comparable(prototype, candidate) && matcher(prototype, candidate, &kind, &score, context) &&
            (kind == STANDARD_FORM_MOTIVE_EXACT || kind == STANDARD_FORM_MOTIVE_REAL || kind == STANDARD_FORM_MOTIVE_TONAL ||
             kind == STANDARD_FORM_MOTIVE_INVERTED || kind == STANDARD_FORM_MOTIVE_RETROGRADE) &&
            score > 0.0 && score <= 1.0) {
            full(out, kind, prototype->intervals);
            out->score = score;
            return true;
        }
        return standard_form_motive_match_augmentation(prototype, candidate, out) ||
               standard_form_motive_match_diminution(prototype, candidate, out) ||
               standard_form_motive_match_varied(prototype, candidate, varied_max_mismatch, out);
    }
    return standard_form_motive_match_pattern(prototype, candidate, out) ||
           standard_form_motive_match_inversion(prototype, candidate, out) ||
           standard_form_motive_match_retrograde(prototype, candidate, out) ||
           standard_form_motive_match_augmentation(prototype, candidate, out) ||
           standard_form_motive_match_diminution(prototype, candidate, out) ||
           standard_form_motive_match_varied(prototype, candidate, varied_max_mismatch, out);
}

bool standard_form_motive_classify(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                   size_t varied_max_mismatch, StandardFormMotiveMatch *out) {
    return standard_form_motive_classify_with(prototype, candidate, varied_max_mismatch, NULL, NULL, out);
}
