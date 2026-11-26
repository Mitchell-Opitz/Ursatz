#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/dimension/dimension.h>
#include <theory/knowledge/rule_entry.h>

#include "critic/critic_internal.h"

/*
 * The ten evaluators (R-719, R-755). Each is a penalty from 0 to 1 over the melody line, an exact Rational. The code of each, the name of
 * its function and its parameters are the data of the entry michelle.rule.<code>; the table below holds the first two, and a test
 * pins it to the entries both ways. Overflow of a Rational is CRITIC_ERR_OVERFLOW.
 */

static const struct {
    const char *code;
    const char *function;
} EVALUATORS[CRITIC_EVALUATOR_COUNT] = {
    {"rhythmic_monotony", "critic_rhythmic_monotony"}, {"contrast_distance", "critic_contrast_distance"},
    {"leap_load", "critic_leap_load"},                 {"open_ending", "critic_open_ending"},
    {"stray_syncopation", "critic_stray_syncopation"}, {"one_rhythm", "critic_one_rhythm"},
    {"repeated_notes", "critic_repeated_notes"},       {"wide_range", "critic_wide_range"},
    {"register_shift", "critic_register_shift"},       {"half_notes", "critic_half_notes"}};

const char *critic_evaluator_code(CriticEvaluator evaluator) {
    return (int)evaluator >= 0 && evaluator < CRITIC_EVALUATOR_COUNT ? EVALUATORS[evaluator].code : NULL;
}

const char *critic_evaluator_function(CriticEvaluator evaluator) {
    return (int)evaluator >= 0 && evaluator < CRITIC_EVALUATOR_COUNT ? EVALUATORS[evaluator].function : NULL;
}

bool critic_evaluator_from_code(const char *code, CriticEvaluator *out) {
    for (int i = 0; code != NULL && out != NULL && i < CRITIC_EVALUATOR_COUNT; i++) {
        if (strcmp(code, EVALUATORS[i].code) == 0) {
            *out = (CriticEvaluator)i;
            return true;
        }
    }
    return false;
}

void critic_settings_default(CriticSettings *settings) {
    memset(settings, 0, sizeof(*settings));
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        settings->weights[i] = 1;
    }
    settings->candidates = 4;
    settings->keep_rejected = 8;
}

/* ---------------------------------------------------------------------------------------- Rational helpers */

#define TRY(call)                          \
    do {                                   \
        if ((call) != RATIONAL_OK) {       \
            return CRITIC_ERR_OVERFLOW;    \
        }                                  \
    } while (0)

static const Rational ZERO = {0, 1};
static const Rational ONE = {1, 1};

static Rational whole(long long value) {
    return rational_ratio(value, 1);
}

/* num/den, both counts, as a Rational; den above 0. */
static CriticResult share(long long num, long long den, Rational *out) {
    TRY(rational_make(num, den, out));
    return CRITIC_OK;
}

/* min(1, max(0, value)). */
static Rational clamp01(Rational value) {
    if (rational_cmp(value, ZERO) < 0) {
        return ZERO;
    }
    return rational_cmp(value, ONE) > 0 ? ONE : value;
}

/* The parameter `name` of the evaluator's entry into *out; with no name it only checks that the entry is there. */
static CriticResult parameter(const KnowledgeSnapshot *knowledge, CriticEvaluator evaluator, const char *name, Rational *out) {
    char id[64];
    const char *code = EVALUATORS[evaluator].code;
    size_t length = 0;
    snprintf(id, sizeof(id), "michelle.rule.");
    length = strlen(id);
    for (size_t i = 0; code[i] != '\0' && length + 1 < sizeof(id); i++) {
        id[length++] = code[i] == '_' ? '-' : code[i];
    }
    id[length] = '\0';
    const RuleEntry *rule = knowledge_snapshot_rule(knowledge, id);
    if (rule == NULL || rule->subtype != RULE_EVALUATOR) {
        return CRITIC_ERR_KNOWLEDGE;
    }
    if (name == NULL) {
        return CRITIC_OK;
    }
    for (size_t i = 0; i < rule->parameter_count; i++) {
        if (strcmp(rule->parameters[i].name, name) == 0) {
            *out = rule->parameters[i].value;
            return CRITIC_OK;
        }
    }
    return CRITIC_ERR_KNOWLEDGE;
}

