#include "style/style_internal.h"

#include <limits.h>
#include <string.h>

#include <phrase_beginning.h>
#include <phrase_ending.h>
#include <phrase_level_form.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_prolong.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"

/* Chords of the harmony lane read back from before a target: no more than this many spans are looked at. */
#define BEFORE_SPANS 64
/* ...from this many bars back. */
#define BEFORE_BARS 8

/*
 * The numeral of the chord written right before a span: a span of the harmony lane, not open, that ends where the
 * span starts. False when the span starts the plan or the chord before is still open.
 */
static bool chord_before(const PiecePlan *plan, PlanSpan span, PlanTime bar, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (span.start.num <= 0 || bar.num <= 0) {
        return false;
    }
    PlanTime back = {-bar.num * BEFORE_BARS, bar.den};
    PlanTime from;
    if (plan_time_add(span.start, back, &from) != PLAN_OK) {
        return false;
    }
    if (from.num < 0) {
        from.num = 0;
        from.den = 1;
    }
    PlanSpan window = {from, {0, 1}};
    if (plan_time_make(span.start.num * (long)from.den - from.num * (long)span.start.den, span.start.den * from.den,
                       &window.length) != PLAN_OK ||
        window.length.num <= 0) {
        return false;
    }
    LaneSpanId ids[BEFORE_SPANS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, window, ids, BEFORE_SPANS);
    if (found == 0 || found > BEFORE_SPANS) {
        return false;
    }
    PlanSpan last = plan_lane_span_span(plan, ids[found - 1]);
    PlanTime end;
    if (plan_span_end(last, &end) != PLAN_OK || plan_time_compare(end, span.start) != 0 ||
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

static bool melody_labels(const MelodyCandidate *melody, ComposerStyleLabels *labels) {
    ComposerStyleNote notes[MELODY_MAX_NOTES];
    if (!melody || melody->magic != MELODY_PAYLOAD_MAGIC || melody->count > MELODY_MAX_NOTES) {
        return false;
    }
    for (size_t i = 0; i < melody->count; i++) {
        notes[i].pitch = melody->notes[i].pitch;
        notes[i].duration_numerator = (unsigned long)melody->notes[i].duration;
        notes[i].duration_denominator = (unsigned long)MELODY_TICKS;
    }
    const MelodyContext *context = &melody->context;
    int before = context->lead_in_count > 0 ? context->lead_in[context->lead_in_count - 1] : 0;
    return composer_style_labels_add_melody(labels, context->lead_in_count > 0 ? &before : NULL, notes, melody->count);
}

/* Whether the plan fixes the form of the section: a theme pairing constraint says how it begins and ends. */
static bool form_fixed(const PiecePlan *plan, FillTarget target) {
    if (target.kind != FILL_TARGET_NODE) {
        return true;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        if (plan_node_constraint_at(plan, target.id, i)->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
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

__extension__ typedef unsigned __int128 Wide;

static Wide wide_gcd(Wide a, Wide b) {
    while (b) {
        Wide rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

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

/* --------------------------------------------------------------- checker */

bool style_stage_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out) {
    const StyleCheckContext *style = context;
    if (!style || !style->inner || !style->style || !plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (!style->inner(plan, target, operator_, candidate, NULL, out)) {
        return false;
    }
    out->style_numerator = 0;
    out->style_denominator = 1;
    if (out->hard_violation) {
        return true;
    }
    const char *name = operator_->generator.name;
    /* The structure stage prefers a form only where the plan has not fixed it: a form every candidate shares would
       add the same to each of them and only change the score the structure is reported with. */
    bool theme = strcmp(name, GROW_THEME_OPERATOR_NAME) == 0;
    if (theme && form_fixed(plan, target)) {
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
        ok = melody_labels(candidate->payload, labels);
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
