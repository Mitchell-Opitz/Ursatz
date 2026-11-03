#include "style/style_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <standard-form/hybrid_rules.h>
#include <standard-tonality/tonal_key.h>
#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_prolong.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"
#include "texture/texture_internal.h"

/* Chords of the harmony lane read back from before a target: no more than this many spans are looked at. */
#define BEFORE_SPANS 64
/* ...from this many bars back. */
#define BEFORE_BARS 8

/*
 * The numeral of the chord written right before a span: a span of the harmony lane, not open, that ends where the
 * span starts. False when the span starts the plan or the chord before is still open.
 */
static bool chord_before(const PiecePlan *plan, PlanSpan span, Rational bar, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (span.start.num <= 0 || bar.num <= 0) {
        return false;
    }
    Rational back = {-bar.num * BEFORE_BARS, bar.den};
    Rational from;
    if (rational_add(span.start, back, &from) != RATIONAL_OK) {
        return false;
    }
    if (from.num < 0) {
        from.num = 0;
        from.den = 1;
    }
    PlanSpan window = {from, {0, 1}};
    if (rational_sub(span.start, from, &window.length) != RATIONAL_OK ||
        window.length.num <= 0) {
        return false;
    }
    LaneSpanId ids[BEFORE_SPANS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, window, ids, BEFORE_SPANS);
    if (found == 0 || found > BEFORE_SPANS) {
        return false;
    }
    PlanSpan last = plan_lane_span_span(plan, ids[found - 1]);
    Rational end;
    if (plan_span_end(last, &end) != PLAN_OK || rational_cmp(end, span.start) != 0 ||
        plan_lane_span_state(plan, ids[found - 1]) == PLAN_STATE_CONSTRAINED) {
        return false;
    }
    const char *roman = plan_lane_span_payload(plan, ids[found - 1])->harmony.roman_numeral;
    if (!roman[0]) {
        return false;
    }
    memcpy(out, roman, PLAN_ROMAN_NUMERAL_MAX);
    return true;
}

static bool harmony_labels(const PiecePlan *plan, const PhraseProgression *progression, ComposerStyleLabels *labels) {
    const char *numerals[HARMONY_MAX_CHORDS];
    char before[PLAN_ROMAN_NUMERAL_MAX];
    if (!progression || progression->count > HARMONY_MAX_CHORDS) {
        return false;
    }
    bool has_before = chord_before(plan, progression->request.span, progression->request.bar_length, before);
    for (size_t i = 0; i < progression->count; i++) {
        numerals[i] = progression->chords[i].roman;
    }
    return composer_style_labels_add_harmony(labels, has_before ? before : NULL, numerals, progression->count);
}

/* The degree label of a pitch in the key. */
static const char *degree_of(const AnalysisKey *key, int pitch) {
    return standard_tonality_chromatic_degree_label(pitch - analysis_key_tonic_pitch_class(key));
}

/* A `motive-nct-figure` label for every non-chord tone the melody writes: the degrees of the note and its neighbors. */
static bool nct_labels(const MelodyCandidate *melody, ComposerStyleLabels *labels) {
    static const struct {
        unsigned bit;
        const char *name;
    } TYPES[] = {{MELODY_NCT_PASSING, "passing"},
                 {MELODY_NCT_NEIGHBOR, "neighbor"},
                 {MELODY_NCT_APPOGGIATURA, "appoggiatura"},
                 {MELODY_NCT_SUSPENSION, "suspension"},
                 {MELODY_NCT_ANTICIPATION, "anticipation"}};
    unsigned tones[MELODY_MAX_NOTES];
    if (!melody_nct_types(melody, tones)) {
        return false;
    }
    const MelodyContext *context = &melody->context;
    const AnalysisKey *key = &context->key;
    for (size_t i = 0; i < melody->count; i++) {
        const char *name = NULL;
        for (size_t t = 0; t < sizeof TYPES / sizeof *TYPES; t++) {
            if (tones[i] & TYPES[t].bit) {
                name = TYPES[t].name;
                break;
            }
        }
        if (!name) {
            continue;
        }
        const char *previous = NULL;
        const char *next = NULL;
        if (i > 0) {
            previous = degree_of(key, melody->notes[i - 1].pitch);
        } else if (context->lead_in_count > 0) {
            previous = degree_of(key, context->lead_in[context->lead_in_count - 1]);
        }
        if (i + 1 < melody->count) {
            next = degree_of(key, melody->notes[i + 1].pitch);
        } else if (context->has_lead_out) {
            next = degree_of(key, context->lead_out);
        }
        if (!composer_style_labels_add_nct(labels, previous, degree_of(key, melody->notes[i].pitch), name, next)) {
            return false;
        }
    }
    return true;
}

