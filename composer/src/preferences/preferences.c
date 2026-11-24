#include <ursatz_composer/preferences.h>

#include "grow/grow_internal.h"
#include "preferences/preferences_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <theory/tonality/roman_numeral_rules.h>

#define SCALE_MAX COMPOSER_PREFERENCES_SCALE_MAX
#define RATIO_DENOMINATOR_MAX COMPOSER_PREFERENCES_RATIO_DENOMINATOR_MAX
#define ATTEMPTS_MAX COMPOSER_PREFERENCES_ATTEMPTS_MAX
#define RANGE_LOW_MIN COMPOSER_PREFERENCES_RANGE_LOW_MIN
#define RANGE_LOW_MAX COMPOSER_PREFERENCES_RANGE_LOW_MAX
#define RANGE_HIGH_MAX COMPOSER_PREFERENCES_RANGE_HIGH_MAX
#define RANGE_SPAN_MIN COMPOSER_PREFERENCES_RANGE_SPAN_MIN
#define DENSITY_MAX COMPOSER_PREFERENCES_DENSITY_MAX
#define CLIMAX_RISE_MIN COMPOSER_PREFERENCES_CLIMAX_RISE_MIN
#define CLIMAX_RISE_MAX COMPOSER_PREFERENCES_CLIMAX_RISE_MAX
#define PICKUP_ONE_IN_MAX COMPOSER_PREFERENCES_PICKUP_ONE_IN_MAX
#define ACCENTED_ONE_IN_MAX COMPOSER_PREFERENCES_ACCENTED_ONE_IN_MAX
#define LONG_NCT_ONE_IN_MAX COMPOSER_PREFERENCES_LONG_NCT_ONE_IN_MAX
#define LEAPS_PER_MOTIF_MAX COMPOSER_PREFERENCES_LEAPS_PER_MOTIF_MAX
#define SYNCOPATION_ONE_IN_MAX COMPOSER_PREFERENCES_SYNCOPATION_ONE_IN_MAX
#define SKIPS_PER_MOTIF_MAX COMPOSER_PREFERENCES_SKIPS_PER_MOTIF_MAX
#define COLOR_NOTE_MAX 20      /* melody.color_note, R-732 */
#define MOTIF_COLOR_NOTE_MAX 8 /* motif.color_note, R-732 */
#define PLAN_MAX COMPOSER_PREFERENCES_PLAN_MAX
#define PLAN_BEAT_MAX COMPOSER_PREFERENCES_PLAN_BEAT_MAX
#define LEAP_MIN COMPOSER_PREFERENCES_LEAP_MIN
#define LEAP_MAX COMPOSER_PREFERENCES_LEAP_MAX
#define RHYTHM_REPEAT_BARS_MAX COMPOSER_PREFERENCES_RHYTHM_REPEAT_BARS_MAX
#define POOL_MIN COMPOSER_PREFERENCES_POOL_MIN
#define POOL_MAX COMPOSER_PREFERENCES_POOL_MAX

/* The non-chord tones nct_types may name, in the order the document writes them. */
static const struct {
    const char *name;
    unsigned bit;
} NCT_NAMES[] = {{"passing", MELODY_NCT_PASSING},
                 {"neighbor", MELODY_NCT_NEIGHBOR},
                 {"appoggiatura", MELODY_NCT_APPOGGIATURA},
                 {"suspension", MELODY_NCT_SUSPENSION},
                 {"anticipation", MELODY_NCT_ANTICIPATION}};
#define NCT_COUNT (sizeof(NCT_NAMES) / sizeof(NCT_NAMES[0]))

/* The weight keys, in the order the document writes them, and where each lives in MelodyWeights. */
static const struct {
    const char *name;
    size_t offset;
} WEIGHT_KEYS[] = {{"melodic", offsetof(MelodyWeights, melodic)},
                   {"against_bass", offsetof(MelodyWeights, against_bass)},
                   {"tendency", offsetof(MelodyWeights, tendency)},
                   {"climax", offsetof(MelodyWeights, climax)},
                   {"contour", offsetof(MelodyWeights, contour)},
                   {"ending_reduction", offsetof(MelodyWeights, ending_reduction)},
                   {"waves", offsetof(MelodyWeights, waves)},
                   {"central_range", offsetof(MelodyWeights, central_range)},
                   {"monotony", offsetof(MelodyWeights, monotony)},
                   {"seam", offsetof(MelodyWeights, seam)}};
#define WEIGHT_COUNT (sizeof(WEIGHT_KEYS) / sizeof(WEIGHT_KEYS[0]))

void composer_preferences_default(ComposerPreferences *out) {
    memset(out, 0, sizeof(*out));
    out->melody = melody_fill_params_default();
    motive_random_preferences_default(&out->motif);
    grow_fidelity_table_default(&out->development);
    out->continuation.fragment_lengths[GROW_FRAGMENT_SEGMENT] = 1;
    out->contrast.distance[GROW_CONTRAST_SIMILAR] = 1;
    out->candidates.pool = 4;
    critic_settings_default(&out->critic);
    composer_mixture_default(&out->harmony.mixture);
}

/* Refuses with the path under the document, "melody/keep" for /melody/keep; "" is the document itself. */
bool preferences_refuse(const Refusal *refusal, const char *where, const char *limit) {
    if (refusal->path && refusal->path_size > 0) {
        snprintf(refusal->path, refusal->path_size, "%s%s", where[0] ? "/" : "", where);
    }
    if (refusal->message && refusal->message_size > 0) {
        snprintf(refusal->message, refusal->message_size, "%s", limit);
    }
    return false;
}

/* The key's path under a group: "keep" under "melody" for /melody/keep, "leap/high" under "motif" for /motif/leap/high. */
bool preferences_refuse_in(const Refusal *refusal, const char *group, const char *key, const char *limit) {
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", group, key);
    return preferences_refuse(refusal, path, limit);
}

static bool refuse_melody(const Refusal *refusal, const char *key, const char *limit) {
    return preferences_refuse_in(refusal, "melody", key, limit);
}

bool preferences_whole_number(const cJSON *item, double low, double high, double *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble < low || item->valuedouble > high ||
        item->valuedouble != (double)(long long)item->valuedouble) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