/* ------------------------------------------------------------------------------------------------ bars */

/* The line's bars that hold a note onset, in order: the run of notes that start in each. */
typedef struct {
    int bar;
    size_t first;
    size_t count;
} Bar;

static size_t line_bars(const CriticLine *line, Bar *bars) {
    size_t n = 0;
    for (size_t i = 0; i < line->count; i++) {
        if (n > 0 && bars[n - 1].bar == line->notes[i].bar) {
            bars[n - 1].count++;
        } else {
            bars[n].bar = line->notes[i].bar;
            bars[n].first = i;
            bars[n].count = 1;
            n++;
        }
    }
    return n;
}

/* Whether two bars have one rhythm: the same positions and durations of the notes that start in them. */
static bool same_rhythm(const CriticLine *a, const Bar *bar_a, const CriticLine *b, const Bar *bar_b) {
    if (bar_a->count != bar_b->count) {
        return false;
    }
    for (size_t i = 0; i < bar_a->count; i++) {
        const CriticNote *x = &a->notes[bar_a->first + i];
        const CriticNote *y = &b->notes[bar_b->first + i];
        if (!rational_equals(x->position, y->position) || !rational_equals(x->duration, y->duration)) {
            return false;
        }
    }
    return true;
}

static void begin(CriticFinding *finding, CriticEvaluator evaluator) {
    memset(finding, 0, sizeof(*finding));
    finding->evaluator = evaluator;
    finding->penalty = ZERO;
    finding->bar = -1;
    finding->beat = ZERO;
    finding->value = ZERO;
    finding->limit = ZERO;
}

/* ------------------------------------------------------------------------------------------ evaluators */

static CriticResult rhythmic_monotony(const CriticLine *line, const KnowledgeSnapshot *knowledge, CriticFinding *out) {
    Rational run_parameter;
    CriticResult status = parameter(knowledge, CRITIC_RHYTHMIC_MONOTONY, "run", &run_parameter);
    if (status != CRITIC_OK) {
        return status;
    }
    Bar *bars = calloc(line->count, sizeof(Bar));
    if (bars == NULL) {
        return CRITIC_ERR_ALLOC;
    }
    size_t n = line_bars(line, bars);
    long long run_min = run_parameter.num / run_parameter.den;
    long long counted = 0;
    long long run = 1;
    long long longest = 1;
    size_t longest_end = 0;
    for (size_t i = 1; i < n; i++) {
        run = same_rhythm(line, &bars[i - 1], line, &bars[i]) ? run + 1 : 1;
        counted += run >= run_min ? 1 : 0;
        if (run > longest) {
            longest = run;
            longest_end = i;
        }
    }
    Rational penalty = ZERO;
    if (n > 2) {
        status = share(counted, (long long)n - 2, &penalty);
    }
    /* A line whose notes but the last all have one duration is monotonous whatever its bars. */
    bool even = true;
    for (size_t i = 1; i + 1 < line->count; i++) {
        even = even && rational_equals(line->notes[i].duration, line->notes[0].duration);
    }
    if (status == CRITIC_OK && even) {
        penalty = ONE;
    }
    if (status == CRITIC_OK) {
        out->penalty = clamp01(penalty);
        out->first = bars[longest_end + 1 - (size_t)longest].bar;
        out->last = bars[longest_end].bar;
        out->count = (int)longest;
        out->value = whole(longest);
        out->limit = whole(run_min);
    }
    free(bars);
    return status;
}

