#include "style/style_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/hybrid_rules.h>
#include <theory/tonality/tonal_key.h>
#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_prolong.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive_develop.h>
#include <plan/plan_state.h>

#include "grow/grow_internal.h"
#include "util/plan_util.h"
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
static bool chord_before(const Plan *plan, PlanSpan span, Rational bar, char out[PLAN_ROMAN_NUMERAL_MAX]) {
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

static bool harmony_labels(const Plan *plan, const PhraseProgression *progression, ComposerStyleLabels *labels) {
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
static const char *degree_of(const SpelledKey *key, int pitch) {
    return standard_tonality_chromatic_degree_label(pitch - spelled_key_tonic_pitch_class(key));
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
    const SpelledKey *key = &context->key;
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
static int plan_tonic(const Plan *plan) {
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    LaneSpanId first;
    if (plan_lane_find(plan, PLAN_LANE_REGION, 0, everything, &first, 1) == 0) {
        return 0;
    }
    return spelled_key_tonic_pitch_class(&plan_lane_span_payload(plan, first)->region);
}

/* The motive labels of a motive-form or development candidate the motives stage offers. */
static bool motive_labels(const Plan *plan, const FillCandidate *candidate, ComposerStyleLabels *labels) {
    const Motive *motive = grow_form_candidate_motive(candidate);
    if (!motive) {
        motive = motive_develop_candidate_motive(candidate);
    }
    if (!motive) {
        return true;
    }
    int tonic = motive->has_scale ? motive->tonic_pc : plan_tonic(plan);
    return composer_style_labels_add_motive(labels, motive, tonic);
}

/*
 * Whether the plan fixes the form of the section: a theme pairing constraint says how it begins and ends. A free phrase
 * (R-170), a liquidation (R-494), a retransition and a coda (R-503) have no form of a theme to prefer, so the style leaves them alone.
 */
static bool form_fixed(const Plan *plan, FillTarget target) {
    if (target.kind != FILL_TARGET_NODE) {
        return true;
    }
    SectionFunction function;
    if (plan_section_function_of(plan, target.id, &function) &&
        (function.function == SECTION_FUNCTION_RETRANSITION || function.function == SECTION_FUNCTION_CODA)) {
        return true;
    }
    const SectionDescription *section = plan_section_or_empty(plan, target.id);
    return section->has_pairing || section->free_phrase || (section->has_role && section->role == SECTION_ROLE_LIQUIDATION);
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
    if (spec->pairing.beginning == SECTION_BEGINNING_ANTECEDENT) {
        bars[0] = bars[1] = (unsigned)(spec->bars / 2);
        phrases = 2;
    }
    return composer_style_labels_add_form(labels, phrase_level_form_name(form), bars, phrases);
}

/* ---------------------------------------------------------------- score */


#define SCORE_LIMIT (1ULL << 32)

/* A fraction below 2^32 on both sides: shifted down together while a part is at or above the limit. */
static void below_score_limit(unsigned long long *numerator, unsigned long long *denominator) {
    while (*numerator >= SCORE_LIMIT || *denominator >= SCORE_LIMIT) {
        *numerator >>= 1;
        *denominator >>= 1;
    }
    if (*denominator == 0) {
        *denominator = 1;
    }
}

/* The value as a fraction over 2^20, for a sum or product that does not fit 64 bits (the value is capped at 4000). */
static Rational approximate_value(long double value) {
    long double capped = value > 4000.0L ? 4000.0L : value;
    long long scale = 1LL << 20;
    return rational_ratio((long long)(capped * (long double)scale + 0.5L), scale);
}

void style_add_to_score(FillCheck *check, Rational closeness, Rational weight) {
    if (closeness.den == 0 || weight.den == 0 || check->score_denominator == 0) {
        return;
    }
    /* n/d + (wn * cn) / (wd * cd): the check's own parts are below 2^32; a weight or closeness that makes the sum overflow 64 bits is
       rounded to a fraction over 2^20 instead. */
    Rational score = {(long long)check->score_numerator, (long long)check->score_denominator};
    Rational term;
    Rational sum;
    if (rational_mul(weight, closeness, &term) != RATIONAL_OK || rational_add(score, term, &sum) != RATIONAL_OK) {
        sum = approximate_value((long double)check->score_numerator / (long double)check->score_denominator +
                                ((long double)weight.num / (long double)weight.den) *
                                    ((long double)closeness.num / (long double)closeness.den));
    }
    unsigned long long numerator = (unsigned long long)sum.num;
    unsigned long long denominator = (unsigned long long)sum.den;
    below_score_limit(&numerator, &denominator);
    check->score_numerator = (unsigned long)numerator;
    check->score_denominator = (unsigned long)denominator;
}

/* The term style_add_to_score adds, weight * closeness, in lowest terms and below 2^32 as the score is. */
static void style_term(Rational closeness, Rational weight, FillCheck *check) {
    check->style_numerator = 0;
    check->style_denominator = 1;
    if (closeness.den == 0 || weight.den == 0) {
        return;
    }
    Rational term;
    if (rational_mul(weight, closeness, &term) != RATIONAL_OK) {
        term = approximate_value(((long double)weight.num / (long double)weight.den) * ((long double)closeness.num / (long double)closeness.den));
    }
    unsigned long long numerator = (unsigned long long)term.num;
    unsigned long long denominator = (unsigned long long)term.den;
    below_score_limit(&numerator, &denominator);
    check->style_numerator = (unsigned long)numerator;
    check->style_denominator = (unsigned long)denominator;
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
static bool accompaniment_closeness(const ComposerStyle *style, const Plan *plan, FillTarget target,
                                    const FillCandidate *candidate, Rational *out) {
    Rational density;
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
        unsigned long wanted = density.num;
        unsigned long low = rate < wanted ? rate : wanted;
        unsigned long high = rate < wanted ? wanted : rate;
        out->num = low;
        out->den = high ? high : 1;
    }
    free(read);
    return found;
}

/* --------------------------------------------------------------- checker */

bool style_stage_checker(const Plan *plan, FillTarget target, const FillOperator *operator_,
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
        Rational closeness;
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
    Rational closeness;
    if (ok && composer_style_closeness(style->style, labels, &closeness)) {
        style_term(closeness, style->weight, out);
        style_add_to_score(out, closeness, style->weight);
    } else {
        ok = false;
    }
    composer_style_labels_destroy(labels);
    return ok;
}
