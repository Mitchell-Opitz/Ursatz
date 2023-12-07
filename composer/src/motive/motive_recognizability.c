#include <ursatz_composer/motive_develop.h>

#include <string.h>

#include "motive/motive_internal.h"

/*
 * Recognizability. The formula, the parts and the default weights are described with motive_recognizability in
 * motive_develop.h; this is the arithmetic. All of it is exact: counts are small whole numbers, ratios of durations are
 * kept as fractions, and the score is a fraction in lowest terms.
 */

typedef struct {
    int direction[MOTIVE_MAX_NOTES]; /* -1, 0 or 1 */
    int interval_class[MOTIVE_MAX_NOTES];
    PlanTime ratio[MOTIVE_MAX_NOTES];
    size_t intervals; /* note_count - 1 */
    size_t ratios;    /* the same */
} Features;

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

static MotiveScore make_score(unsigned long long numerator, unsigned long long denominator) {
    MotiveScore score;
    unsigned long long divisor = gcd_ull(numerator, denominator);
    if (divisor == 0) {
        divisor = 1;
    }
    score.numerator = (unsigned long)(numerator / divisor);
    score.denominator = (unsigned long)(denominator / divisor);
    return score;
}

static void features_of(const Motive *motive, Features *out) {
    memset(out, 0, sizeof(*out));
    size_t count = motive->note_count;
    out->intervals = count - 1;
    out->ratios = count - 1;
    for (size_t i = 0; i + 1 < count; i++) {
        int semitones = motive->notes[i + 1].midi_pitch - motive->notes[i].midi_pitch;
        int size = (semitones < 0 ? -semitones : semitones) % 12;
        out->direction[i] = semitones > 0 ? 1 : (semitones < 0 ? -1 : 0);
        out->interval_class[i] = size > 6 ? 12 - size : size;
    }
    /* The inter-onset interval of each note; the last note lasts as long as it sounds. */
    PlanTime ioi[MOTIVE_MAX_NOTES];
    for (size_t i = 0; i < count; i++) {
        ioi[i] = i + 1 < count ? motive_time_minus(motive->notes[i + 1].offset, motive->notes[i].offset)
                               : motive->notes[i].duration;
    }
    for (size_t i = 0; i + 1 < count; i++) {
        out->ratio[i] = motive_time(ioi[i + 1].num * (long)ioi[i].den, ioi[i + 1].den * (unsigned long)ioi[i].num);
    }
}

/* A part when one or both have nothing to compare: 1 when both have nothing, 0 when only one has. */
static bool empty_part(size_t a, size_t b, MotiveScore *out) {
    if (a == 0 || b == 0) {
        *out = make_score(a == b ? 1 : 0, 1);
        return true;
    }
    return false;
}

static size_t longer(size_t a, size_t b) {
    return a > b ? a : b;
}

/* The length of the longest run the two sequences have in common. equal(i, j) says whether a[i] is b[j]. */
typedef bool (*EqualFn)(const Features *a, size_t i, const Features *b, size_t j);

static size_t longest_common_run(const Features *a, size_t a_count, const Features *b, size_t b_count, EqualFn equal) {
    size_t best = 0;
    for (size_t i = 0; i < a_count; i++) {
        for (size_t j = 0; j < b_count; j++) {
            size_t run = 0;
            while (i + run < a_count && j + run < b_count && equal(a, i + run, b, j + run)) {
                run++;
            }
            best = run > best ? run : best;
        }
    }
    return best;
}

static bool same_direction(const Features *a, size_t i, const Features *b, size_t j) {
    return a->direction[i] == b->direction[j];
}

static bool same_ratio(const Features *a, size_t i, const Features *b, size_t j) {
    return plan_time_compare(a->ratio[i], b->ratio[j]) == 0;
}

static MotiveScore contour_part(const Features *a, const Features *b) {
    MotiveScore score;
    if (empty_part(a->intervals, b->intervals, &score)) {
        return score;
    }
    return make_score(longest_common_run(a, a->intervals, b, b->intervals, same_direction),
                      longer(a->intervals, b->intervals));
}

static MotiveScore intervals_part(const Features *a, const Features *b) {
    MotiveScore score;
    if (empty_part(a->intervals, b->intervals, &score)) {
        return score;
    }
    size_t counts_a[7] = {0};
    size_t counts_b[7] = {0};
    for (size_t i = 0; i < a->intervals; i++) {
        counts_a[a->interval_class[i]]++;
    }
    for (size_t i = 0; i < b->intervals; i++) {
        counts_b[b->interval_class[i]]++;
    }
    size_t shared = 0;
    for (int c = 0; c <= 6; c++) {
        shared += counts_a[c] < counts_b[c] ? counts_a[c] : counts_b[c];
    }
    return make_score(shared, longer(a->intervals, b->intervals));
}

static MotiveScore rhythm_part(const Features *a, const Features *b) {
    MotiveScore score;
    if (empty_part(a->ratios, b->ratios, &score)) {
        return score;
    }
    return make_score(longest_common_run(a, a->ratios, b, b->ratios, same_ratio), longer(a->ratios, b->ratios));
}

MotiveScoreWeights motive_score_default_weights(void) {
    MotiveScoreWeights weights = {3, 2, 2, 1};
    return weights;
}

bool motive_recognizability(const Motive *source, const Motive *development, const MotiveScoreWeights *weights,
                            MotiveRecognizability *out) {
    MotiveScoreWeights used = weights ? *weights : motive_score_default_weights();
    unsigned long long total_weight = (unsigned long long)used.contour + used.intervals + used.rhythm + used.length;
    if (!source || !development || !out || total_weight == 0 || source->note_count == 0 ||
        development->note_count == 0) {
        return false;
    }
    Features a;
    Features b;
    features_of(source, &a);
    features_of(development, &b);

    MotiveRecognizability result;
    result.contour = contour_part(&a, &b);
    result.intervals = intervals_part(&a, &b);
    result.rhythm = rhythm_part(&a, &b);
    size_t shorter = source->note_count < development->note_count ? source->note_count : development->note_count;
    result.length = make_score(shorter, longer(source->note_count, development->note_count));

    /* The weighted sum of the four parts, as one fraction over a common denominator. */
    const MotiveScore *parts[4] = {&result.contour, &result.intervals, &result.rhythm, &result.length};
    const unsigned long long weight[4] = {used.contour, used.intervals, used.rhythm, used.length};
    unsigned long long numerator = 0;
    unsigned long long denominator = 1;
    for (int i = 0; i < 4; i++) {
        numerator = numerator * parts[i]->denominator + weight[i] * parts[i]->numerator * denominator;
        denominator *= parts[i]->denominator;
        MotiveScore reduced = make_score(numerator, denominator);
        numerator = reduced.numerator;
        denominator = reduced.denominator;
    }
    result.total = make_score(numerator, denominator * total_weight);
    *out = result;
    return true;
}