CriticResult critic_pair_contrast(const CriticLine *line, const CriticIdeaPair *pair, ContrastVector *vector, bool *measured) {
    DimensionNote idea[MOTIVE_MAX_NOTES * 2];
    DimensionNote against[MOTIVE_MAX_NOTES * 2];
    DimensionSpan a;
    DimensionSpan b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    *measured = false;
    if (pair->idea_count == 0 || pair->against_count == 0 || pair->idea_count > MOTIVE_MAX_NOTES * 2 ||
        pair->against_count > MOTIVE_MAX_NOTES * 2) {
        return CRITIC_OK;
    }
    for (size_t i = 0; i < pair->idea_count; i++) {
        const CriticNote *note = &line->notes[pair->idea_first + i];
        TRY(rational_sub(note->onset, pair->idea_start, &idea[i].onset));
        idea[i].duration = note->duration;
        idea[i].pitch = note->pitch;
    }
    for (size_t i = 0; i < pair->against_count; i++) {
        const CriticNote *note = &line->notes[pair->against_first + i];
        TRY(rational_sub(note->onset, pair->against_start, &against[i].onset));
        against[i].duration = note->duration;
        against[i].pitch = note->pitch;
    }
    a.notes = against;
    a.count = pair->against_count;
    a.length = pair->against_length;
    a.beat = line->beat;
    b.notes = idea;
    b.count = pair->idea_count;
    b.length = pair->idea_length;
    b.beat = line->beat;
    if (dimension_contrast(&a, &b, vector) != DIMENSION_OK) {
        return CRITIC_OK; /* a span the measure cannot read is not held against the line */
    }
    *measured = true;
    return CRITIC_OK;
}

static CriticResult contrast_distance(const CriticLine *line, const KnowledgeSnapshot *knowledge, CriticFinding *out) {
    Rational least, register_limit, register_span;
    CriticResult status = parameter(knowledge, CRITIC_CONTRAST_DISTANCE, "least", &least);
    status = status == CRITIC_OK ? parameter(knowledge, CRITIC_CONTRAST_DISTANCE, "register", &register_limit) : status;
    status = status == CRITIC_OK ? parameter(knowledge, CRITIC_CONTRAST_DISTANCE, "register_span", &register_span) : status;
    if (status != CRITIC_OK) {
        return status;
    }
    Rational total = ZERO;
    Rational worst = ZERO;
    size_t measured_pairs = 0;
    for (size_t i = 0; i < line->pair_count; i++) {
        ContrastVector vector;
        bool measured = false;
        const CriticIdeaPair *pair = &line->pairs[i];
        status = critic_pair_contrast(line, pair, &vector, &measured);
        if (status != CRITIC_OK) {
            return status;
        }
        if (!measured) {
            continue;
        }
        Rational rhythm = vector.distance[CONTRAST_ASPECT_RHYTHM];
        Rational contour = vector.distance[CONTRAST_ASPECT_CONTOUR];
        Rational reach = vector.distance[CONTRAST_ASPECT_REGISTER];
        Rational penalty = ZERO;
        const char *aspect = "rhythm";
        Rational value = rhythm;
        Rational limit = least;
        if (rational_cmp(rhythm, least) < 0 && rational_cmp(contour, least) < 0) {
            Rational twice;
            TRY(rational_mul_int(rhythm, 2, &twice));
            TRY(rational_sub(ONE, twice, &penalty));
        }
        if (rational_cmp(reach, register_limit) > 0) {
            Rational over, far;
            TRY(rational_sub(reach, register_limit, &over));
            TRY(rational_div(over, register_span, &far));
            far = clamp01(far);
            if (rational_cmp(far, penalty) > 0) {
                penalty = far;
                aspect = "register";
                value = reach;
                limit = register_limit;
            }
        }
        TRY(rational_add(total, penalty, &total));
        measured_pairs++;
        if (rational_cmp(penalty, worst) > 0) {
            worst = penalty;
            snprintf(out->idea, sizeof(out->idea), "%s", pair->idea);
            snprintf(out->against, sizeof(out->against), "%s", pair->against);
            snprintf(out->aspect, sizeof(out->aspect), "%s", aspect);
            out->value = value;
            out->limit = limit;
            out->bar = line->notes[pair->idea_first].bar;
        }
    }
    if (measured_pairs > 0) {
        TRY(rational_div_int(total, (long long)measured_pairs, &out->penalty));
        out->penalty = clamp01(out->penalty);
    }
    return CRITIC_OK;
}