/* Whether the style counts a category (it was not read without it, and its corpus gave it something). */
static bool style_has_category(const ComposerStyle *style, const char *category) {
    ComposerStyleShare share;
    return composer_style_share(style, category, "", &share);
}

static bool melody_labels(const ComposerStyle *style, const MelodyCandidate *melody, ComposerStyleLabels *labels) {
    ComposerStyleNote notes[MELODY_MAX_NOTES];
    if (!melody || melody->magic != MELODY_PAYLOAD_MAGIC || melody->count > MELODY_MAX_NOTES) {
        return false;
    }
    for (size_t i = 0; i < melody->count; i++) {
        notes[i].pitch = melody->notes[i].pitch;
        notes[i].duration_numerator = (unsigned long)melody->notes[i].duration.num;
        notes[i].duration_denominator = (unsigned long)melody->notes[i].duration.den;
    }
    const MelodyContext *context = &melody->context;
    int before = context->lead_in_count > 0 ? context->lead_in[context->lead_in_count - 1] : 0;
    if (!composer_style_labels_add_melody(labels, context->lead_in_count > 0 ? &before : NULL, notes, melody->count)) {
        return false;
    }
    return !style_has_category(style, STYLE_CATEGORY_MOTIVE_NCT_FIGURE) || nct_labels(melody, labels);
}

/* The key the plan starts in, for a motive that carries none. */
static int plan_tonic(const PiecePlan *plan) {
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    LaneSpanId first;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, &first, 1) == 0) {
        return 0;
    }
    return analysis_key_tonic_pitch_class(&plan_lane_span_payload(plan, first)->key);
}

/* The motive labels of a motive-form or development candidate the motives stage offers. */
static bool motive_labels(const PiecePlan *plan, const FillCandidate *candidate, ComposerStyleLabels *labels) {
    const Motive *motive = grow_form_candidate_motive(candidate);
    if (!motive) {
        motive = motive_develop_candidate_motive(candidate);
    }
    if (!motive) {
        return true;
    }
    int tonic = motive->key_source != MOTIVE_KEY_UNSPECIFIED ? analysis_key_tonic_pitch_class(&motive->key) : plan_tonic(plan);
    return composer_style_labels_add_motive(labels, motive, tonic);
}

/*
 * Whether the plan fixes the form of the section: a theme pairing constraint says how it begins and ends. A free phrase
 * (R-170) has no form of a theme to prefer, so the style leaves it alone.
 */
static bool form_fixed(const PiecePlan *plan, FillTarget target) {
    if (target.kind != FILL_TARGET_NODE) {
        return true;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        PlanConstraintKind kind = plan_node_constraint_at(plan, target.id, i)->kind;
        if (kind == PLAN_CONSTRAINT_THEME_PAIRING || kind == PLAN_CONSTRAINT_FREE_PHRASE) {
            return true;
        }
    }
    return false;
}

/* The form labels of a planned theme: its form's name, and a phrase a period's half or otherwise the whole of it. */
static bool form_labels(const GrowThemeSpec *spec, ComposerStyleLabels *labels) {
    PhraseLevelForm form;
    if (!spec || spec->magic != GROW_PAYLOAD_MAGIC ||
        !standard_form_pairing_form(grow_phrase_beginning(spec->pairing.beginning),
                                    grow_phrase_ending(spec->pairing.ending), &form)) {
        return false;
    }
    unsigned bars[2] = {(unsigned)spec->bars, 0};
    size_t phrases = 1;
    if (spec->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        bars[0] = bars[1] = (unsigned)(spec->bars / 2);
        phrases = 2;
    }
    return composer_style_labels_add_form(labels, phrase_level_form_name(form), bars, phrases);
}

