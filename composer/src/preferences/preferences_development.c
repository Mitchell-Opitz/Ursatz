#include "preferences/preferences_internal.h"

#include <stdio.h>
#include <string.h>

#include "grow/grow_internal.h"

/* The development group of the preferences document: the fidelity minimums, the continuation, the basic idea as written and the contrast. */

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
        return preferences_refuse(refusal, path, "must be an object with presentation, continuation and cadential");
    }
    cJSON_ArrayForEach(entry, setting) {
        bool known = false;
        for (size_t i = 0; i < ROLE_COUNT && !known; i++) {
            if (strcmp(entry->string, ROLE_KEYS[i].name) == 0) {
                Rational ratio;
                known = true;
                if (!cJSON_IsString(entry) || !preferences_parse_ratio(entry->valuestring, FIDELITY_DENOMINATOR_MAX, &ratio) ||
                    ratio.den < 1) {
                    snprintf(path, sizeof(path), "development/fidelity_minimum/%s/%s", name, entry->string);
                    return preferences_refuse(refusal, path, "must be a ratio n/d from 0 to 1 with 1 <= d <= 1000");
                }
                table->minimum[development][ROLE_KEYS[i].role].numerator = ratio.num;
                table->minimum[development][ROLE_KEYS[i].role].denominator = ratio.den;
            }
        }
        if (!known) {
            snprintf(path, sizeof(path), "development/fidelity_minimum/%s/%s", name, entry->string);
            return preferences_refuse(refusal, path, "unknown member");
        }
    }
    return true;
}

/* The weights of the continuation's fragment lengths, in the order the document writes them. */
static const char *const FRAGMENT_LENGTH_NAMES[GROW_FRAGMENT_LENGTH_COUNT] = {"segment", "half_bar", "beat"};

static bool read_fragment_lengths(const cJSON *item, GrowContinuation *continuation, const Refusal *refusal) {
    const char *where = "development/continuation/fragment_lengths";
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    const cJSON *entry;
    unsigned sum = 0;
    double value;
    if (!cJSON_IsObject(item)) {
        return preferences_refuse(refusal, where, "must be an object of whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(entry, item) {
        bool known = false;
        for (size_t i = 0; i < GROW_FRAGMENT_LENGTH_COUNT && !known; i++) {
            if (strcmp(entry->string, FRAGMENT_LENGTH_NAMES[i]) == 0) {
                known = true;
                snprintf(path, sizeof(path), "%s/%s", where, entry->string);
                if (!preferences_whole_number(entry, 0, WEIGHT_MAX, &value)) {
                    return preferences_refuse(refusal, path, "must be a whole number from 0 to 20");
                }
                continuation->fragment_lengths[i] = (unsigned)entry->valuedouble;
            }
        }
        if (!known) {
            snprintf(path, sizeof(path), "%s/%s", where, entry->string);
            return preferences_refuse(refusal, path, "unknown name");
        }
    }
    for (size_t i = 0; i < GROW_FRAGMENT_LENGTH_COUNT; i++) {
        sum += continuation->fragment_lengths[i];
    }
    if (sum == 0) {
        return preferences_refuse(refusal, where, "the weights must sum to more than 0");
    }
    return true;
}

static bool read_continuation(const cJSON *group, GrowContinuation *continuation, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "development/continuation", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        bool *flag = NULL;
        char path[COMPOSER_PREFERENCES_PATH_MAX];
        if (strcmp(item->string, "fragment_lengths") == 0) {
            if (!read_fragment_lengths(item, continuation, refusal)) {
                return false;
            }
            continue;
        }
        if (strcmp(item->string, "every_segment") == 0) {
            flag = &continuation->every_segment;
        } else if (strcmp(item->string, "diminish") == 0) {
            flag = &continuation->diminish;
        } else if (strcmp(item->string, "liquidate") == 0) {
            flag = &continuation->liquidate;
        } else if (strcmp(item->string, "faster_harmony") == 0) {
            flag = &continuation->faster_harmony;
        }
        snprintf(path, sizeof(path), "development/continuation/%s", item->string);
        if (flag == NULL) {
            return preferences_refuse(refusal, path, "unknown member");
        }
        if (!cJSON_IsBool(item)) {
            return preferences_refuse(refusal, path, "must be true or false");
        }
        *flag = cJSON_IsTrue(item);
    }
    return true;
}