static CriticResult leap_load(const CriticLine *line, const KnowledgeSnapshot *knowledge, CriticFinding *out) {
    Rational allowance;
    CriticResult status = parameter(knowledge, CRITIC_LEAP_LOAD, "allowance", &allowance);
    if (status != CRITIC_OK || line->count < 2) {
        return status;
    }
    long long moves = (long long)line->count - 1;
    long long weight = 0;
    for (size_t i = 1; i < line->count; i++) {
        int step = abs(line->notes[i].pitch - line->notes[i - 1].pitch);
        weight += step >= 5 ? 2 : (step >= 3 ? 1 : 0);
    }
    Rational load;
    Rational over;
    status = share(weight, moves, &load);
    if (status != CRITIC_OK) {
        return status;
    }
    TRY(rational_sub(load, allowance, &over));
    out->penalty = clamp01(over);
    out->value = load;
    out->limit = allowance;
    out->count = (int)moves;
    return CRITIC_OK;
}

static CriticResult open_ending(const CriticLine *line, CriticFinding *out) {
    Rational sum = ZERO;
    Rational mean_times_count;
    for (size_t i = 0; i < line->count; i++) {
        TRY(rational_add(sum, line->notes[i].duration, &sum));
    }
    TRY(rational_mul_int(line->notes[line->count - 1].duration, (long long)line->count, &mean_times_count));
    if (rational_cmp(mean_times_count, sum) <= 0) {
        out->penalty = ONE;
    }
    out->bar = line->notes[line->count - 1].bar;
    return CRITIC_OK;
}

/* Whether the note is syncopated: off the beat and held past the next one. */
static CriticResult syncopated(const CriticLine *line, const CriticNote *note, bool *result) {
    Rational beats;
    long long floor = 0;
    Rational inside;
    Rational reach;
    TRY(rational_div(note->position, line->beat, &beats));
    if (beats.den == 1) {
        *result = false;
        return CRITIC_OK;
    }
    TRY(rational_floor(beats, &floor));
    TRY(rational_mul_int(line->beat, floor, &inside));
    TRY(rational_sub(note->position, inside, &inside));
    TRY(rational_add(inside, note->duration, &reach));
    *result = rational_cmp(reach, line->beat) > 0;
    return CRITIC_OK;
}

static CriticResult stray_syncopation(const CriticLine *line, CriticFinding *out) {
    bool *sync = calloc(line->count, sizeof(bool));
    if (sync == NULL) {
        return CRITIC_ERR_ALLOC;
    }
    long long total = 0;
    long long strays = 0;
    size_t first_stray = line->count;
    CriticResult status = CRITIC_OK;
    for (size_t i = 0; i < line->count && status == CRITIC_OK; i++) {
        status = syncopated(line, &line->notes[i], &sync[i]);
        total += sync[i] ? 1 : 0;
    }
    for (size_t i = 0; i < line->count && status == CRITIC_OK; i++) {
        bool shared = false;
        for (size_t j = 0; sync[i] && j < line->count; j++) {
            shared = shared || (j != i && sync[j] && rational_equals(line->notes[i].position, line->notes[j].position));
        }
        if (sync[i] && !shared) {
            strays++;
            first_stray = first_stray == line->count ? i : first_stray;
        }
    }
    if (status == CRITIC_OK && total > 0) {
        status = share(strays, total, &out->penalty);
        out->count = (int)strays;
    }
    if (status == CRITIC_OK && first_stray < line->count) {
        Rational beats;
        out->bar = line->notes[first_stray].bar;
        status = rational_div(line->notes[first_stray].position, line->beat, &beats) == RATIONAL_OK &&
                         rational_add(beats, ONE, &out->beat) == RATIONAL_OK
                     ? CRITIC_OK
                     : CRITIC_ERR_OVERFLOW;
    }
    free(sync);
    return status;
}

static CriticResult one_rhythm(const CriticLine *line, const CriticLine *const *set, size_t set_count, CriticFinding *out) {
    if (set == NULL || set_count < 2) {
        return CRITIC_OK;
    }
    Bar mine;
    Bar *scratch = calloc(line->count, sizeof(Bar));
    if (scratch == NULL) {
        return CRITIC_ERR_ALLOC;
    }
    line_bars(line, scratch);
    mine = scratch[0];
    free(scratch);
    long long matches = 0;
    long long others = 0;
    for (size_t i = 0; i < set_count; i++) {
        const CriticLine *other = set[i];
        if (other == line || other == NULL || other->count == 0) {
            others += other == line ? 0 : 1;
            continue;
        }
        Bar first;
        first.bar = other->notes[0].bar;
        first.first = 0;
        first.count = 0;
        while (first.count < other->count && other->notes[first.count].bar == first.bar) {
            first.count++;
        }
        others++;
        matches += same_rhythm(line, &mine, other, &first) ? 1 : 0;
    }
    out->count = (int)matches;
    out->bar = line->notes[0].bar;
    return others > 0 ? share(matches, others, &out->penalty) : CRITIC_OK;
}