/* An unsigned setting from a whole number low to high; says which limit it broke when it is refused. */
bool preferences_read_unsigned_in(const cJSON *item, const char *group, const char *key, unsigned low, unsigned high,
                             unsigned *out, const Refusal *refusal) {
    double value;
    char limit[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (preferences_whole_number(item, low, high, &value)) {
        *out = (unsigned)value;
        return true;
    }
    snprintf(limit, sizeof(limit), "must be a whole number from %u to %u", low, high);
    return preferences_refuse_in(refusal, group, key, limit);
}

static bool read_unsigned(const cJSON *item, const char *key, unsigned low, unsigned high, unsigned *out,
                          const Refusal *refusal) {
    return preferences_read_unsigned_in(item, "melody", key, low, high, out, refusal);
}

/* "n/d" with 0 <= n <= d and 1 <= d <= denominator_max, digits only. */
bool preferences_parse_ratio(const char *text, unsigned long denominator_max, Rational *out) {
    unsigned long numerator = 0;
    unsigned long denominator = 0;
    size_t i = 0;
    size_t digits = 0;
    for (; text[i] >= '0' && text[i] <= '9' && digits < 4; i++, digits++) {
        numerator = numerator * 10 + (unsigned long)(text[i] - '0');
    }
    if (digits == 0 || text[i] != '/') {
        return false;
    }
    digits = 0;
    for (i++; text[i] >= '0' && text[i] <= '9' && digits < 4; i++, digits++) {
        denominator = denominator * 10 + (unsigned long)(text[i] - '0');
    }
    if (digits == 0 || text[i] != '\0' || denominator < 1 || denominator > denominator_max || numerator > denominator) {
        return false;
    }
    out->num = numerator;
    out->den = denominator;
    return true;
}

static bool read_range(const cJSON *range, MelodyFillParams *melody, const Refusal *refusal) {
    if (!cJSON_IsObject(range)) {
        return refuse_melody(refusal, "range", "must be an object with low and high");
    }
    int low = melody->range_low;
    int high = melody->range_high;
    const cJSON *item;
    cJSON_ArrayForEach(item, range) {
        unsigned value;
        if (strcmp(item->string, "low") == 0) {
            if (!read_unsigned(item, "range/low", RANGE_LOW_MIN, RANGE_LOW_MAX, &value, refusal)) {
                return false;
            }
            low = (int)value;
        } else if (strcmp(item->string, "high") == 0) {
            if (!read_unsigned(item, "range/high", RANGE_LOW_MIN + RANGE_SPAN_MIN, RANGE_HIGH_MAX, &value, refusal)) {
                return false;
            }
            high = (int)value;
        } else {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "range/%s", item->string);
            return refuse_melody(refusal, key, "unknown member");
        }
    }
    if (high < low + RANGE_SPAN_MIN) {
        return refuse_melody(refusal, "range/high", "must be at least range.low + 12");
    }
    melody->range_low = low;
    melody->range_high = high;
    return true;
}

static bool read_nct_types(const cJSON *list, MelodyFillParams *melody, const Refusal *refusal) {
    unsigned bits = 0;
    const cJSON *item;
    if (!cJSON_IsArray(list)) {
        return refuse_melody(refusal, "nct_types", "must be a list of passing, neighbor, appoggiatura, suspension, anticipation");
    }
    cJSON_ArrayForEach(item, list) {
        unsigned bit = 0;
        for (size_t i = 0; cJSON_IsString(item) && i < NCT_COUNT; i++) {
            if (strcmp(item->valuestring, NCT_NAMES[i].name) == 0) {
                bit = NCT_NAMES[i].bit;
            }
        }
        if (bit == 0) {
            return refuse_melody(refusal, "nct_types", "must name only passing, neighbor, appoggiatura, suspension, anticipation");
        }
        if (bits & bit) {
            return refuse_melody(refusal, "nct_types", "must not repeat a type");
        }
        bits |= bit;
    }
    melody->nct_types = bits;
    return true;
}

static bool read_ratio(const cJSON *item, const char *key, Rational *out, const Refusal *refusal) {
    if (!cJSON_IsString(item) || !preferences_parse_ratio(item->valuestring, RATIO_DENOMINATOR_MAX, out)) {
        return refuse_melody(refusal, key, "must be a ratio n/d with 0 <= n <= d and 1 <= d <= 16");
    }
    return true;
}

static bool read_weights(const cJSON *weights, MelodyWeights *out, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(weights)) {
        return refuse_melody(refusal, "weights", "must be an object of whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(item, weights) {
        bool known = false;
        for (size_t i = 0; i < WEIGHT_COUNT && !known; i++) {
            if (strcmp(item->string, WEIGHT_KEYS[i].name) == 0) {
                char key[COMPOSER_PREFERENCES_PATH_MAX];
                snprintf(key, sizeof(key), "weights/%s", item->string);
                if (!read_unsigned(item, key, 0, WEIGHT_MAX, (unsigned *)(void *)((char *)out + WEIGHT_KEYS[i].offset),
                                   refusal)) {
                    return false;
                }
                known = true;
            }
        }
        if (!known) {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "weights/%s", item->string);
            return refuse_melody(refusal, key, "unknown member");
        }
    }
    return true;
}

/* The closure weights (phase L4): final and first, each 0 to 20; a name left out keeps its value. */
static bool read_closure(const cJSON *closure, MelodyFillParams *melody, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(closure)) {
        return refuse_melody(refusal, "closure", "must be an object with final and first, whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(item, closure) {
        if (strcmp(item->string, "final") == 0) {
            if (!read_unsigned(item, "closure/final", 0, WEIGHT_MAX, &melody->closure_final, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "first") == 0) {
            if (!read_unsigned(item, "closure/first", 0, WEIGHT_MAX, &melody->closure_first, refusal)) {
                return false;
            }
        } else {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "closure/%s", item->string);
            return refuse_melody(refusal, key, "unknown member");
        }
    }
    return true;
}

/* The names a list of the melody group may hold, "forbidden-leap, forbidden-outline, ...", for the refusal that names them. */
static void rule_names(bool groups, char *text, size_t size) {
    size_t used = 0;
    unsigned count = groups ? MELODY_GROUP_COUNT : MELODY_RULE_COUNT;
    text[0] = '\0';
    for (unsigned i = 0; i < count && used < size; i++) {
        const char *name = groups ? melody_soft_group_name((MelodySoftGroup)i) : melody_hard_rule_name((MelodyHardRule)i);
        int written = snprintf(text + used, size - used, "%s%s", i ? ", " : "", name);
        used += written > 0 ? (size_t)written : 0;
    }
}

/*
 * waive and reverse (R-672): a list of names, none twice. waive names hard rules the melody stage does not hold,
 * reverse names soft groups whose score counts what a candidate breaks. *bits is the list as bits of the enum.
 */
static bool read_rule_list(const cJSON *list, bool groups, unsigned *bits, const Refusal *refusal) {
    const char *key_name = groups ? "reverse" : "waive";
    unsigned found = 0;
    const cJSON *item;
    char names[192];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    int index = 0;
    if (!cJSON_IsArray(list)) {
        return refuse_melody(refusal, key_name, "must be a list of names");
    }
    rule_names(groups, names, sizeof(names));
    cJSON_ArrayForEach(item, list) {
        unsigned bit = 0;
        bool known = false;
        char key[COMPOSER_PREFERENCES_PATH_MAX];
        snprintf(key, sizeof(key), "%s/%d", key_name, index++);
        if (cJSON_IsString(item) && groups) {
            MelodySoftGroup group = MELODY_GROUP_MELODIC;
            known = melody_soft_group_find(item->valuestring, &group);
            bit = 1u << group;
        } else if (cJSON_IsString(item)) {
            MelodyHardRule rule = MELODY_RULE_FORBIDDEN_LEAP;
            known = melody_hard_rule_find(item->valuestring, &rule);
            bit = 1u << rule;
        }
        if (!known) {
            snprintf(message, sizeof(message), groups ? "is not a soft rule group: %s" : "is not a rule the melody stage can waive: %s",
                     names);
            return refuse_melody(refusal, key, message);
        }
        if (found & bit) {
            return refuse_melody(refusal, key, "repeats a name of the list");
        }
        found |= bit;
    }
    *bits = found;
    return true;
}

