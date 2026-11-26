#include "preferences/preferences_internal.h"

#include <stdio.h>
#include <string.h>

/*
 * The blueprint group of the preferences document (P69c, R-750): the weights of the sounds, of the rhythm jobs per formal function and
 * of the climax's bar, the melody's range below the climax and the number of candidates. Every object is closed.
 */

static const char *const SOUND_NAMES[BLUEPRINT_SOUND_COUNT] = {"ninth_downbeat", "sus4_dominant", "appoggiatura", "major_seventh", "leading_tone", "long_weak"};
static const char *const FUNCTION_NAMES[BLUEPRINT_FUNCTION_COUNT] = {"basic", "fragment", "contrasting", "cadential"};
static const char *const JOB_NAMES[BLUEPRINT_JOB_COUNT] = {"running", "long", "syncopated", "variant", "contrast"};
static const char *const CLIMAX_NAMES[2] = {"m6", "m7"};

void blueprint_preferences_default(BlueprintPreferences *out) {
    static const unsigned SOUNDS[BLUEPRINT_SOUND_COUNT] = {3, 2, 3, 1, 2, 2};
    static const unsigned JOBS[BLUEPRINT_FUNCTION_COUNT][BLUEPRINT_JOB_COUNT] = {
        {2, 3, 1, 2, 1}, {3, 0, 2, 1, 1}, {1, 1, 1, 0, 3}, {2, 2, 1, 1, 0}};
    memset(out, 0, sizeof(*out));
    memcpy(out->sounds, SOUNDS, sizeof(SOUNDS));
    memcpy(out->jobs, JOBS, sizeof(JOBS));
    out->climax[0] = 1;
    out->climax[1] = 2;
    out->range = 16;
    out->candidates = 3;
}

/* A closed object of weights 0 to 20 whose members are `names`, into `values`; `prefix` is the object's path under the group ("jobs/basic"). */
static bool read_weights(const cJSON *object, const char *prefix, const char *const *names, size_t count, unsigned *values, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(object)) {
        return preferences_refuse_in(refusal, "blueprint", prefix, "must be an object");
    }
    cJSON_ArrayForEach(item, object) {
        char key[COMPOSER_PREFERENCES_PATH_MAX];
        snprintf(key, sizeof(key), "%s/%s", prefix, item->string);
        size_t at = 0;
        while (at < count && strcmp(item->string, names[at]) != 0) {
            at++;
        }
        if (at == count) {
            return preferences_refuse_in(refusal, "blueprint", key, "unknown member");
        }
        if (!preferences_read_unsigned_in(item, "blueprint", key, 0, WEIGHT_MAX, &values[at], refusal)) {
            return false;
        }
    }
    return true;
}

static bool read_jobs(const cJSON *object, BlueprintPreferences *blueprint, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(object)) {
        return preferences_refuse_in(refusal, "blueprint", "jobs", "must be an object");
    }
    cJSON_ArrayForEach(item, object) {
        char key[COMPOSER_PREFERENCES_PATH_MAX];
        snprintf(key, sizeof(key), "jobs/%s", item->string);
        size_t at = 0;
        while (at < BLUEPRINT_FUNCTION_COUNT && strcmp(item->string, FUNCTION_NAMES[at]) != 0) {
            at++;
        }
        if (at == BLUEPRINT_FUNCTION_COUNT) {
            return preferences_refuse_in(refusal, "blueprint", key, "unknown member");
        }
        if (!read_weights(item, key, JOB_NAMES, BLUEPRINT_JOB_COUNT, blueprint->jobs[at], refusal)) {
            return false;
        }
    }
    return true;
}

bool preferences_read_blueprint(const cJSON *group, ComposerPreferences *preferences, const Refusal *refusal) {
    BlueprintPreferences *blueprint = &preferences->blueprint;
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "blueprint", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        if (strcmp(item->string, "sounds") == 0) {
            if (!read_weights(item, "sounds", SOUND_NAMES, BLUEPRINT_SOUND_COUNT, blueprint->sounds, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "jobs") == 0) {
            if (!read_jobs(item, blueprint, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "climax") == 0) {
            if (!read_weights(item, "climax", CLIMAX_NAMES, 2, blueprint->climax, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "range") == 0) {
            if (!preferences_read_unsigned_in(item, "blueprint", "range", COMPOSER_PREFERENCES_BLUEPRINT_RANGE_MIN,
                                              COMPOSER_PREFERENCES_BLUEPRINT_RANGE_MAX, &blueprint->range, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "candidates") == 0) {
            if (!preferences_read_unsigned_in(item, "blueprint", "candidates", 1, COMPOSER_PREFERENCES_BLUEPRINT_CANDIDATES_MAX,
                                              &blueprint->candidates, refusal)) {
                return false;
            }
        } else {
            return preferences_refuse_in(refusal, "blueprint", item->string, "unknown member");
        }
    }
    return true;
}

static cJSON *write_weights(cJSON *parent, const char *name, const char *const *names, size_t count, const unsigned *values) {
    cJSON *object = cJSON_AddObjectToObject(parent, name);
    for (size_t i = 0; object != NULL && i < count; i++) {
        if (!preferences_add_number(object, names[i], values[i])) {
            return NULL;
        }
    }
    return object;
}

cJSON *preferences_write_blueprint(const ComposerPreferences *preferences) {
    const BlueprintPreferences *blueprint = &preferences->blueprint;
    cJSON *group = cJSON_CreateObject();
    bool ok = group != NULL && write_weights(group, "sounds", SOUND_NAMES, BLUEPRINT_SOUND_COUNT, blueprint->sounds) != NULL;
    cJSON *jobs = ok ? cJSON_AddObjectToObject(group, "jobs") : NULL;
    ok = ok && jobs != NULL;
    for (size_t f = 0; ok && f < BLUEPRINT_FUNCTION_COUNT; f++) {
        ok = write_weights(jobs, FUNCTION_NAMES[f], JOB_NAMES, BLUEPRINT_JOB_COUNT, blueprint->jobs[f]) != NULL;
    }
    ok = ok && write_weights(group, "climax", CLIMAX_NAMES, 2, blueprint->climax) != NULL &&
         preferences_add_number(group, "range", blueprint->range) && preferences_add_number(group, "candidates", blueprint->candidates);
    if (!ok) {
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}
