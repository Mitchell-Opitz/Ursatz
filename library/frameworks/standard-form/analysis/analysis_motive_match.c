#include "analysis_motive_match.h"

#include <string.h>

#include "core/rational.h"

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
        if (rational_cmp(a->ioi[i], b->ioi[i]) != 0) {
            return false;
        }
    }
    return true;
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
    out->kind = (int)MOTIVE_OCCURRENCE_VARIED;
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

bool standard_form_motive_classify_match(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                         size_t varied_max_mismatch, StandardFormRelationMatchFn match, void *context,
                                         StandardFormMotiveMatch *out) {
    if (out == NULL || match == NULL) {
        return false;
    }
    StandardFormRelationMatch found;
    if (comparable(prototype, candidate) && match(prototype, candidate, &found, context) && found.kind != MOTIVE_OCCURRENCE_FRAGMENTED &&
        found.kind != MOTIVE_OCCURRENCE_VARIED && motive_occurrence_kind_valid(found.kind) && found.score > 0.0 && found.score <= 1.0) {
        full(out, found.kind, prototype->intervals);
        out->score = found.score;
        return true;
    }
    return standard_form_motive_match_varied(prototype, candidate, varied_max_mismatch, out);
}

bool standard_form_motive_fragment_match(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                         StandardFormRelationMatchFn match, void *context, StandardFormMotiveMatch *out) {
    if (out == NULL || match == NULL || prototype == NULL || candidate == NULL || prototype->generic != candidate->generic ||
        candidate->intervals >= prototype->intervals ||
        candidate->intervals < standard_form_motive_fragment_min_intervals(prototype->intervals)) {
        return false;
    }
    StandardFormRelationMatch found;
    if (!match(prototype, candidate, &found, context) || found.kind != MOTIVE_OCCURRENCE_FRAGMENTED || !(found.score > 0.0) ||
        found.score > 1.0 || (found.fragment_part != 0 && found.fragment_part != 1) || found.fragment_intervals != candidate->intervals) {
        return false;
    }
    full(out, MOTIVE_OCCURRENCE_FRAGMENTED, candidate->intervals);
    out->score = found.score;
    out->fragment_part = found.fragment_part;
    out->fragment_intervals = found.fragment_intervals;
    return true;
}