static bool read_melody(const cJSON *group, MelodyFillParams *melody, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "melody", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        const char *key = item->string;
        bool ok;
        if (strcmp(key, "range") == 0) {
            ok = read_range(item, melody, refusal);
        } else if (strcmp(key, "nct_types") == 0) {
            ok = read_nct_types(item, melody, refusal);
        } else if (strcmp(key, "density") == 0) {
            ok = read_unsigned(item, key, 0, DENSITY_MAX, &melody->density, refusal);
        } else if (strcmp(key, "climax_position") == 0) {
            ok = read_unsigned(item, key, 0, SCALE_MAX, &melody->climax_position, refusal);
        } else if (strcmp(key, "climax_rise") == 0) {
            ok = read_unsigned(item, key, CLIMAX_RISE_MIN, CLIMAX_RISE_MAX, &melody->climax_rise, refusal);
        } else if (strcmp(key, "climax_slack") == 0) {
            ok = read_unsigned(item, key, 0, SCALE_MAX, &melody->climax_slack, refusal);
        } else if (strcmp(key, "second_nearest_chord_tone") == 0) {
            ok = read_ratio(item, key, &melody->second_nearest, refusal);
        } else if (strcmp(key, "attempts") == 0) {
            ok = read_unsigned(item, key, 1, ATTEMPTS_MAX, &melody->attempts, refusal);
        } else if (strcmp(key, "keep") == 0) {
            ok = read_unsigned(item, key, 1, MELODY_KEEP_MAX, &melody->keep, refusal);
        } else if (strcmp(key, "plain_shaping") == 0) {
            ok = cJSON_IsBool(item) ? (melody->plain_shaping = cJSON_IsTrue(item), true)
                                    : refuse_melody(refusal, key, "must be true or false");
        } else if (strcmp(key, "weights") == 0) {
            ok = read_weights(item, &melody->weights, refusal);
        } else if (strcmp(key, "closure") == 0) {
            ok = read_closure(item, melody, refusal);
        } else if (strcmp(key, "rhythm_repeat_bars") == 0) {
            ok = read_unsigned(item, key, 0, RHYTHM_REPEAT_BARS_MAX, &melody->rhythm_repeat_bars, refusal);
        } else if (strcmp(key, "leaps") == 0) {
            ok = read_unsigned(item, key, 0, WEIGHT_MAX, &melody->leap_weight, refusal);
        } else if (strcmp(key, "register") == 0) {
            ok = read_unsigned(item, key, 0, WEIGHT_MAX, &melody->register_weight, refusal);
        } else if (strcmp(key, "cadence_long") == 0) {
            ok = cJSON_IsBool(item) ? (melody->cadence_long = cJSON_IsTrue(item), true)
                                    : refuse_melody(refusal, key, "must be true or false");
        } else if (strcmp(key, "color_note") == 0) {
            ok = read_unsigned(item, key, 0, COLOR_NOTE_MAX, &melody->color_note, refusal);
        } else if (strcmp(key, "waive") == 0) {
            ok = read_rule_list(item, false, &melody->waive, refusal);
        } else if (strcmp(key, "reverse") == 0) {
            ok = read_rule_list(item, true, &melody->reverse, refusal);
        } else {
            ok = refuse_melody(refusal, key, "unknown member");
        }
        if (!ok) {
            return false;
        }
    }
    /* keep is checked against attempts once both are known, whichever came first in the document. */
    if (melody->keep > melody->attempts) {
        return refuse_melody(refusal, "keep", "must be at most attempts");
    }
    return true;
}

/* ------------------------------------------------------------- the motif group */

/* A weighted list of the motif group: its key, where it lives in MotiveRandomPreferences, and its entries' names. */
typedef struct {
    const char *key;
    size_t offset;
    size_t count;
    const char *names[8];
} WeightedList;

static const WeightedList MOTIF_LISTS[] = {
    {"bars", offsetof(MotiveRandomPreferences, bars), 2, {"one", "two"}},
    {"shapes_one_bar", offsetof(MotiveRandomPreferences, shapes_one_bar), 2, {"repeat", "contrast"}},
    {"shapes_two_bars", offsetof(MotiveRandomPreferences, shapes_two_bars), 3, {"repeat", "contrast", "repeat_contrast"}},
    {"plans_two", offsetof(MotiveRandomPreferences, plans_two), 3, {"I-V", "I-IV", "I-I"}},
    {"plans_three", offsetof(MotiveRandomPreferences, plans_three), 2, {"I-V-I", "I-IV-I"}},
    {"features", offsetof(MotiveRandomPreferences, features), 3, {"dotted", "leap", "repeat"}},
    {"rung_moves", offsetof(MotiveRandomPreferences, rung_moves), 2, {"step", "skip"}},
    {"decorations", offsetof(MotiveRandomPreferences, decorations), 5,
     {"chord_tone", "passing", "neighbor", "appoggiatura", "escape"}},
    {"durations", offsetof(MotiveRandomPreferences, durations), 8,
     {"half_beat", "beat", "two_beats", "quarter_beats", "long_short", "short_long", "triplet", "triplet_two_beats"}},
    {"accented_types", offsetof(MotiveRandomPreferences, accented_types), 3, {"appoggiatura", "passing", "suspension"}}};
#define MOTIF_LIST_COUNT (sizeof(MOTIF_LISTS) / sizeof(MOTIF_LISTS[0]))

/* The lists of phase L2: how a motive ends and the long non-chord tones. They follow final_long and compound_beats. */
static const WeightedList MOTIF_LISTS_L2[] = {
    {"endings", offsetof(MotiveRandomPreferences, endings), 4, {"free", "closed", "open", "lead_in"}},
    {"long_nct_types", offsetof(MotiveRandomPreferences, long_nct_types), 5,
     {"appoggiatura", "suspension", "retardation", "passing", "neighbor"}},
    {"long_nct_intervals", offsetof(MotiveRandomPreferences, long_nct_intervals), 4,
     {"second", "fourth", "sixth", "seventh"}},
    {"long_nct_beats", offsetof(MotiveRandomPreferences, long_nct_beats), 3, {"downbeat", "middle", "other"}},
    {"long_nct_lengths", offsetof(MotiveRandomPreferences, long_nct_lengths), 3, {"beat", "dotted_beat", "two_beats"}}};
#define MOTIF_LIST_L2_COUNT (sizeof(MOTIF_LISTS_L2) / sizeof(MOTIF_LISTS_L2[0]))

/* The lists of phase L3: the syncopation figures and their levels. They follow long_nct_lengths, after syncopation_one_in. */
static const WeightedList MOTIF_LISTS_L3[] = {
    {"syncopation_types", offsetof(MotiveRandomPreferences, syncopation_types), 4,
     {"off_beat", "tie_beat", "tie_bar", "anticipation"}},
    {"syncopation_levels", offsetof(MotiveRandomPreferences, syncopation_levels), 2, {"eighth", "sixteenth"}}};
#define MOTIF_LIST_L3_COUNT (sizeof(MOTIF_LISTS_L3) / sizeof(MOTIF_LISTS_L3[0]))

