#include <ursatz_composer/preferences.h>

#include "grow/grow_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define WEIGHT_MAX COMPOSER_PREFERENCES_WEIGHT_MAX
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
#define LEAPS_PER_MOTIF_MAX COMPOSER_PREFERENCES_LEAPS_PER_MOTIF_MAX
#define LEAP_MIN COMPOSER_PREFERENCES_LEAP_MIN
#define LEAP_MAX COMPOSER_PREFERENCES_LEAP_MAX
#define FIDELITY_DENOMINATOR_MAX COMPOSER_PREFERENCES_FIDELITY_DENOMINATOR_MAX

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
}

/* Where a refusal goes: the path and the message of the caller, which may both be absent. */
typedef struct {
    char *path;
    size_t path_size;
    char *message;
    size_t message_size;
} Refusal;

/* Refuses with the path under the document, "melody/keep" for /melody/keep; "" is the document itself. */
static bool refuse(const Refusal *refusal, const char *where, const char *limit) {
    if (refusal->path && refusal->path_size > 0) {
        snprintf(refusal->path, refusal->path_size, "%s%s", where[0] ? "/" : "", where);
    }
    if (refusal->message && refusal->message_size > 0) {
        snprintf(refusal->message, refusal->message_size, "%s", limit);
    }
    return false;
}

/* The key's path under a group: "keep" under "melody" for /melody/keep, "leap/high" under "motif" for /motif/leap/high. */
static bool refuse_in(const Refusal *refusal, const char *group, const char *key, const char *limit) {
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", group, key);
    return refuse(refusal, path, limit);
}

static bool refuse_melody(const Refusal *refusal, const char *key, const char *limit) {
    return refuse_in(refusal, "melody", key, limit);
}

static bool whole_number(const cJSON *item, double low, double high, double *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble < low || item->valuedouble > high ||
        item->valuedouble != (double)(long long)item->valuedouble) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