static CriticResult repeated_notes(const CriticLine *line, CriticFinding *out) {
    long long repeats = 0;
    if (line->count < 2) {
        return CRITIC_OK;
    }
    for (size_t i = 1; i < line->count; i++) {
        repeats += line->notes[i].pitch == line->notes[i - 1].pitch ? 1 : 0;
    }
    out->count = (int)repeats;
    CriticResult status = share(repeats, (long long)line->count - 1, &out->penalty);
    out->value = out->penalty;
    return status;
}

static CriticResult wide_range(const CriticLine *line, const KnowledgeSnapshot *knowledge, CriticFinding *out) {
    Rational limit;
    Rational span_parameter;
    CriticResult status = parameter(knowledge, CRITIC_WIDE_RANGE, "limit", &limit);
    status = status == CRITIC_OK ? parameter(knowledge, CRITIC_WIDE_RANGE, "span", &span_parameter) : status;
    if (status != CRITIC_OK) {
        return status;
    }
    int low = line->notes[0].pitch;
    int high = low;
    for (size_t i = 1; i < line->count; i++) {
        low = line->notes[i].pitch < low ? line->notes[i].pitch : low;
        high = line->notes[i].pitch > high ? line->notes[i].pitch : high;
    }
    Rational span = whole(high - low);
    Rational over;
    Rational part;
    TRY(rational_sub(span, limit, &over));
    if (rational_cmp(over, ZERO) > 0) {
        TRY(rational_div(over, span_parameter, &part));
        out->penalty = clamp01(part);
    }
    out->value = span;
    out->limit = limit;
    return CRITIC_OK;
}

/* A bar's register: the unweighted mean pitch of the notes that start in it. */
static CriticResult bar_register(const CriticLine *line, const Bar *bar, Rational *out) {
    long long sum = 0;
    for (size_t i = 0; i < bar->count; i++) {
        sum += line->notes[bar->first + i].pitch;
    }
    return share(sum, (long long)bar->count, out);
}

/* The largest shift of register between neighbouring bars that have notes: (shift - limit) / span, from 0 to 1 (R-755). */
static CriticResult register_shift(const CriticLine *line, const KnowledgeSnapshot *knowledge, CriticFinding *out) {
    Rational limit;
    Rational span;
    CriticResult status = parameter(knowledge, CRITIC_REGISTER_SHIFT, "limit", &limit);
    status = status == CRITIC_OK ? parameter(knowledge, CRITIC_REGISTER_SHIFT, "span", &span) : status;
    if (status != CRITIC_OK) {
        return status;
    }
    Bar *bars = calloc(line->count, sizeof(Bar));
    if (bars == NULL) {
        return CRITIC_ERR_ALLOC;
    }
    size_t count = line_bars(line, bars);
    Rational largest = ZERO;
    size_t at = count;
    Rational previous = ZERO;
    for (size_t k = 0; k < count && status == CRITIC_OK; k++) {
        Rational mean;
        Rational shift;
        status = bar_register(line, &bars[k], &mean);
        if (status == CRITIC_OK && k > 0) {
            status = rational_sub(mean, previous, &shift) == RATIONAL_OK ? CRITIC_OK : CRITIC_ERR_OVERFLOW;
            if (status == CRITIC_OK && rational_cmp(shift, ZERO) < 0) {
                status = rational_sub(ZERO, shift, &shift) == RATIONAL_OK ? CRITIC_OK : CRITIC_ERR_OVERFLOW;
            }
            if (status == CRITIC_OK && (at == count || rational_cmp(shift, largest) > 0)) {
                largest = shift;
                at = k - 1;
            }
        }
        previous = mean;
    }
    if (status == CRITIC_OK && at < count) {
        Rational over;
        Rational part;
        long long rounded = 0;
        Rational half = {1, 2};
        Rational shifted;
        TRY(rational_sub(largest, limit, &over));
        if (rational_cmp(over, ZERO) > 0) {
            TRY(rational_div(over, span, &part));
            out->penalty = clamp01(part);
        }
        TRY(rational_add(largest, half, &shifted));
        TRY(rational_floor(shifted, &rounded));
        out->value = whole(rounded);
        out->limit = limit;
        out->first = bars[at].bar;
        out->last = bars[at + 1].bar;
        out->bar = bars[at].bar;
    }
    free(bars);
    return status;
}