typedef enum { PLAN_CHORD_OK, PLAN_CHORD_SYNTAX, PLAN_CHORD_BAR, PLAN_CHORD_BEAT } PlanChordResult;

/* Digits of the text from *at, up to 4 of them; false when there is none. */
static bool plan_digits(const char *text, size_t *at, unsigned *out) {
    size_t digits = 0;
    unsigned value = 0;
    while (text[*at] >= '0' && text[*at] <= '9') {
        if (digits < 4) {
            value = value * 10 + (unsigned)(text[*at] - '0');
        } else {
            value = 99999;
        }
        digits++;
        (*at)++;
    }
    *out = value;
    return digits > 0;
}

/* One plan chord: a Roman numeral I to VII in any case, an optional o or degree sign, an optional @bar or @bar:beat. */
static PlanChordResult parse_plan_chord(const char *text, MotiveRandomPlanChord *out) {
    size_t n = standard_tonality_roman_numeral_letters(text);
    size_t at = n;
    unsigned value;
    int degree = standard_tonality_roman_numeral_degree_of(text, n);
    if (degree == 0) {
        return PLAN_CHORD_SYNTAX;
    }
    if (text[at] == 'o') {
        at++;
    } else if ((unsigned char)text[at] == 0xC2 && (unsigned char)text[at + 1] == 0xB0) {
        at += 2;
    }
    out->degree = degree;
    out->bar = 0;
    out->beat = 0;
    if (text[at] == '@') {
        at++;
        if (!plan_digits(text, &at, &value)) {
            return PLAN_CHORD_SYNTAX;
        }
        if (value < 1 || value > 2) {
            return PLAN_CHORD_BAR;
        }
        out->bar = value;
        out->beat = 1;
        if (text[at] == ':') {
            at++;
            if (!plan_digits(text, &at, &value)) {
                return PLAN_CHORD_SYNTAX;
            }
            if (value < 1 || value > PLAN_BEAT_MAX) {
                return PLAN_CHORD_BEAT;
            }
            out->beat = value;
        }
    }
    return text[at] == '\0' ? PLAN_CHORD_OK : PLAN_CHORD_SYNTAX;
}

static bool refuse_plan_chord(const Refusal *refusal, size_t index, const char *limit) {
    char key[COMPOSER_PREFERENCES_PATH_MAX];
    snprintf(key, sizeof(key), "plan/%zu", index);
    return preferences_refuse_in(refusal, "motif", key, limit);
}

/*
 * The given chord plan: 1 to PLAN_MAX strings. Either no chord has a place (at most 3 chords, one per segment) or every chord
 * after the first has one, strictly rising; the first is at 1:1 and may be written with it. The meter-dependent refusal (the
 * beat against the bar's beats, the bars and shapes allowed) is the run's (MOTIVE_RANDOM_ERR_PLAN).
 */
static bool read_plan(const cJSON *item, MotiveRandomPreferences *motif, const Refusal *refusal) {
    MotiveRandomPlanChord plan[COMPOSER_PREFERENCES_PLAN_MAX];
    size_t count = 0;
    size_t placed = 0;
    const cJSON *entry;
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) < 1 || cJSON_GetArraySize(item) > PLAN_MAX) {
        return preferences_refuse_in(refusal, "motif", "plan", "must be a list of 1 to 4 chords");
    }
    cJSON_ArrayForEach(entry, item) {
        PlanChordResult result = cJSON_IsString(entry) ? parse_plan_chord(entry->valuestring, &plan[count]) : PLAN_CHORD_SYNTAX;
        if (result == PLAN_CHORD_SYNTAX) {
            return refuse_plan_chord(refusal, count, "must be a Roman numeral I to VII with an optional @bar or @bar:beat");
        }
        if (result == PLAN_CHORD_BAR) {
            return refuse_plan_chord(refusal, count, "the bar must be 1 or 2");
        }
        if (result == PLAN_CHORD_BEAT) {
            return refuse_plan_chord(refusal, count, "the beat must be a whole number from 1 to 12");
        }
        if (count > 0 && plan[count].bar > 0) {
            placed++;
        }
        count++;
    }
    if (plan[0].bar != 0 && (plan[0].bar != 1 || plan[0].beat != 1)) {
        return refuse_plan_chord(refusal, 0, "the first chord starts at 1:1");
    }
    if (placed > 0) {
        for (size_t k = 1; k < count; k++) {
            if (plan[k].bar == 0) {
                return refuse_plan_chord(refusal, k, "every chord after the first has a place, or none does");
            }
        }
        for (size_t k = 1; k < count; k++) {
            unsigned before_bar = k == 1 ? 1 : plan[k - 1].bar;
            unsigned before_beat = k == 1 ? 1 : plan[k - 1].beat;
            if (plan[k].bar < before_bar || (plan[k].bar == before_bar && plan[k].beat <= before_beat)) {
                return refuse_plan_chord(refusal, k, "must start after the chord before");
            }
        }
    } else if (count > 3) {
        return preferences_refuse_in(refusal, "motif", "plan", "at most 3 chords without places (one per segment)");
    }
    motif->plan_count = count;
    memcpy(motif->plan, plan, count * sizeof(plan[0]));
    return true;
}

static unsigned long *list_values(MotiveRandomPreferences *motif, const WeightedList *list) {
    return (unsigned long *)(void *)((char *)motif + list->offset);
}

static const unsigned long *list_values_const(const MotiveRandomPreferences *motif, const WeightedList *list) {
    return (const unsigned long *)(const void *)((const char *)motif + list->offset);
}

/* Whole numbers 0 to 20 under the entries' names, with a sum above 0. */
static bool read_weighted_list(const cJSON *item, const WeightedList *list, MotiveRandomPreferences *motif,
                               const Refusal *refusal) {
    unsigned long *values = list_values(motif, list);
    char key[COMPOSER_PREFERENCES_PATH_MAX];
    const cJSON *entry;
    if (!cJSON_IsObject(item)) {
        return preferences_refuse_in(refusal, "motif", list->key, "must be an object of whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(entry, item) {
        bool known = false;
        for (size_t i = 0; i < list->count && !known; i++) {
            if (strcmp(entry->string, list->names[i]) == 0) {
                unsigned value;
                snprintf(key, sizeof(key), "%s/%s", list->key, entry->string);
                if (!preferences_read_unsigned_in(entry, "motif", key, 0, WEIGHT_MAX, &value, refusal)) {
                    return false;
                }
                values[i] = value;
                known = true;
            }
        }
        if (!known) {
            snprintf(key, sizeof(key), "%s/%s", list->key, entry->string);
            return preferences_refuse_in(refusal, "motif", key, "unknown member");
        }
    }
    unsigned long sum = 0;
    for (size_t i = 0; i < list->count; i++) {
        sum += values[i];
    }
    if (sum == 0) {
        return preferences_refuse_in(refusal, "motif", list->key, "the weights must sum to more than 0");
    }
    return true;
}

static bool read_leap(const cJSON *item, MotiveRandomPreferences *motif, const Refusal *refusal) {
    int low = motif->leap_low;
    int high = motif->leap_high;
    const cJSON *entry;
    if (!cJSON_IsObject(item)) {
        return preferences_refuse_in(refusal, "motif", "leap", "must be an object with low and high");
    }
    cJSON_ArrayForEach(entry, item) {
        unsigned value;
        if (strcmp(entry->string, "low") == 0) {
            if (!preferences_read_unsigned_in(entry, "motif", "leap/low", LEAP_MIN, LEAP_MAX, &value, refusal)) {
                return false;
            }
            low = (int)value;
        } else if (strcmp(entry->string, "high") == 0) {
            if (!preferences_read_unsigned_in(entry, "motif", "leap/high", LEAP_MIN, LEAP_MAX, &value, refusal)) {
                return false;
            }
            high = (int)value;
        } else {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "leap/%s", entry->string);
            return preferences_refuse_in(refusal, "motif", key, "unknown member");
        }
    }
    if (high < low) {
        return preferences_refuse_in(refusal, "motif", "leap/high", "must be at least leap.low");
    }
    motif->leap_low = low;
    motif->leap_high = high;
    return true;
}

