#include "analysis/form/analysis_similarity.h"

#include <math.h>

#include "model/core/rational.h"
#include "model/time/score_time.h"

bool standard_form_idea_similarity(const AnalysisNote *a, size_t a_count, const AnalysisNote *b, size_t b_count,
                                   double similarity_min, StandardFormSimilarity *out) {
    if (out == NULL || (a_count > 0 && a == NULL) || (b_count > 0 && b == NULL) || !isfinite(similarity_min) ||
        !(similarity_min > 0.0 && similarity_min <= 1.0)) {
        return false;
    }
    StandardFormSimilarity result = {false, 0, 0, 0.0, false, 0.0};
    if (a_count >= 2 && b_count >= 2) {
        size_t a_intervals = a_count - 1;
        size_t b_intervals = b_count - 1;
        size_t shared = a_intervals < b_intervals ? a_intervals : b_intervals;
        result.comparable = true;
        result.positions = a_intervals > b_intervals ? a_intervals : b_intervals;
        for (size_t i = 0; i < shared; i++) {
            Rational a_from;
            Rational a_to;
            Rational b_from;
            Rational b_to;
            a_from = a[i].span.start.value;
            a_to = a[i + 1].span.start.value;
            b_from = b[i].span.start.value;
            b_to = b[i + 1].span.start.value;
            bool same_interval = a[i + 1].midi - a[i].midi == b[i + 1].midi - b[i].midi;
            Rational a_length;
            Rational b_length;
            if (rational_sub(a_to, a_from, &a_length) != RATIONAL_OK || rational_sub(b_to, b_from, &b_length) != RATIONAL_OK) {
                return false;
            }
            bool same_rhythm = rational_cmp(a_length, b_length) == 0;
            if (same_interval && same_rhythm) {
                result.matched++;
            }
        }
        result.similarity = (double)result.matched / (double)result.positions;
        result.match = result.similarity >= similarity_min;
        double margin = result.match ? (similarity_min >= 1.0 ? 1.0 : (result.similarity - similarity_min) / (1.0 - similarity_min))
                                     : (similarity_min - result.similarity) / similarity_min;
        result.margin = margin < 0.0 ? 0.0 : (margin > 1.0 ? 1.0 : margin);
    }
    *out = result;
    return true;
}

double standard_form_similarity_uncertainty(const StandardFormSimilarity *similarity) {
    if (similarity == NULL || !similarity->comparable) {
        return 0.5;
    }
    return 0.5 * (1.0 - similarity->margin);
}
