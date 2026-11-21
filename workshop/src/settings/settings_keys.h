#ifndef WORKSHOP_SETTINGS_KEYS_H
#define WORKSHOP_SETTINGS_KEYS_H

/*
 * The setting keys and the override rules the workshop knows (R-671, R-672, R-676): the Composer's tables, read from its vocabulary
 * (preference_keys and rules) when the workshop opens, and the workshop's own two keys, explore.share and explore.distance. A key is a
 * dotted path into the preferences document ("melody.weights.melodic"); its type says how its value reads. The tables are installed once
 * per process and read by every thread afterwards.
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

typedef struct {
    char key[96];
    char type[12]; /* integer, boolean, ratio, words or plan */
    int min;       /* an integer key's least value */
    int max;       /* an integer key's greatest value; a ratio key's denominator limit */
    bool hard;     /* a hard setting changes what a request may ask for or what a candidate may be (R-671) */
    cJSON *words;  /* the words a words key may name (an array of strings), else NULL */
    cJSON *dflt;   /* the default value, NULL for motif.plan */
} WorkshopSettingKey;

typedef struct {
    char name[48];
    bool hard;
    char key[96]; /* the setting that holds the rule: a weight for a soft group, melody.waive for a hard rule */
} WorkshopSettingRule;

/*
 * Installs the tables from the vocabulary lists (the arrays of objects the Composer's vocabulary gives as preference_keys and rules) and
 * the workshop's keys; the tables of an earlier call are replaced. False when a list is not what the Composer gives or memory runs out.
 */
bool workshop_settings_keys_install(const cJSON *preference_keys, const cJSON *rules);

/* Frees the tables (the workshop does it when it closes). */
void workshop_settings_keys_clear(void);

size_t workshop_settings_key_count(void);
const WorkshopSettingKey *workshop_settings_key_at(size_t index);
const WorkshopSettingKey *workshop_settings_key_find(const char *key);

size_t workshop_settings_rule_count(void);
const WorkshopSettingRule *workshop_settings_rule_at(size_t index);
const WorkshopSettingRule *workshop_settings_rule_find(const char *name);

/* The tables as {keys: [{key, type, min, max, words, class, default}], rules: [{name, class, key}]}; NULL on allocation failure. */
cJSON *workshop_settings_keys_json(void);

#endif
