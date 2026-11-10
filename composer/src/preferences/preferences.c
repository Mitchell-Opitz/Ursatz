#include <ursatz_composer/preferences.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define WEIGHT_MAX 20
#define SCALE_MAX 1000
#define RATIO_DENOMINATOR_MAX 16
#define ATTEMPTS_MAX 256
#define RANGE_LOW_MIN 36
#define RANGE_LOW_MAX 96
#define RANGE_HIGH_MAX 108
#define RANGE_SPAN_MIN 12
#define DENSITY_MAX 4
#define CLIMAX_RISE_MIN 1
#define CLIMAX_RISE_MAX 12

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
    out->melody = melody_fill_params_default();
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

/* The key's path under "melody": "keep" for /melody/keep, "range/high" for /melody/range/high. */
static bool refuse_melody(const Refusal *refusal, const char *key, const char *limit) {
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    snprintf(path, sizeof(path), "melody/%s", key);
    return refuse(refusal, path, limit);
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
static bool read_unsigned(const cJSON *item, const char *key, unsigned low, unsigned high, unsigned *out,
                          const Refusal *refusal) {
    double value;
    char limit[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (whole_number(item, low, high, &value)) {
        *out = (unsigned)value;
        return true;
    }
    snprintf(limit, sizeof(limit), "must be a whole number from %u to %u", low, high);
    return refuse_melody(refusal, key, limit);
}

/* "n/d" with 0 <= n <= d and 1 <= d <= 16, digits only. */
static bool parse_ratio(const char *text, ComposerRatio *out) {
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
    if (digits == 0 || text[i] != '\0' || denominator < 1 || denominator > RATIO_DENOMINATOR_MAX || numerator > denominator) {
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
    if (!cJSON_IsString(item) || !parse_ratio(item->valuestring, out)) {
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

cJSON *composer_preferences_write(const ComposerPreferences *preferences) {
    cJSON *document = cJSON_CreateObject();
    cJSON *melody = document ? write_melody(&preferences->melody) : NULL;
    if (!melody || !cJSON_AddItemToObject(document, "melody", melody)) {
        cJSON_Delete(melody);
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

bool composer_preferences_is_default(const ComposerPreferences *preferences) {
    return melody_is_default(&preferences->melody);
}