/* Notes of exactly a half note that follow a half note: n / span, from 0 to 1 (R-755). */
static CriticResult half_notes(const CriticLine *line, const KnowledgeSnapshot *knowledge, CriticFinding *out) {
    Rational span;
    Rational half = {1, 2};
    CriticResult status = parameter(knowledge, CRITIC_HALF_NOTES, "span", &span);
    if (status != CRITIC_OK) {
        return status;
    }
    long long runs = 0;
    for (size_t i = 1; i < line->count; i++) {
        if (rational_cmp(line->notes[i].duration, half) == 0 && rational_cmp(line->notes[i - 1].duration, half) == 0) {
            out->bar = runs == 0 ? line->notes[i].bar : out->bar;
            runs++;
        }
    }
    if (runs > 0) {
        Rational part;
        TRY(rational_div(whole(runs), span, &part));
        out->penalty = clamp01(part);
        out->count = (int)runs;
    }
    return CRITIC_OK;
}

CriticResult critic_evaluate(const CriticLine *line, const CriticLine *const *set, size_t set_count, const KnowledgeSnapshot *knowledge,
                             CriticFinding out[CRITIC_EVALUATOR_COUNT]) {
    if (line == NULL || out == NULL) {
        return CRITIC_ERR_NULL;
    }
    if (line->count == 0 || line->notes == NULL) {
        return CRITIC_ERR_EMPTY;
    }
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        begin(&out[i], (CriticEvaluator)i);
        CriticResult status = parameter(knowledge, (CriticEvaluator)i, NULL, NULL);
        if (status != CRITIC_OK) {
            return status;
        }
    }
    CriticResult status = rhythmic_monotony(line, knowledge, &out[CRITIC_RHYTHMIC_MONOTONY]);
    status = status == CRITIC_OK ? contrast_distance(line, knowledge, &out[CRITIC_CONTRAST_DISTANCE]) : status;
    status = status == CRITIC_OK ? leap_load(line, knowledge, &out[CRITIC_LEAP_LOAD]) : status;
    status = status == CRITIC_OK ? open_ending(line, &out[CRITIC_OPEN_ENDING]) : status;
    status = status == CRITIC_OK ? stray_syncopation(line, &out[CRITIC_STRAY_SYNCOPATION]) : status;
    status = status == CRITIC_OK ? one_rhythm(line, set, set_count, &out[CRITIC_ONE_RHYTHM]) : status;
    status = status == CRITIC_OK ? repeated_notes(line, &out[CRITIC_REPEATED_NOTES]) : status;
    status = status == CRITIC_OK ? wide_range(line, knowledge, &out[CRITIC_WIDE_RANGE]) : status;
    status = status == CRITIC_OK ? register_shift(line, knowledge, &out[CRITIC_REGISTER_SHIFT]) : status;
    status = status == CRITIC_OK ? half_notes(line, knowledge, &out[CRITIC_HALF_NOTES]) : status;
    return status;
}

Rational critic_score(const CriticFinding findings[CRITIC_EVALUATOR_COUNT], const CriticSettings *settings) {
    Rational score = ZERO;
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        Rational term;
        if (rational_mul_int(findings[i].penalty, settings->weights[i], &term) != RATIONAL_OK ||
            (settings->reversed[i] ? rational_add(score, term, &score) : rational_sub(score, term, &score)) != RATIONAL_OK) {
            return ZERO;
        }
    }
    return score;
}