static bool read_motif_bool(const cJSON *item, const char *key, bool *out, const Refusal *refusal) {
    if (!cJSON_IsBool(item)) {
        return preferences_refuse_in(refusal, "motif", key, "must be true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

static bool read_contrast_fidelity_min(const cJSON *item, MotiveRandomPreferences *motif, const Refusal *refusal) {
    Rational ratio;
    if (!cJSON_IsString(item) || !preferences_parse_ratio(item->valuestring, FIDELITY_DENOMINATOR_MAX, &ratio)) {
        return preferences_refuse_in(refusal, "motif", "contrast_fidelity_min", "must be a ratio n/d from 0 to 1 with 1 <= d <= 1000");
    }
    motif->contrast_fidelity_min = ratio;
    return true;
}

static bool read_motif(const cJSON *group, MotiveRandomPreferences *motif, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "motif", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        const char *key = item->string;
        bool known = false;
        for (size_t i = 0; i < MOTIF_LIST_COUNT && !known; i++) {
            if (strcmp(key, MOTIF_LISTS[i].key) == 0) {
                known = true;
                if (!read_weighted_list(item, &MOTIF_LISTS[i], motif, refusal)) {
                    return false;
                }
            }
        }
        for (size_t i = 0; i < MOTIF_LIST_L2_COUNT && !known; i++) {
            if (strcmp(key, MOTIF_LISTS_L2[i].key) == 0) {
                known = true;
                if (!read_weighted_list(item, &MOTIF_LISTS_L2[i], motif, refusal)) {
                    return false;
                }
            }
        }
        for (size_t i = 0; i < MOTIF_LIST_L3_COUNT && !known; i++) {
            if (strcmp(key, MOTIF_LISTS_L3[i].key) == 0) {
                known = true;
                if (!read_weighted_list(item, &MOTIF_LISTS_L3[i], motif, refusal)) {
                    return false;
                }
            }
        }
        if (known) {
            continue;
        }
        if (strcmp(key, "pickup_one_in") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, PICKUP_ONE_IN_MAX, &motif->pickup_one_in, refusal)) {
                return false;
            }
        } else if (strcmp(key, "leap") == 0) {
            if (!read_leap(item, motif, refusal)) {
                return false;
            }
        } else if (strcmp(key, "accented_one_in") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, ACCENTED_ONE_IN_MAX, &motif->accented_one_in, refusal)) {
                return false;
            }
        } else if (strcmp(key, "leaps_per_motif") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, LEAPS_PER_MOTIF_MAX, &motif->leaps_per_motif, refusal)) {
                return false;
            }
        } else if (strcmp(key, "long_on_weak") == 0) {
            unsigned value;
            if (!preferences_read_unsigned_in(item, "motif", key, 0, WEIGHT_MAX, &value, refusal)) {
                return false;
            }
            motif->long_on_weak = value;
        } else if (strcmp(key, "avoid_repeats") == 0) {
            if (!read_motif_bool(item, key, &motif->avoid_repeats, refusal)) {
                return false;
            }
        } else if (strcmp(key, "leap_recovery") == 0) {
            if (!read_motif_bool(item, key, &motif->leap_recovery, refusal)) {
                return false;
            }
        } else if (strcmp(key, "pickup_from_accent") == 0) {
            if (!read_motif_bool(item, key, &motif->pickup_from_accent, refusal)) {
                return false;
            }
        } else if (strcmp(key, "final_long") == 0) {
            if (!read_motif_bool(item, key, &motif->final_long, refusal)) {
                return false;
            }
        } else if (strcmp(key, "compound_beats") == 0) {
            if (!read_motif_bool(item, key, &motif->compound_beats, refusal)) {
                return false;
            }
        } else if (strcmp(key, "long_nct_one_in") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, LONG_NCT_ONE_IN_MAX, &motif->long_nct_one_in, refusal)) {
                return false;
            }
        } else if (strcmp(key, "syncopation_one_in") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, SYNCOPATION_ONE_IN_MAX, &motif->syncopation_one_in, refusal)) {
                return false;
            }
        } else if (strcmp(key, "skips_per_motif") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, SKIPS_PER_MOTIF_MAX, &motif->skips_per_motif, refusal)) {
                return false;
            }
        } else if (strcmp(key, "color_note") == 0) {
            if (!preferences_read_unsigned_in(item, "motif", key, 0, MOTIF_COLOR_NOTE_MAX, &motif->color_note, refusal)) {
                return false;
            }
        } else if (strcmp(key, "plan") == 0) {
            if (!read_plan(item, motif, refusal)) {
                return false;
            }
        } else if (strcmp(key, "contrast_fidelity_min") == 0) {
            if (!read_contrast_fidelity_min(item, motif, refusal)) {
                return false;
            }
        } else {
            return preferences_refuse_in(refusal, "motif", key, "unknown member");
        }
    }
    return true;
}

/* ---------------------------------------------------------- the candidates group (R-673) */

static bool read_candidates(const cJSON *group, CandidatePreferences *candidates, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "candidates", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        if (strcmp(item->string, "variety") == 0) {
            if (!preferences_read_unsigned_in(item, "candidates", "variety", 0, WEIGHT_MAX, &candidates->variety, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "pool") == 0) {
            if (!preferences_read_unsigned_in(item, "candidates", "pool", POOL_MIN, POOL_MAX, &candidates->pool, refusal)) {
                return false;
            }
        } else {
            return preferences_refuse_in(refusal, "candidates", item->string, "unknown member");
        }
    }
    return true;
}