/* ---------------------------------------------------------------- score */


#define SCORE_LIMIT ((Wide)1 << 32)

void style_add_to_score(FillCheck *check, ComposerRatio closeness, ComposerRatio weight) {
    if (closeness.denominator == 0 || weight.denominator == 0 || check->score_denominator == 0) {
        return;
    }
    /* n/d + (wn * cn) / (wd * cd) over the common denominator d * wd * cd, in 128 bits: each factor is below 2^64, so
       two products of three do not fit; the check's own parts are below 2^32, which leaves room for all but the
       largest weights, and those are rounded below. */
    Wide d = check->score_denominator;
    Wide n = check->score_numerator;
    Wide product_den, product_num, scaled_n, numerator;
    bool fits = !__builtin_mul_overflow((Wide)weight.denominator, (Wide)closeness.denominator, &product_den) &&
                !__builtin_mul_overflow((Wide)weight.numerator, (Wide)closeness.numerator, &product_num) &&
                !__builtin_mul_overflow(n, product_den, &scaled_n) &&
                !__builtin_mul_overflow(d, product_den, &product_den) &&
                !__builtin_mul_overflow(d, product_num, &product_num) &&
                !__builtin_add_overflow(scaled_n, product_num, &numerator);
    Wide denominator = product_den;
    if (!fits) {
        /* Past 128 bits: both parts of each fraction to 30 bits first. */
        long double value = (long double)check->score_numerator / (long double)check->score_denominator +
                            ((long double)weight.numerator / (long double)weight.denominator) *
                                ((long double)closeness.numerator / (long double)closeness.denominator);
        long double capped = value > 4000.0L ? 4000.0L : value;
        denominator = (Wide)1 << 20;
        numerator = (Wide)(capped * (long double)denominator + 0.5L);
    }
    Wide divisor = wide_gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    while (numerator >= SCORE_LIMIT || denominator >= SCORE_LIMIT) {
        numerator >>= 1;
        denominator >>= 1;
    }
    if (denominator == 0) {
        denominator = 1;
    }
    check->score_numerator = (unsigned long)numerator;
    check->score_denominator = (unsigned long)denominator;
}

/* The term style_add_to_score adds, weight * closeness, in lowest terms and below 2^32 as the score is. */
static void style_term(ComposerRatio closeness, ComposerRatio weight, FillCheck *check) {
    check->style_numerator = 0;
    check->style_denominator = 1;
    if (closeness.denominator == 0 || weight.denominator == 0) {
        return;
    }
    Wide numerator = (Wide)weight.numerator * (Wide)closeness.numerator;
    Wide denominator = (Wide)weight.denominator * (Wide)closeness.denominator;
    Wide divisor = wide_gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    while (numerator >= SCORE_LIMIT || denominator >= SCORE_LIMIT) {
        numerator >>= 1;
        denominator >>= 1;
    }
    check->style_numerator = (unsigned long)numerator;
    check->style_denominator = denominator ? (unsigned long)denominator : 1;
}

/*
 * How many times an accompaniment pattern sounds in a beat of the meter, times 2: block and oom-pah once, the
 * arpeggiated figure once an eighth and the Alberti figure twice a beat.
 */
static unsigned long pattern_halves(PlanAccompanimentPattern pattern, Rational beat) {
    switch (pattern) {
    case PLAN_ACCOMPANIMENT_ARPEGGIATED:
        return 2UL * (unsigned long)position_floor_units(beat, ACCOMP_EIGHTH);
    case PLAN_ACCOMPANIMENT_ALBERTI:
        return 4;
    case PLAN_ACCOMPANIMENT_ANY:
    case PLAN_ACCOMPANIMENT_BLOCK:
    case PLAN_ACCOMPANIMENT_OOM_PAH:
    case PLAN_ACCOMPANIMENT_MOTIVE:
        break;
    }
    return 2;
}