/* The distance weights, similar and distant, each 0 to 20; a name left out keeps its value, all 0 reads as similar 1. */
static bool read_contrast_distance(const cJSON *distance, GrowContrast *contrast, const Refusal *refusal) {
    static const char *const NAMES[GROW_CONTRAST_DISTANCE_COUNT] = {"similar", "distant"};
    const cJSON *item;
    if (!cJSON_IsObject(distance)) {
        return preferences_refuse_in(refusal, "development", "contrast/distance", "must be an object with similar and distant, whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(item, distance) {
        bool known = false;
        for (size_t i = 0; i < GROW_CONTRAST_DISTANCE_COUNT && !known; i++) {
            if (strcmp(item->string, NAMES[i]) == 0) {
                char key[COMPOSER_PREFERENCES_PATH_MAX];
                snprintf(key, sizeof(key), "contrast/distance/%s", NAMES[i]);
                if (!preferences_read_unsigned_in(item, "development", key, 0, WEIGHT_MAX, &contrast->distance[i], refusal)) {
                    return false;
                }
                known = true;
            }
        }
        if (!known) {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "contrast/distance/%s", item->string);
            return preferences_refuse_in(refusal, "development", key, "unknown member");
        }
    }
    if (contrast->distance[GROW_CONTRAST_SIMILAR] == 0 && contrast->distance[GROW_CONTRAST_DISTANT] == 0) {
        contrast->distance[GROW_CONTRAST_SIMILAR] = 1;
    }
    return true;
}

/* The contrast settings (phase L4): distance {similar, distant} and new_material. */
static bool read_contrast(const cJSON *group, GrowContrast *contrast, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse_in(refusal, "development", "contrast", "must be an object with distance and new_material");
    }
    cJSON_ArrayForEach(item, group) {
        if (strcmp(item->string, "distance") == 0) {
            if (!read_contrast_distance(item, contrast, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "new_material") == 0) {
            if (!preferences_read_unsigned_in(item, "development", "contrast/new_material", 0, WEIGHT_MAX, &contrast->new_material,
                                  refusal)) {
                return false;
            }
        } else {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "contrast/%s", item->string);
            return preferences_refuse_in(refusal, "development", key, "unknown member");
        }
    }
    return true;
}

bool preferences_read_development(const cJSON *group, ComposerPreferences *preferences, const Refusal *refusal) {
    GrowFidelityTable *table = &preferences->development;
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "development", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        const cJSON *setting;
        if (strcmp(item->string, "continuation") == 0) {
            if (!read_continuation(item, &preferences->continuation, refusal)) {
                return false;
            }
            continue;
        }
        if (strcmp(item->string, "basic_as_written") == 0) {
            if (!cJSON_IsBool(item)) {
                return preferences_refuse_in(refusal, "development", "basic_as_written", "must be true or false");
            }
            preferences->basic_as_written = cJSON_IsTrue(item);
            continue;
        }
        if (strcmp(item->string, "contrast") == 0) {
            if (!read_contrast(item, &preferences->contrast, refusal)) {
                return false;
            }
            continue;
        }
        if (strcmp(item->string, "fidelity_minimum") != 0) {
            return preferences_refuse_in(refusal, "development", item->string, "unknown member");
        }
        if (!cJSON_IsObject(item)) {
            return preferences_refuse_in(refusal, "development", "fidelity_minimum", "must be an object of melody, balanced and theme");
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
                return preferences_refuse_in(refusal, "development", key, "unknown member");
            }
        }
    }
    return true;
}


static bool write_continuation(cJSON *group, const GrowContinuation *continuation) {
    cJSON *object = cJSON_AddObjectToObject(group, "continuation");
    cJSON *lengths = object ? cJSON_CreateObject() : NULL;
    bool ok = lengths != NULL && cJSON_AddBoolToObject(object, "every_segment", continuation->every_segment) != NULL;
    if (ok) {
        ok = cJSON_AddItemToObject(object, "fragment_lengths", lengths);
    } else {
        cJSON_Delete(lengths);
    }
    for (size_t i = 0; ok && i < GROW_FRAGMENT_LENGTH_COUNT; i++) {
        ok = preferences_add_number(lengths, FRAGMENT_LENGTH_NAMES[i], continuation->fragment_lengths[i]);
    }
    return ok && cJSON_AddBoolToObject(object, "diminish", continuation->diminish) != NULL &&
           cJSON_AddBoolToObject(object, "liquidate", continuation->liquidate) != NULL &&
           cJSON_AddBoolToObject(object, "faster_harmony", continuation->faster_harmony) != NULL;
}

cJSON *preferences_write_development(const ComposerPreferences *preferences) {
    const GrowFidelityTable *table = &preferences->development;
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
    ok = ok && write_continuation(group, &preferences->continuation) &&
         cJSON_AddBoolToObject(group, "basic_as_written", preferences->basic_as_written) != NULL;
    cJSON *contrast = ok ? cJSON_AddObjectToObject(group, "contrast") : NULL;
    cJSON *distance = contrast ? cJSON_AddObjectToObject(contrast, "distance") : NULL;
    ok = ok && distance && preferences_add_number(distance, "similar", preferences->contrast.distance[GROW_CONTRAST_SIMILAR]) &&
         preferences_add_number(distance, "distant", preferences->contrast.distance[GROW_CONTRAST_DISTANT]) &&
         preferences_add_number(contrast, "new_material", preferences->contrast.new_material);
    if (!ok) {
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}