bool composer_preferences_read(const cJSON *object, ComposerPreferences *out, char *path, size_t path_size, char *message,
                               size_t message_size) {
    Refusal refusal = {path, path_size, message, message_size};
    const cJSON *item;
    composer_preferences_default(out);
    if (path && path_size > 0) {
        path[0] = '\0';
    }
    if (message && message_size > 0) {
        message[0] = '\0';
    }
    if (!cJSON_IsObject(object)) {
        return preferences_refuse(&refusal, "", "preferences must be an object");
    }
    cJSON_ArrayForEach(item, object) {
        if (strcmp(item->string, "melody") == 0) {
            if (!read_melody(item, &out->melody, &refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "motif") == 0) {
            if (!read_motif(item, &out->motif, &refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "development") == 0) {
            if (!preferences_read_development(item, out, &refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "candidates") == 0) {
            if (!read_candidates(item, &out->candidates, &refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "critic") == 0) {
            if (!preferences_read_critic(item, out, &refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "harmony") == 0) {
            if (!preferences_read_harmony(item, out, &refusal)) {
                return false;
            }
        } else {
            return preferences_refuse(&refusal, item->string, "unknown member");
        }
    }
    return true;
}

bool preferences_add_number(cJSON *object, const char *name, double value) {
    return cJSON_AddNumberToObject(object, name, value) != NULL;
}

static bool add_ratio(cJSON *object, const char *name, Rational ratio) {
    char text[32];
    snprintf(text, sizeof(text), "%lld/%lld", ratio.num, ratio.den);
    return cJSON_AddStringToObject(object, name, text) != NULL;
}

static cJSON *write_melody(const MelodyFillParams *melody) {
    cJSON *group = cJSON_CreateObject();
    cJSON *range = group ? cJSON_AddObjectToObject(group, "range") : NULL;
    cJSON *types = group ? cJSON_CreateArray() : NULL;
    bool ok = range && types && preferences_add_number(range, "low", melody->range_low) && preferences_add_number(range, "high", melody->range_high);
    for (size_t i = 0; ok && i < NCT_COUNT; i++) {
        if (melody->nct_types & NCT_NAMES[i].bit) {
            cJSON *name = cJSON_CreateString(NCT_NAMES[i].name);
            if (!name) {
                ok = false;
            } else if (!cJSON_AddItemToArray(types, name)) {
                cJSON_Delete(name);
                ok = false;
            }
        }
    }
    if (ok) {
        ok = cJSON_AddItemToObject(group, "nct_types", types);
        types = ok ? NULL : types;
    }
    ok = ok && preferences_add_number(group, "density", melody->density) && preferences_add_number(group, "climax_position", melody->climax_position) &&
         preferences_add_number(group, "climax_rise", melody->climax_rise) && preferences_add_number(group, "climax_slack", melody->climax_slack) &&
         add_ratio(group, "second_nearest_chord_tone", melody->second_nearest) &&
         preferences_add_number(group, "attempts", melody->attempts) && preferences_add_number(group, "keep", melody->keep) &&
         cJSON_AddBoolToObject(group, "plain_shaping", melody->plain_shaping) != NULL;
    cJSON *weights = ok ? cJSON_AddObjectToObject(group, "weights") : NULL;
    for (size_t i = 0; weights && i < WEIGHT_COUNT; i++) {
        const unsigned *value = (const unsigned *)(const void *)((const char *)&melody->weights + WEIGHT_KEYS[i].offset);
        ok = ok && preferences_add_number(weights, WEIGHT_KEYS[i].name, *value);
    }
    cJSON *closure = ok && weights ? cJSON_AddObjectToObject(group, "closure") : NULL;
    ok = ok && closure && preferences_add_number(closure, "final", melody->closure_final) &&
         preferences_add_number(closure, "first", melody->closure_first) &&
         preferences_add_number(group, "rhythm_repeat_bars", melody->rhythm_repeat_bars) &&
         preferences_add_number(group, "leaps", melody->leap_weight) && preferences_add_number(group, "register", melody->register_weight) &&
         cJSON_AddBoolToObject(group, "cadence_long", melody->cadence_long) != NULL &&
         preferences_add_number(group, "color_note", melody->color_note);
    cJSON *waive = ok ? cJSON_AddArrayToObject(group, "waive") : NULL;
    for (unsigned i = 0; waive && i < MELODY_RULE_COUNT; i++) {
        if (melody->waive & (1u << i)) {
            cJSON *name = cJSON_CreateString(melody_hard_rule_name((MelodyHardRule)i));
            if (!name || !cJSON_AddItemToArray(waive, name)) {
                cJSON_Delete(name);
                ok = false;
            }
        }
    }
    ok = ok && waive;
    cJSON *reverse = ok ? cJSON_AddArrayToObject(group, "reverse") : NULL;
    for (unsigned i = 0; reverse && i < MELODY_GROUP_COUNT; i++) {
        if (melody->reverse & (1u << i)) {
            cJSON *name = cJSON_CreateString(melody_soft_group_name((MelodySoftGroup)i));
            if (!name || !cJSON_AddItemToArray(reverse, name)) {
                cJSON_Delete(name);
                ok = false;
            }
        }
    }
    ok = ok && reverse;
    if (!ok || !weights) {
        cJSON_Delete(types);
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}

static bool add_weighted_list(cJSON *group, const WeightedList *list, const MotiveRandomPreferences *motif) {
    cJSON *object = cJSON_AddObjectToObject(group, list->key);
    const unsigned long *values = list_values_const(motif, list);
    for (size_t i = 0; object && i < list->count; i++) {
        if (!preferences_add_number(object, list->names[i], (double)values[i])) {
            return false;
        }
    }
    return object != NULL;
}

/* The plan as strings, "II" or "V@1:3": the place only when set (R-195). Nothing is written when no plan is given. */
static bool write_plan(cJSON *group, const MotiveRandomPreferences *motif) {
    cJSON *array;
    if (motif->plan_count == 0) {
        return true;
    }
    array = cJSON_AddArrayToObject(group, "plan");
    for (size_t k = 0; array && k < motif->plan_count; k++) {
        char text[24];
        const MotiveRandomPlanChord *chord = &motif->plan[k];
        if (chord->bar > 0) {
            snprintf(text, sizeof(text), "%s@%u:%u", standard_tonality_roman_numeral_degree(chord->degree, true), chord->bar, chord->beat);
        } else {
            snprintf(text, sizeof(text), "%s", standard_tonality_roman_numeral_degree(chord->degree, true));
        }
        cJSON *string = cJSON_CreateString(text);
        if (!string || !cJSON_AddItemToArray(array, string)) {
            cJSON_Delete(string);
            return false;
        }
    }
    return array != NULL;
}

static cJSON *write_motif(const MotiveRandomPreferences *motif) {
    cJSON *group = cJSON_CreateObject();
    cJSON *leap = NULL;
    bool ok = group != NULL;
    char ratio[32];
    /* The document's order: bars, pickup_one_in, then the lists, with leap between features and rung_moves, and
     * accented_one_in before accented_types (the last list), the other settings after it. */
    for (size_t i = 0; ok && i < MOTIF_LIST_COUNT; i++) {
        if (i == 1) {
            ok = preferences_add_number(group, "pickup_one_in", motif->pickup_one_in);
        }
        if (ok && i == MOTIF_LIST_COUNT - 1) {
            ok = preferences_add_number(group, "accented_one_in", motif->accented_one_in);
        }
        if (ok && i == 6) {
            leap = cJSON_AddObjectToObject(group, "leap");
            ok = leap && preferences_add_number(leap, "low", motif->leap_low) && preferences_add_number(leap, "high", motif->leap_high);
        }
        ok = ok && add_weighted_list(group, &MOTIF_LISTS[i], motif);
    }
    snprintf(ratio, sizeof(ratio), "%lld/%lld", motif->contrast_fidelity_min.num,
             motif->contrast_fidelity_min.den);
    ok = ok && cJSON_AddBoolToObject(group, "avoid_repeats", motif->avoid_repeats) != NULL &&
         preferences_add_number(group, "leaps_per_motif", motif->leaps_per_motif) &&
         cJSON_AddBoolToObject(group, "leap_recovery", motif->leap_recovery) != NULL &&
         cJSON_AddBoolToObject(group, "pickup_from_accent", motif->pickup_from_accent) != NULL &&
         cJSON_AddStringToObject(group, "contrast_fidelity_min", ratio) != NULL &&
         preferences_add_number(group, "long_on_weak", (double)motif->long_on_weak) &&
         cJSON_AddBoolToObject(group, "final_long", motif->final_long) != NULL &&
         cJSON_AddBoolToObject(group, "compound_beats", motif->compound_beats) != NULL;
    /* endings, long_nct_one_in, then the four long_nct lists: the order of the document. */
    for (size_t i = 0; ok && i < MOTIF_LIST_L2_COUNT; i++) {
        if (i == 1) {
            ok = preferences_add_number(group, "long_nct_one_in", motif->long_nct_one_in);
        }
        ok = ok && add_weighted_list(group, &MOTIF_LISTS_L2[i], motif);
    }
    /* Phase L3: syncopation_one_in, its two lists, skips_per_motif, and the plan when one is given. */
    ok = ok && preferences_add_number(group, "syncopation_one_in", motif->syncopation_one_in);
    for (size_t i = 0; ok && i < MOTIF_LIST_L3_COUNT; i++) {
        ok = add_weighted_list(group, &MOTIF_LISTS_L3[i], motif);
    }
    ok = ok && preferences_add_number(group, "skips_per_motif", motif->skips_per_motif) &&
         preferences_add_number(group, "color_note", motif->color_note) && write_plan(group, motif);
    if (!ok) {
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}

cJSON *composer_preferences_write(const ComposerPreferences *preferences) {
    cJSON *document = cJSON_CreateObject();
    cJSON *melody = document ? write_melody(&preferences->melody) : NULL;
    if (!melody || !cJSON_AddItemToObject(document, "melody", melody)) {
        cJSON_Delete(melody);
        cJSON_Delete(document);
        return NULL;
    }
    cJSON *motif = write_motif(&preferences->motif);
    if (!motif || !cJSON_AddItemToObject(document, "motif", motif)) {
        cJSON_Delete(motif);
        cJSON_Delete(document);
        return NULL;
    }
    cJSON *development = preferences_write_development(preferences);
    if (!development || !cJSON_AddItemToObject(document, "development", development)) {
        cJSON_Delete(development);
        cJSON_Delete(document);
        return NULL;
    }
    cJSON *candidates = cJSON_AddObjectToObject(document, "candidates");
    if (!candidates || !preferences_add_number(candidates, "variety", preferences->candidates.variety) ||
        !preferences_add_number(candidates, "pool", preferences->candidates.pool)) {
        cJSON_Delete(document);
        return NULL;
    }
    cJSON *critic = preferences_write_critic(preferences);
    if (!critic || !cJSON_AddItemToObject(document, "critic", critic)) {
        cJSON_Delete(critic);
        cJSON_Delete(document);
        return NULL;
    }
    cJSON *harmony = preferences_write_harmony(preferences);
    if (!harmony || !cJSON_AddItemToObject(document, "harmony", harmony)) {
        cJSON_Delete(harmony);
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* ------------------------------------------------------------------- the key and rule tables (R-672) */

static const char *const WORDS_NCT[] = {"passing", "neighbor", "appoggiatura", "suspension", "anticipation", NULL};
static const char *const WORDS_WAIVE[] = {"forbidden-leap",         "forbidden-outline",      "no-crossing", "parallel-octaves",
                                          "parallel-fifths",        "dissonance-against-bass", "strong-beat-chord-tone", NULL};
static const char *const WORDS_CRITIC_REVERSE[] = {"rhythmic_monotony", "contrast_distance", "leap_load",      "open_ending",
                                                   "stray_syncopation", "one_rhythm",        "repeated_notes", "wide_range", NULL};
static const char *const WORDS_MIXTURE_FROM[] = {"parallel", "one step darker", "one step brighter", "two steps darker", "two steps brighter", NULL};
static const char *const WORDS_REVERSE[] = {"melodic",  "against_bass", "tendency", "climax",   "contour", "ending_reduction", "waves",
                                            "central_range", "monotony", "seam",    "closure", "leaps",   "register", NULL};

/*
 * One row per leaf of the default document, in the order the document writes them, then motif.plan (the one setting the
 * default document does not hold). A hard key shapes what a request can ask for or what a candidate may be (R-671); the
 * rest are soft: a preference a listener can move.
 */
static const ComposerPreferenceKey KEYS[] = {
    {"melody.range.low", COMPOSER_KEY_INTEGER, RANGE_LOW_MIN, RANGE_LOW_MAX, true, NULL},
    {"melody.range.high", COMPOSER_KEY_INTEGER, RANGE_LOW_MIN + RANGE_SPAN_MIN, RANGE_HIGH_MAX, true, NULL},
    {"melody.nct_types", COMPOSER_KEY_WORDS, 0, 0, true, WORDS_NCT},
    {"melody.density", COMPOSER_KEY_INTEGER, 0, DENSITY_MAX, false, NULL},
    {"melody.climax_position", COMPOSER_KEY_INTEGER, 0, SCALE_MAX, false, NULL},
    {"melody.climax_rise", COMPOSER_KEY_INTEGER, CLIMAX_RISE_MIN, CLIMAX_RISE_MAX, false, NULL},
    {"melody.climax_slack", COMPOSER_KEY_INTEGER, 0, SCALE_MAX, false, NULL},
    {"melody.second_nearest_chord_tone", COMPOSER_KEY_RATIO, 0, RATIO_DENOMINATOR_MAX, false, NULL},
    {"melody.attempts", COMPOSER_KEY_INTEGER, 1, ATTEMPTS_MAX, false, NULL},
    {"melody.keep", COMPOSER_KEY_INTEGER, 1, MELODY_KEEP_MAX, false, NULL},
    {"melody.plain_shaping", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"melody.weights.melodic", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.against_bass", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.tendency", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.climax", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.contour", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.ending_reduction", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.waves", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.central_range", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.monotony", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.weights.seam", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.closure.final", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.closure.first", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.rhythm_repeat_bars", COMPOSER_KEY_INTEGER, 0, RHYTHM_REPEAT_BARS_MAX, true, NULL},
    {"melody.leaps", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.register", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"melody.cadence_long", COMPOSER_KEY_BOOLEAN, 0, 0, true, NULL},
    {"melody.color_note", COMPOSER_KEY_INTEGER, 0, COLOR_NOTE_MAX, false, NULL},
    {"melody.waive", COMPOSER_KEY_WORDS, 0, 0, true, WORDS_WAIVE},
    {"melody.reverse", COMPOSER_KEY_WORDS, 0, 0, false, WORDS_REVERSE},
    {"motif.bars.one", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.bars.two", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.pickup_one_in", COMPOSER_KEY_INTEGER, 0, PICKUP_ONE_IN_MAX, false, NULL},
    {"motif.shapes_one_bar.repeat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.shapes_one_bar.contrast", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.shapes_two_bars.repeat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.shapes_two_bars.contrast", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.shapes_two_bars.repeat_contrast", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.plans_two.I-V", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.plans_two.I-IV", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.plans_two.I-I", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.plans_three.I-V-I", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.plans_three.I-IV-I", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.features.dotted", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.features.leap", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.features.repeat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.leap.low", COMPOSER_KEY_INTEGER, LEAP_MIN, LEAP_MAX, true, NULL},
    {"motif.leap.high", COMPOSER_KEY_INTEGER, LEAP_MIN, LEAP_MAX, true, NULL},
    {"motif.rung_moves.step", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.rung_moves.skip", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.decorations.chord_tone", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.decorations.passing", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.decorations.neighbor", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.decorations.appoggiatura", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.decorations.escape", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.half_beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.two_beats", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.quarter_beats", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.long_short", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.short_long", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.triplet", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.durations.triplet_two_beats", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.accented_one_in", COMPOSER_KEY_INTEGER, 0, ACCENTED_ONE_IN_MAX, false, NULL},
    {"motif.accented_types.appoggiatura", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.accented_types.passing", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.accented_types.suspension", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.avoid_repeats", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"motif.leaps_per_motif", COMPOSER_KEY_INTEGER, 0, LEAPS_PER_MOTIF_MAX, true, NULL},
    {"motif.leap_recovery", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"motif.pickup_from_accent", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"motif.contrast_fidelity_min", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"motif.long_on_weak", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.final_long", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"motif.compound_beats", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"motif.endings.free", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.endings.closed", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.endings.open", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.endings.lead_in", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_one_in", COMPOSER_KEY_INTEGER, 0, LONG_NCT_ONE_IN_MAX, false, NULL},
    {"motif.long_nct_types.appoggiatura", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_types.suspension", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_types.retardation", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_types.passing", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_types.neighbor", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_intervals.second", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_intervals.fourth", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_intervals.sixth", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_intervals.seventh", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_beats.downbeat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_beats.middle", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_beats.other", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_lengths.beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_lengths.dotted_beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.long_nct_lengths.two_beats", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.syncopation_one_in", COMPOSER_KEY_INTEGER, 0, SYNCOPATION_ONE_IN_MAX, false, NULL},
    {"motif.syncopation_types.off_beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.syncopation_types.tie_beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.syncopation_types.tie_bar", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.syncopation_types.anticipation", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.syncopation_levels.eighth", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.syncopation_levels.sixteenth", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"motif.skips_per_motif", COMPOSER_KEY_INTEGER, 0, SKIPS_PER_MOTIF_MAX, true, NULL},
    {"motif.color_note", COMPOSER_KEY_INTEGER, 0, MOTIF_COLOR_NOTE_MAX, false, NULL},
    {"motif.plan", COMPOSER_KEY_PLAN, 0, 0, true, NULL},
    {"development.fidelity_minimum.melody.presentation", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.melody.continuation", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.melody.cadential", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.balanced.presentation", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.balanced.continuation", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.balanced.cadential", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.theme.presentation", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.theme.continuation", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.fidelity_minimum.theme.cadential", COMPOSER_KEY_RATIO, 0, FIDELITY_DENOMINATOR_MAX, true, NULL},
    {"development.continuation.every_segment", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.continuation.fragment_lengths.segment", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"development.continuation.fragment_lengths.half_bar", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"development.continuation.fragment_lengths.beat", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"development.continuation.diminish", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.continuation.liquidate", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.continuation.faster_harmony", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.basic_as_written", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.contrast.distance.similar", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"development.contrast.distance.distant", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"development.contrast.new_material", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"development.functions.harmony", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.functions.surface", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"development.functions.contrast", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"candidates.variety", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"candidates.pool", COMPOSER_KEY_INTEGER, POOL_MIN, POOL_MAX, false, NULL},
    {"critic.rank", COMPOSER_KEY_BOOLEAN, 0, 0, false, NULL},
    {"critic.candidates", COMPOSER_KEY_INTEGER, POOL_MIN, POOL_MAX, false, NULL},
    {"critic.keep_rejected", COMPOSER_KEY_INTEGER, 0, COMPOSER_PREFERENCES_KEEP_REJECTED_MAX, false, NULL},
    {"critic.reverse", COMPOSER_KEY_WORDS, 0, 0, false, WORDS_CRITIC_REVERSE},
    {"critic.weights.rhythmic_monotony", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.contrast_distance", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.leap_load", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.open_ending", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.stray_syncopation", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.one_rhythm", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.repeated_notes", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"critic.weights.wide_range", COMPOSER_KEY_INTEGER, 0, WEIGHT_MAX, false, NULL},
    {"harmony.mixture.share", COMPOSER_KEY_RATIO, 0, 16, false, NULL},
    {"harmony.mixture.from", COMPOSER_KEY_WORDS, 0, 0, false, WORDS_MIXTURE_FROM}};

const ComposerPreferenceKey *composer_preferences_keys(size_t *count) {
    if (count) {
        *count = sizeof(KEYS) / sizeof(KEYS[0]);
    }
    return KEYS;
}

/* The 13 soft groups, then the 7 hard rules, in enum order; each names the setting that holds it (R-672). */
static const ComposerRule RULES[] = {
    {"melodic", false, "melody.weights.melodic"},
    {"against_bass", false, "melody.weights.against_bass"},
    {"tendency", false, "melody.weights.tendency"},
    {"climax", false, "melody.weights.climax"},
    {"contour", false, "melody.weights.contour"},
    {"ending_reduction", false, "melody.weights.ending_reduction"},
    {"waves", false, "melody.weights.waves"},
    {"central_range", false, "melody.weights.central_range"},
    {"monotony", false, "melody.weights.monotony"},
    {"seam", false, "melody.weights.seam"},
    {"closure", false, "melody.closure"},
    {"leaps", false, "melody.leaps"},
    {"register", false, "melody.register"},
    {"forbidden-leap", true, "melody.waive"},
    {"forbidden-outline", true, "melody.waive"},
    {"no-crossing", true, "melody.waive"},
    {"parallel-octaves", true, "melody.waive"},
    {"parallel-fifths", true, "melody.waive"},
    {"dissonance-against-bass", true, "melody.waive"},
    {"strong-beat-chord-tone", true, "melody.waive"},
    {"rhythmic_monotony", false, "critic.weights.rhythmic_monotony"},
    {"contrast_distance", false, "critic.weights.contrast_distance"},
    {"leap_load", false, "critic.weights.leap_load"},
    {"open_ending", false, "critic.weights.open_ending"},
    {"stray_syncopation", false, "critic.weights.stray_syncopation"},
    {"one_rhythm", false, "critic.weights.one_rhythm"},
    {"repeated_notes", false, "critic.weights.repeated_notes"},
    {"wide_range", false, "critic.weights.wide_range"}};

const ComposerRule *composer_preferences_rules(size_t *count) {
    if (count) {
        *count = sizeof(RULES) / sizeof(RULES[0]);
    }
    return RULES;
}