/*
 * The accompaniment's closeness to the style's note density: min(rate, d) / max(rate, d), rate the pattern's attacks per
 * beat and d the style's. False when the style has no density or the run did not leave the pattern to the operator.
 */
static bool accompaniment_closeness(const ComposerStyle *style, const PiecePlan *plan, FillTarget target,
                                    const FillCandidate *candidate, ComposerRatio *out) {
    ComposerRatio density;
    const AccompCandidate *accompaniment = candidate->payload;
    AccompContext *read = calloc(1, sizeof(*read));
    if (!read) {
        return false;
    }
    bool found = accompaniment && accompaniment->magic == ACCOMP_PAYLOAD_MAGIC &&
                 composer_style_density(style, &density) && accomp_read(plan, (LaneSpanId)target.id, read) &&
                 read->pattern == PLAN_ACCOMPANIMENT_ANY;
    if (found) {
        unsigned long rate = pattern_halves(accompaniment->pattern, read->beat);
        unsigned long wanted = density.numerator;
        unsigned long low = rate < wanted ? rate : wanted;
        unsigned long high = rate < wanted ? wanted : rate;
        out->numerator = low;
        out->denominator = high ? high : 1;
    }
    free(read);
    return found;
}

/* --------------------------------------------------------------- checker */

bool style_stage_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out) {
    const StyleCheckContext *style = context;
    if (!style || !style->inner || !style->style || !plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (!style->inner(plan, target, operator_, candidate, style->inner_context, out)) {
        return false;
    }
    out->style_numerator = 0;
    out->style_denominator = 1;
    if (out->hard_violation) {
        return true;
    }
    const char *name = operator_->generator.name;
    if (style->texture) {
        ComposerRatio closeness;
        if (strcmp(name, ACCOMPANIMENT_OPERATOR_NAME) == 0 &&
            accompaniment_closeness(style->style, plan, target, candidate, &closeness)) {
            style_term(closeness, style->weight, out);
            style_add_to_score(out, closeness, style->weight);
        }
        return true;
    }
    /* The structure stage prefers a form only where the plan has not fixed it: a form every candidate shares would
       add the same to each of them and only change the score the structure is reported with. */
    bool theme = strcmp(name, GROW_THEME_OPERATOR_NAME) == 0;
    if (theme && form_fixed(plan, target)) {
        return true;
    }
    bool motive_operator = strcmp(name, GROW_FORM_OPERATOR_NAME) == 0 || strcmp(name, MOTIVE_RESTATE_OPERATOR_NAME) == 0 ||
                           strcmp(name, MOTIVE_FRAGMENT_OPERATOR_NAME) == 0 || strcmp(name, MOTIVE_RHYTHM_OPERATOR_NAME) == 0;
    /* A style that counts no motive category leaves the motives stage scoring as it does without a style. */
    if (motive_operator && !style_has_category(style->style, STYLE_CATEGORY_MOTIVE_DEGREES) &&
        !style_has_category(style->style, STYLE_CATEGORY_MOTIVE_INTERVALS) &&
        !style_has_category(style->style, STYLE_CATEGORY_MOTIVE_RHYTHM)) {
        return true;
    }
    ComposerStyleLabels *labels = composer_style_labels_create();
    if (!labels) {
        return false;
    }
    bool ok = true;
    if (theme) {
        ok = form_labels(candidate->payload, labels);
    } else if (strcmp(name, HARMONY_PHRASE_OPERATOR_NAME) == 0 || strcmp(name, HARMONY_PROLONG_OPERATOR_NAME) == 0) {
        ok = harmony_labels(plan, candidate->payload, labels);
    } else if (strcmp(name, MELODY_FILL_OPERATOR_NAME) == 0) {
        ok = melody_labels(style->style, candidate->payload, labels);
    } else if (motive_operator) {
        ok = motive_labels(plan, candidate, labels);
    }
    ComposerRatio closeness;
    if (ok && composer_style_closeness(style->style, labels, &closeness)) {
        style_term(closeness, style->weight, out);
        style_add_to_score(out, closeness, style->weight);
    } else {
        ok = false;
    }
    composer_style_labels_destroy(labels);
    return ok;
}