/* An unsigned setting from a whole number low to high; says which limit it broke when it is refused. */
static bool read_unsigned_in(const cJSON *item, const char *group, const char *key, unsigned low, unsigned high,
                             unsigned *out, const Refusal *refusal) {
    double value;
    char limit[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (whole_number(item, low, high, &value)) {
        *out = (unsigned)value;
        return true;
    }
    snprintf(limit, sizeof(limit), "must be a whole number from %u to %u", low, high);
    return refuse_in(refusal, group, key, limit);
}

static bool read_unsigned(const cJSON *item, const char *key, unsigned low, unsigned high, unsigned *out,
                          const Refusal *refusal) {
    return read_unsigned_in(item, "melody", key, low, high, out, refusal);
}

/* "n/d" with 0 <= n <= d and 1 <= d <= denominator_max, digits only. */
static bool parse_ratio(const char *text, unsigned long denominator_max, ComposerRatio *out) {
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
    out->numerator = numerator;
    out->denominator = denominator;
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

static bool read_ratio(const cJSON *item, const char *key, ComposerRatio *out, const Refusal *refusal) {
    if (!cJSON_IsString(item) || !parse_ratio(item->valuestring, RATIO_DENOMINATOR_MAX, out)) {
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

static bool read_melody(const cJSON *group, MelodyFillParams *melody, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return refuse(refusal, "melody", "must be an object");
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
        return refuse_in(refusal, "motif", list->key, "must be an object of whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(entry, item) {
        bool known = false;
        for (size_t i = 0; i < list->count && !known; i++) {
            if (strcmp(entry->string, list->names[i]) == 0) {
                unsigned value;
                snprintf(key, sizeof(key), "%s/%s", list->key, entry->string);
                if (!read_unsigned_in(entry, "motif", key, 0, WEIGHT_MAX, &value, refusal)) {
                    return false;
                }
                values[i] = value;
                known = true;
            }
        }
        if (!known) {
            snprintf(key, sizeof(key), "%s/%s", list->key, entry->string);
            return refuse_in(refusal, "motif", key, "unknown member");
        }
    }
    unsigned long sum = 0;
    for (size_t i = 0; i < list->count; i++) {
        sum += values[i];
    }
    if (sum == 0) {
        return refuse_in(refusal, "motif", list->key, "the weights must sum to more than 0");
    }
    return true;
}

static bool read_leap(const cJSON *item, MotiveRandomPreferences *motif, const Refusal *refusal) {
    int low = motif->leap_low;
    int high = motif->leap_high;
    const cJSON *entry;
    if (!cJSON_IsObject(item)) {
        return refuse_in(refusal, "motif", "leap", "must be an object with low and high");
    }
    cJSON_ArrayForEach(entry, item) {
        unsigned value;
        if (strcmp(entry->string, "low") == 0) {
            if (!read_unsigned_in(entry, "motif", "leap/low", LEAP_MIN, LEAP_MAX, &value, refusal)) {
                return false;
            }
            low = (int)value;
        } else if (strcmp(entry->string, "high") == 0) {
            if (!read_unsigned_in(entry, "motif", "leap/high", LEAP_MIN, LEAP_MAX, &value, refusal)) {
                return false;
            }
            high = (int)value;
        } else {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "leap/%s", entry->string);
            return refuse_in(refusal, "motif", key, "unknown member");
        }
    }
    if (high < low) {
        return refuse_in(refusal, "motif", "leap/high", "must be at least leap.low");
    }
    motif->leap_low = low;
    motif->leap_high = high;
    return true;
}

static bool read_motif_bool(const cJSON *item, const char *key, bool *out, const Refusal *refusal) {
    if (!cJSON_IsBool(item)) {
        return refuse_in(refusal, "motif", key, "must be true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

static bool read_contrast_fidelity_min(const cJSON *item, MotiveRandomPreferences *motif, const Refusal *refusal) {
    ComposerRatio ratio;
    if (!cJSON_IsString(item) || !parse_ratio(item->valuestring, FIDELITY_DENOMINATOR_MAX, &ratio)) {
        return refuse_in(refusal, "motif", "contrast_fidelity_min", "must be a ratio n/d from 0 to 1 with 1 <= d <= 1000");
    }
    motif->contrast_fidelity_min = ratio;
    return true;
}

static bool read_motif(const cJSON *group, MotiveRandomPreferences *motif, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return refuse(refusal, "motif", "must be an object");
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
        if (known) {
            continue;
        }
        if (strcmp(key, "pickup_one_in") == 0) {
            if (!read_unsigned_in(item, "motif", key, 0, PICKUP_ONE_IN_MAX, &motif->pickup_one_in, refusal)) {
                return false;
            }
        } else if (strcmp(key, "leap") == 0) {
            if (!read_leap(item, motif, refusal)) {
                return false;
            }
        } else if (strcmp(key, "accented_one_in") == 0) {
            if (!read_unsigned_in(item, "motif", key, 0, ACCENTED_ONE_IN_MAX, &motif->accented_one_in, refusal)) {
                return false;
            }
        } else if (strcmp(key, "leaps_per_motif") == 0) {
            if (!read_unsigned_in(item, "motif", key, 0, LEAPS_PER_MOTIF_MAX, &motif->leaps_per_motif, refusal)) {
                return false;
            }
        } else if (strcmp(key, "long_on_weak") == 0) {
            unsigned value;
            if (!read_unsigned_in(item, "motif", key, 0, WEIGHT_MAX, &value, refusal)) {
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
        } else if (strcmp(key, "contrast_fidelity_min") == 0) {
            if (!read_contrast_fidelity_min(item, motif, refusal)) {
                return false;
            }
        } else {
            return refuse_in(refusal, "motif", key, "unknown member");
        }
    }
    return true;
}

/* ------------------------------------------------------- the development group */

/* The settings of the fidelity table, in the order the document writes them, and the enum each stands for. */
static const struct {
    const char *name;
    GrowDevelopment development;
} DEVELOPMENT_KEYS[] = {{"melody", GROW_DEVELOP_MELODY}, {"balanced", GROW_DEVELOP_BALANCED}, {"theme", GROW_DEVELOP_THEME}};
#define DEVELOPMENT_COUNT (sizeof(DEVELOPMENT_KEYS) / sizeof(DEVELOPMENT_KEYS[0]))

/* The roles with a minimum; none and contrast are 0 and are not settings. */
static const struct {
    const char *name;
    GrowIdeaRole role;
} ROLE_KEYS[] = {{"presentation", GROW_ROLE_PRESENTATION},
                 {"continuation", GROW_ROLE_CONTINUATION},
                 {"cadential", GROW_ROLE_CADENTIAL}};
#define ROLE_COUNT (sizeof(ROLE_KEYS) / sizeof(ROLE_KEYS[0]))

static bool read_fidelity_setting(const cJSON *setting, const char *name, GrowDevelopment development,
                                  GrowFidelityTable *table, const Refusal *refusal) {
    const cJSON *entry;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    if (!cJSON_IsObject(setting)) {
        snprintf(path, sizeof(path), "development/fidelity_minimum/%s", name);
        return refuse(refusal, path, "must be an object with presentation, continuation and cadential");
    }
    cJSON_ArrayForEach(entry, setting) {
        bool known = false;
        for (size_t i = 0; i < ROLE_COUNT && !known; i++) {
            if (strcmp(entry->string, ROLE_KEYS[i].name) == 0) {
                ComposerRatio ratio;
                known = true;
                if (!cJSON_IsString(entry) || !parse_ratio(entry->valuestring, FIDELITY_DENOMINATOR_MAX, &ratio) ||
                    ratio.denominator < 1) {
                    snprintf(path, sizeof(path), "development/fidelity_minimum/%s/%s", name, entry->string);
                    return refuse(refusal, path, "must be a ratio n/d from 0 to 1 with 1 <= d <= 1000");
                }
                table->minimum[development][ROLE_KEYS[i].role].numerator = ratio.numerator;
                table->minimum[development][ROLE_KEYS[i].role].denominator = ratio.denominator;
            }
        }
        if (!known) {
            snprintf(path, sizeof(path), "development/fidelity_minimum/%s/%s", name, entry->string);
            return refuse(refusal, path, "unknown member");
        }
    }
    return true;
}

static bool read_development(const cJSON *group, GrowFidelityTable *table, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return refuse(refusal, "development", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        const cJSON *setting;
        if (strcmp(item->string, "fidelity_minimum") != 0) {
            return refuse_in(refusal, "development", item->string, "unknown member");
        }
        if (!cJSON_IsObject(item)) {
            return refuse_in(refusal, "development", "fidelity_minimum", "must be an object of melody, balanced and theme");
        }
        cJSON_ArrayForEach(setting, item) {
            bool known = false;
            for (size_t i = 0; i < DEVELOPMENT_COUNT && !known; i++) {
                if (strcmp(setting->string, DEVELOPMENT_KEYS[i].name) == 0) {
                    known = true;
                    if (!read_fidelity_setting(setting, DEVELOPMENT_KEYS[i].name, DEVELOPMENT_KEYS[i].development, table,
                                               refusal)) {
                        return false;
                    }
                }
            }
            if (!known) {
                char key[COMPOSER_PREFERENCES_PATH_MAX];
                snprintf(key, sizeof(key), "fidelity_minimum/%s", setting->string);
                return refuse_in(refusal, "development", key, "unknown member");
            }
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
        return refuse(&refusal, "", "preferences must be an object");
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
            if (!read_development(item, &out->development, &refusal)) {
                return false;
            }
        } else {
            return refuse(&refusal, item->string, "unknown member");
        }
    }
    return true;
}

static bool add_number(cJSON *object, const char *name, double value) {
    return cJSON_AddNumberToObject(object, name, value) != NULL;
}

static bool add_ratio(cJSON *object, const char *name, ComposerRatio ratio) {
    char text[32];
    snprintf(text, sizeof(text), "%lu/%lu", ratio.numerator, ratio.denominator);
    return cJSON_AddStringToObject(object, name, text) != NULL;
}

static cJSON *write_melody(const MelodyFillParams *melody) {
    cJSON *group = cJSON_CreateObject();
    cJSON *range = group ? cJSON_AddObjectToObject(group, "range") : NULL;
    cJSON *types = group ? cJSON_CreateArray() : NULL;
    bool ok = range && types && add_number(range, "low", melody->range_low) && add_number(range, "high", melody->range_high);
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
    ok = ok && add_number(group, "density", melody->density) && add_number(group, "climax_position", melody->climax_position) &&
         add_number(group, "climax_rise", melody->climax_rise) && add_number(group, "climax_slack", melody->climax_slack) &&
         add_ratio(group, "second_nearest_chord_tone", melody->second_nearest) &&
         add_number(group, "attempts", melody->attempts) && add_number(group, "keep", melody->keep) &&
         cJSON_AddBoolToObject(group, "plain_shaping", melody->plain_shaping) != NULL;
    cJSON *weights = ok ? cJSON_AddObjectToObject(group, "weights") : NULL;
    for (size_t i = 0; weights && i < WEIGHT_COUNT; i++) {
        const unsigned *value = (const unsigned *)(const void *)((const char *)&melody->weights + WEIGHT_KEYS[i].offset);
        ok = ok && add_number(weights, WEIGHT_KEYS[i].name, *value);
    }
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
    /* The five new durations are read but not written until the document carries them (phase L2, B6): the document is as it was. */
    size_t count = strcmp(list->key, "durations") == 0 ? 3 : list->count;
    for (size_t i = 0; object && i < count; i++) {
        if (!add_number(object, list->names[i], (double)values[i])) {
            return false;
        }
    }
    return object != NULL;
}

static cJSON *write_motif(const MotiveRandomPreferences *motif) {
    cJSON *group = cJSON_CreateObject();
    cJSON *leap = NULL;
    bool ok = group != NULL;
    char ratio[32];
    /* The document's order: bars, pickup_one_in, then the lists, with leap between features and rung_moves, and
     * accented_one_in before accented_types (the last list), the other new settings after it. */
    for (size_t i = 0; ok && i < MOTIF_LIST_COUNT; i++) {
        if (i == 1) {
            ok = add_number(group, "pickup_one_in", motif->pickup_one_in);
        }
        if (ok && i == MOTIF_LIST_COUNT - 1) {
            ok = add_number(group, "accented_one_in", motif->accented_one_in);
        }
        if (ok && i == 6) {
            leap = cJSON_AddObjectToObject(group, "leap");
            ok = leap && add_number(leap, "low", motif->leap_low) && add_number(leap, "high", motif->leap_high);
        }
        ok = ok && add_weighted_list(group, &MOTIF_LISTS[i], motif);
    }
    snprintf(ratio, sizeof(ratio), "%lu/%lu", motif->contrast_fidelity_min.numerator,
             motif->contrast_fidelity_min.denominator);
    ok = ok && cJSON_AddBoolToObject(group, "avoid_repeats", motif->avoid_repeats) != NULL &&
         add_number(group, "leaps_per_motif", motif->leaps_per_motif) &&
         cJSON_AddBoolToObject(group, "leap_recovery", motif->leap_recovery) != NULL &&
         cJSON_AddBoolToObject(group, "pickup_from_accent", motif->pickup_from_accent) != NULL &&
         cJSON_AddStringToObject(group, "contrast_fidelity_min", ratio) != NULL &&
         add_number(group, "long_on_weak", (double)motif->long_on_weak) &&
         cJSON_AddBoolToObject(group, "final_long", motif->final_long) != NULL;
    if (!ok) {
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}

static cJSON *write_development(const GrowFidelityTable *table) {
    cJSON *group = cJSON_CreateObject();
    cJSON *minimum = group ? cJSON_AddObjectToObject(group, "fidelity_minimum") : NULL;
    bool ok = minimum != NULL;
    for (size_t d = 0; ok && d < DEVELOPMENT_COUNT; d++) {
        cJSON *setting = cJSON_AddObjectToObject(minimum, DEVELOPMENT_KEYS[d].name);
        ok = setting != NULL;
        for (size_t r = 0; ok && r < ROLE_COUNT; r++) {
            const MotiveScore *score = &table->minimum[DEVELOPMENT_KEYS[d].development][ROLE_KEYS[r].role];
            char text[32];
            snprintf(text, sizeof(text), "%lu/%lu", score->numerator, score->denominator);
            ok = cJSON_AddStringToObject(setting, ROLE_KEYS[r].name, text) != NULL;
        }
    }
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
    cJSON *development = write_development(&preferences->development);
    if (!development || !cJSON_AddItemToObject(document, "development", development)) {
        cJSON_Delete(development);
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

static bool melody_is_default(const MelodyFillParams *melody) {
    MelodyFillParams defaults = melody_fill_params_default();
    const MelodyWeights *a = &melody->weights;
    const MelodyWeights *b = &defaults.weights;
    return melody->range_low == defaults.range_low && melody->range_high == defaults.range_high &&
           melody->start_cell == defaults.start_cell && melody->nct_types == defaults.nct_types &&
           melody->density == defaults.density && melody->climax_position == defaults.climax_position &&
           melody->plain_shaping == defaults.plain_shaping && melody->climax_rise == defaults.climax_rise &&
           melody->climax_slack == defaults.climax_slack &&
           melody->second_nearest.numerator == defaults.second_nearest.numerator &&
           melody->second_nearest.denominator == defaults.second_nearest.denominator &&
           melody->attempts == defaults.attempts && melody->keep == defaults.keep && a->melodic == b->melodic &&
           a->against_bass == b->against_bass && a->tendency == b->tendency && a->climax == b->climax &&
           a->contour == b->contour && a->ending_reduction == b->ending_reduction && a->waves == b->waves &&
           a->central_range == b->central_range && a->monotony == b->monotony && a->seam == b->seam;
}

static bool motif_is_default(const MotiveRandomPreferences *motif) {
    MotiveRandomPreferences defaults;
    motive_random_preferences_default(&defaults);
    for (size_t i = 0; i < MOTIF_LIST_COUNT; i++) {
        const unsigned long *a = list_values_const(motif, &MOTIF_LISTS[i]);
        const unsigned long *b = list_values_const(&defaults, &MOTIF_LISTS[i]);
        for (size_t k = 0; k < MOTIF_LISTS[i].count; k++) {
            if (a[k] != b[k]) {
                return false;
            }
        }
    }
    return motif->pickup_one_in == defaults.pickup_one_in && motif->leap_low == defaults.leap_low &&
           motif->leap_high == defaults.leap_high && motif->accented_one_in == defaults.accented_one_in &&
           motif->avoid_repeats == defaults.avoid_repeats && motif->leaps_per_motif == defaults.leaps_per_motif &&
           motif->leap_recovery == defaults.leap_recovery && motif->pickup_from_accent == defaults.pickup_from_accent &&
           motif->contrast_fidelity_min.numerator == defaults.contrast_fidelity_min.numerator &&
           motif->contrast_fidelity_min.denominator == defaults.contrast_fidelity_min.denominator &&
           motif->long_on_weak == defaults.long_on_weak && motif->final_long == defaults.final_long;
}

static bool development_is_default(const GrowFidelityTable *table) {
    GrowFidelityTable defaults;
    grow_fidelity_table_default(&defaults);
    for (size_t d = 0; d < GROW_DEVELOP_COUNT; d++) {
        for (size_t r = 0; r <= GROW_ROLE_CONTRAST; r++) {
            if (table->minimum[d][r].numerator != defaults.minimum[d][r].numerator ||
                table->minimum[d][r].denominator != defaults.minimum[d][r].denominator) {
                return false;
            }
        }
    }
    return true;
}

bool composer_preferences_is_default(const ComposerPreferences *preferences) {
    return melody_is_default(&preferences->melody) && motif_is_default(&preferences->motif) &&
           development_is_default(&preferences->development);
}
