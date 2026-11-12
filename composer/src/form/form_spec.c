#include <ursatz_composer/form_spec.h>

#include <cJSON.h>
#include <standard-harmony/harmony_cadence_kinds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"

/* ------------------------------------------------------------- the vocabularies the document names */

typedef struct {
    const char *name;
    const char *label;
    bool theme;
} RoleRow;

/* The role table: P46 to P53 add rows. */
static const RoleRow ROLES[FORM_ROLE_COUNT] = {
    {"theme", "Theme", true},        {"contrast", "Contrast", true}, {"return", "Return", true},
    {"variation", "Variation", true}, {"layer", "Layer", true},       {"intro", "Intro", false},
    {"body", "Loop body", false},    {"turnaround", "Turnaround", false}, {"ending", "Ending", false}};

const char *form_role_name(FormRole role) {
    return role >= 0 && role < FORM_ROLE_COUNT ? ROLES[role].name : NULL;
}

const char *form_role_label(FormRole role) {
    return role >= 0 && role < FORM_ROLE_COUNT ? ROLES[role].label : NULL;
}

bool form_role_find(const char *name, FormRole *out) {
    for (int role = 0; name && role < FORM_ROLE_COUNT; role++) {
        if (strcmp(name, ROLES[role].name) == 0) {
            *out = (FormRole)role;
            return true;
        }
    }
    return false;
}

bool form_role_is_theme(FormRole role) {
    return role >= 0 && role < FORM_ROLE_COUNT && ROLES[role].theme;
}

static const struct {
    const char *name;
    FormCheck check;
} CHECKS[] = {{"sections", FORM_CHECK_SECTIONS},
              {"returns", FORM_CHECK_RETURNS},
              {"keys", FORM_CHECK_KEYS},
              {"cadences", FORM_CHECK_CADENCES},
              {"seam", FORM_CHECK_SEAM}};
#define CHECK_COUNT (sizeof(CHECKS) / sizeof(CHECKS[0]))

const char *form_check_name(FormCheck check) {
    for (size_t i = 0; i < CHECK_COUNT; i++) {
        if (CHECKS[i].check == check) {
            return CHECKS[i].name;
        }
    }
    return NULL;
}


static const struct {
    const char *name;
    PlanLaneKind lane;
} LANES[] = {{"bass", PLAN_LANE_BASS}, {"inner_voice", PLAN_LANE_INNER_VOICE}, {"melody", PLAN_LANE_MELODY}};
#define LANE_COUNT (sizeof(LANES) / sizeof(LANES[0]))

/* ------------------------------------------------------------- reading */

typedef struct {
    FormSpecError *error;
    bool failed;
} Reader;

static bool fail(Reader *reader, const char *path, const char *format, ...) {
    if (!reader->failed && reader->error) {
        va_list args;
        snprintf(reader->error->path, sizeof(reader->error->path), "%s", path);
        va_start(args, format);
        vsnprintf(reader->error->message, sizeof(reader->error->message), format, args);
        va_end(args);
    }
    reader->failed = true;
    return false;
}

/* path + "/" + the member name, with "~" and "/" escaped as a JSON pointer does. */
static void join(char *out, size_t cap, const char *base, const char *member) {
    size_t n = (size_t)snprintf(out, cap, "%s/", base);
    for (; *member && n + 2 < cap; member++) {
        if (*member == '~') {
            out[n++] = '~';
            out[n++] = '0';
        } else if (*member == '/') {
            out[n++] = '~';
            out[n++] = '1';
        } else {
            out[n++] = *member;
        }
    }
    out[n < cap ? n : cap - 1] = '\0';
}

static void join_index(char *out, size_t cap, const char *base, size_t index) {
    snprintf(out, cap, "%s/%zu", base, index);
}

/* Refuses a member the object is not allowed to have, and one it has twice. */
static bool members(Reader *reader, const cJSON *object, const char *path, const char *const *allowed, size_t count) {
    for (const cJSON *child = object->child; child; child = child->next) {
        char member[64];
        bool known = false;
        for (size_t i = 0; i < count; i++) {
            if (child->string && strcmp(child->string, allowed[i]) == 0) {
                known = true;
            }
        }
        join(member, sizeof(member), path, child->string ? child->string : "");
        if (!known) {
            return fail(reader, member, "unknown member");
        }
        for (const cJSON *other = child->next; other; other = other->next) {
            if (other->string && strcmp(other->string, child->string) == 0) {
                return fail(reader, member, "member given twice");
            }
        }
    }
    return true;
}

/* The member, or NULL (with a refusal when it is required). */
static const cJSON *member_of(Reader *reader, const cJSON *object, const char *path, const char *key, bool required) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item && required) {
        char at[64];
        join(at, sizeof(at), path, key);
        fail(reader, at, "required member is missing");
    }
    return item;
}

static bool valid_name(const char *text, size_t max) {
    size_t length = strlen(text);
    if (length < 1 || length >= max) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

/* A string member copied into out; false (refused) when it is the wrong type or does not fit. */
static bool read_text(Reader *reader, const cJSON *object, const char *path, const char *key, bool required, char *out,
                      size_t cap, bool *present) {
    char at[64];
    const cJSON *item = member_of(reader, object, path, key, required);
    if (present) {
        *present = item != NULL;
    }
    if (!item) {
        return !required;
    }
    join(at, sizeof(at), path, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fail(reader, at, "must be a string");
    }
    if (item->valuestring[0] == '\0' || strlen(item->valuestring) >= cap) {
        return fail(reader, at, "must be 1 to %zu characters", cap - 1);
    }
    snprintf(out, cap, "%s", item->valuestring);
    return true;
}

static bool read_name(Reader *reader, const cJSON *object, const char *path, const char *key, bool required, char *out,
                      size_t cap, bool *present) {
    bool here = false;
    if (!read_text(reader, object, path, key, required, out, cap, &here)) {
        return false;
    }
    if (present) {
        *present = here;
    }
    if (here && !valid_name(out, cap)) {
        char at[64];
        join(at, sizeof(at), path, key);
        return fail(reader, at, "a name is 1 to %zu characters of a-z, 0-9 and -", cap - 1);
    }
    return true;
}

static bool read_bool(Reader *reader, const cJSON *object, const char *path, const char *key, bool *out) {
    const cJSON *item = member_of(reader, object, path, key, false);
    char at[64];
    if (!item) {
        return true;
    }
    join(at, sizeof(at), path, key);
    if (!cJSON_IsBool(item)) {
        return fail(reader, at, "must be true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

/* A whole number in [min, max]. */
static bool whole(Reader *reader, const cJSON *item, const char *at, long min, long max, long *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble) {
        return fail(reader, at, "must be a whole number");
    }
    if (item->valuedouble < (double)min || item->valuedouble > (double)max) {
        return fail(reader, at, "must be from %ld to %ld", min, max);
    }
    *out = (long)item->valuedouble;
    return true;
}

static const cJSON *array_member(Reader *reader, const cJSON *object, const char *path, const char *key, bool required,
                                 char *at, size_t at_cap) {
    const cJSON *item = member_of(reader, object, path, key, required);
    if (!item) {
        return NULL;
    }
    join(at, at_cap, path, key);
    if (!cJSON_IsArray(item)) {
        fail(reader, at, "must be an array");
        return NULL;
    }
    return item;
}

/* ------------------------------------------------------------- regions */

static bool read_degrees(Reader *reader, const cJSON *region, const char *path, const char *key, int *out, size_t *count) {
    char at[64];
    const cJSON *list = array_member(reader, region, path, key, true, at, sizeof(at));
    if (!list) {
        return false;
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n < 1 || n > FORM_SPEC_MAX_DEGREES) {
        return fail(reader, at, "must hold 1 to %d degrees", FORM_SPEC_MAX_DEGREES);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        long degree;
        join_index(item, sizeof(item), at, i);
        if (!whole(reader, cJSON_GetArrayItem(list, (int)i), item, 1, 7, &degree)) {
            return false;
        }
        out[i] = (int)degree;
    }
    *count = n;
    return true;
}

static bool read_regions(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const ALLOWED[] = {"name", "major", "minor"};
    char at[64];
    const cJSON *list = array_member(reader, root, "", "regions", false, at, sizeof(at));
    if (reader->failed || !list) {
        return !reader->failed;
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n > FORM_SPEC_MAX_REGIONS) {
        return fail(reader, at, "at most %d regions", FORM_SPEC_MAX_REGIONS);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        const cJSON *region = cJSON_GetArrayItem(list, (int)i);
        FormRegion *out = &form->regions[i];
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsObject(region)) {
            return fail(reader, item, "must be an object");
        }
        if (!members(reader, region, item, ALLOWED, 3) ||
            !read_name(reader, region, item, "name", true, out->name, sizeof(out->name), NULL) ||
            !read_degrees(reader, region, item, "major", out->major, &out->major_count) ||
            !read_degrees(reader, region, item, "minor", out->minor, &out->minor_count)) {
            return false;
        }
        char name_at[64];
        join(name_at, sizeof(name_at), item, "name");
        if (strcmp(out->name, "home") == 0) {
            return fail(reader, name_at, "\"home\" names the home key");
        }
        for (size_t other = 0; other < i; other++) {
            if (strcmp(form->regions[other].name, out->name) == 0) {
                return fail(reader, name_at, "region name repeated");
            }
        }
        form->region_count = i + 1;
    }
    return true;
}

int form_spec_region_find(const FormSpec *form, const char *name) {
    for (size_t i = 0; form && name && i < form->region_count; i++) {
        if (strcmp(form->regions[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------- sections */

/* "home" or the name of a region. */
static bool read_key(Reader *reader, const FormSpec *form, const cJSON *keys, const char *path, const char *key, int *out) {
    char at[64];
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(keys, key);
    if (!item) {
        return true;
    }
    join(at, sizeof(at), path, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fail(reader, at, "must be \"home\" or a region name");
    }
    if (strcmp(item->valuestring, "home") == 0) {
        *out = FORM_SPEC_HOME;
        return true;
    }
    int region = form_spec_region_find(form, item->valuestring);
    if (region < 0) {
        return fail(reader, at, "unknown region \"%s\"", item->valuestring);
    }
    *out = region;
    return true;
}

static bool read_layers(Reader *reader, const cJSON *section, const char *path, unsigned *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(section, "layers");
    char at[64];
    if (!item) {
        return true;
    }
    join(at, sizeof(at), path, "layers");
    if (cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, "all") == 0) {
        *out = PLAN_LAYERS_ALL;
        return true;
    }
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) < 1) {
        return fail(reader, at, "must be \"all\" or a list of lanes");
    }
    unsigned mask = 0;
    for (int i = 0; i < cJSON_GetArraySize(item); i++) {
        const cJSON *lane = cJSON_GetArrayItem(item, i);
        char lane_at[64];
        bool known = false;
        join_index(lane_at, sizeof(lane_at), at, (size_t)i);
        if (!cJSON_IsString(lane) || !lane->valuestring) {
            return fail(reader, lane_at, "must be a lane name");
        }
        for (size_t l = 0; l < LANE_COUNT; l++) {
            if (strcmp(lane->valuestring, LANES[l].name) == 0) {
                mask |= PLAN_LAYER_BIT(LANES[l].lane);
                known = true;
            }
        }
        if (!known) {
            return fail(reader, lane_at, "unknown lane \"%s\" (bass, inner_voice, melody)", lane->valuestring);
        }
    }
    *out = mask;
    return true;
}

static bool read_choice(Reader *reader, const cJSON *section, const char *path, const char *key, const char *const *names,
                        size_t count, int *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(section, key);
    char at[64];
    if (!item) {
        return true;
    }
    join(at, sizeof(at), path, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fail(reader, at, "must be a string");
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(item->valuestring, names[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    return fail(reader, at, "unknown %s \"%s\"", key, item->valuestring);
}

/* A cadence member: a name of the standard-harmony vocabulary whose core is PAC, IAC or HC. */
static bool read_cadence(Reader *reader, const cJSON *object, const char *path, const char *key, bool required,
                         CadenceType *core, char *name, size_t cap) {
    char at[64];
    bool present = false;
    if (!read_text(reader, object, path, key, required, name, cap, &present)) {
        return false;
    }
    if (!present) {
        return true;
    }
    join(at, sizeof(at), path, key);
    const HarmonyCadenceKind *kind = harmony_cadence_kind_find(name);
    if (!kind) {
        return fail(reader, at, "unknown cadence \"%s\"", name);
    }
    if (kind->core != CADENCE_TYPE_PERFECT_AUTHENTIC && kind->core != CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
        kind->core != CADENCE_TYPE_HALF) {
        return fail(reader, at, "a section ends in a perfect authentic, imperfect authentic or half cadence");
    }
    *core = kind->core;
    return true;
}

/* ------------------------------------------------------------- a section's construction */

static const char *const EXTENSION_KINDS[] = {"repeat", "stretch", "addition", "evaded"};

/* One whole number member of an object, 0 to max, into *out; the default when the member is absent. */
static bool read_small(Reader *reader, const cJSON *object, const char *path, const char *key, long max, long *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    char at[96];
    if (!item) {
        return true;
    }
    join(at, sizeof(at), path, key);
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble || item->valuedouble < 0 ||
        item->valuedouble > (double)max) {
        return fail(reader, at, "must be a whole number from 0 to %ld", max);
    }
    *out = (long)item->valuedouble;
    return true;
}

static bool read_extension(Reader *reader, const cJSON *item, const char *path, PlanExtension *out) {
    static const char *const ALLOWED[] = {"kind", "idea", "bars", "varied"};
    char at[96];
    long bars = 0;
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsObject(item)) {
        return fail(reader, path, "must be {\"kind\", \"idea\", \"bars\", \"varied\"}");
    }
    if (!members(reader, item, path, ALLOWED, 4)) {
        return false;
    }
    const cJSON *kind = member_of(reader, item, path, "kind", true);
    if (!kind) {
        return false;
    }
    join(at, sizeof(at), path, "kind");
    int found = -1;
    for (int k = 0; cJSON_IsString(kind) && kind->valuestring && k < 4; k++) {
        found = strcmp(kind->valuestring, EXTENSION_KINDS[k]) == 0 ? k : found;
    }
    if (found < 0) {
        return fail(reader, at, "must be repeat, stretch, addition or evaded");
    }
    out->kind = (PlanExtensionKind)found;
    const cJSON *idea = cJSON_GetObjectItemCaseSensitive(item, "idea");
    join(at, sizeof(at), path, "idea");
    if (idea && !cJSON_IsNull(idea)) {
        const ThemeShapeIdea *row = cJSON_IsString(idea) && idea->valuestring ? theme_shape_idea_find(idea->valuestring) : NULL;
        if (!row) {
            return fail(reader, at, "must be the name of an idea");
        }
        out->idea = (unsigned char)row->idea;
    } else if (out->kind == PLAN_EXTENSION_REPEAT || out->kind == PLAN_EXTENSION_STRETCH) {
        return fail(reader, at, "a %s needs the idea it follows", EXTENSION_KINDS[found]);
    }
    if (!read_small(reader, item, path, "bars", 4, &bars)) {
        return false;
    }
    out->bars = (unsigned char)bars;
    const cJSON *varied = cJSON_GetObjectItemCaseSensitive(item, "varied");
    if (varied) {
        join(at, sizeof(at), path, "varied");
        if (!cJSON_IsBool(varied)) {
            return fail(reader, at, "must be true or false");
        }
        out->varied = cJSON_IsTrue(varied);
    }
    return true;
}

/*
 * The construction member (R-468, R-469): "auto", or {"ideas": {"<idea name>": bars}, "extensions": [{"kind", "idea",
 * "bars", "varied"}]}. Whether the ideas fit the pairing is checked when the form is built (it needs the meter); the
 * bars they add up to are checked here against the section's.
 */
static bool read_construction(Reader *reader, const cJSON *item, const char *path, FormSectionSpec *out) {
    static const char *const ALLOWED[] = {"ideas", "extensions"};
    char at[96];
    out->has_construction = true;
    if (cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, "auto") == 0) {
        out->construction_auto = true;
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return fail(reader, path, "must be \"auto\" or {\"ideas\": {...}, \"extensions\": [...]}");
    }
    if (!members(reader, item, path, ALLOWED, 2)) {
        return false;
    }
    PlanConstruction *construction = &out->construction;
    memset(construction, 0, sizeof(*construction));
    construction->reason = PLAN_CONSTRUCTION_GIVEN;
    const cJSON *ideas = member_of(reader, item, path, "ideas", true);
    if (!ideas) {
        return false;
    }
    join(at, sizeof(at), path, "ideas");
    if (!cJSON_IsObject(ideas)) {
        return fail(reader, at, "must be an object of idea names and bars, for example {\"basic idea\": 2}");
    }
    for (const cJSON *member = ideas->child; member; member = member->next) {
        char idea_at[128];
        join(idea_at, sizeof(idea_at), at, member->string ? member->string : "");
        const ThemeShapeIdea *row = member->string ? theme_shape_idea_find(member->string) : NULL;
        if (!row) {
            return fail(reader, idea_at, "unknown idea");
        }
        if (!cJSON_IsNumber(member) || member->valuedouble != (double)(int)member->valuedouble || member->valuedouble < 0 ||
            member->valuedouble > 16) {
            return fail(reader, idea_at, "must be a whole number of bars from 0 to 16");
        }
        construction->idea_bars[row->idea] = (unsigned char)member->valuedouble;
    }
    const cJSON *extensions = cJSON_GetObjectItemCaseSensitive(item, "extensions");
    if (extensions) {
        join(at, sizeof(at), path, "extensions");
        if (!cJSON_IsArray(extensions)) {
            return fail(reader, at, "must be a list");
        }
        if (cJSON_GetArraySize(extensions) > PLAN_CONSTRUCTION_EXTENSIONS) {
            return fail(reader, at, "at most %d extensions", PLAN_CONSTRUCTION_EXTENSIONS);
        }
        for (int k = 0; k < cJSON_GetArraySize(extensions); k++) {
            char extension_at[128];
            join_index(extension_at, sizeof(extension_at), at, (size_t)k);
            if (!read_extension(reader, cJSON_GetArrayItem(extensions, k), extension_at, &construction->extensions[k])) {
                return false;
            }
            construction->extension_count = (size_t)k + 1;
        }
    }
    return true;
}

/* Whether two sections' constructions are the same: both none, both "auto", or both the same ideas and extensions. */
static bool same_construction(const FormSectionSpec *a, const FormSectionSpec *b) {
    if (a->has_construction != b->has_construction || a->construction_auto != b->construction_auto) {
        return false;
    }
    if (!a->has_construction || a->construction_auto) {
        return true;
    }
    if (memcmp(a->construction.idea_bars, b->construction.idea_bars, sizeof(a->construction.idea_bars)) != 0 ||
        a->construction.extension_count != b->construction.extension_count) {
        return false;
    }
    for (size_t k = 0; k < a->construction.extension_count; k++) {
        const PlanExtension *x = &a->construction.extensions[k];
        const PlanExtension *y = &b->construction.extensions[k];
        if (x->kind != y->kind || x->idea != y->idea || x->bars != y->bars || x->varied != y->varied) {
            return false;
        }
    }
    return true;
}

static bool read_section(Reader *reader, const FormSpec *form, const FormSpecSize *size, const cJSON *json,
                         const char *path, FormSectionSpec *out) {
    static const char *const ALLOWED[] = {"name",    "letter",  "role",   "bars",   "beats",   "beginning", "ending",
                                          "cadence", "closed",  "keys",   "motive", "returns", "variation", "layers",
                                          "pedal",   "construction"};
    char text[FORM_SPEC_CADENCE_NAME_MAX];
    char at[64];
    if (!cJSON_IsObject(json)) {
        return fail(reader, path, "must be an object");
    }
    memset(out, 0, sizeof(*out));
    out->returns = FORM_SPEC_NO_RETURN;
    out->variation = PLAN_RETURN_EXACT;
    out->key_start = FORM_SPEC_HOME;
    out->key_end = FORM_SPEC_HOME;
    if (!members(reader, json, path, ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0])) ||
        !read_text(reader, json, path, "name", true, out->name, sizeof(out->name), NULL) ||
        !read_text(reader, json, path, "letter", true, out->letter, sizeof(out->letter), NULL)) {
        return false;
    }
    /* A name is unique in its size. */
    for (size_t i = 0; i < size->section_count; i++) {
        if (strcmp(size->sections[i].name, out->name) == 0) {
            join(at, sizeof(at), path, "name");
            return fail(reader, at, "section name \"%s\" repeated in the size", out->name);
        }
    }
    /* role */
    if (!read_text(reader, json, path, "role", true, text, sizeof(text), NULL)) {
        return false;
    }
    if (!form_role_find(text, &out->role)) {
        join(at, sizeof(at), path, "role");
        return fail(reader, at, "unknown role \"%s\"", text);
    }
    /* bars or beats, exactly one */
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(json, "bars");
    const cJSON *beats = cJSON_GetObjectItemCaseSensitive(json, "beats");
    if ((bars != NULL) == (beats != NULL)) {
        return fail(reader, path, bars ? "give bars or beats, not both" : "give bars or beats");
    }
    long length;
    join(at, sizeof(at), path, bars ? "bars" : "beats");
    if (!whole(reader, bars ? bars : beats, at, 1, 4096, &length)) {
        return false;
    }
    if (bars) {
        out->bars = (size_t)length;
    } else {
        out->beats = (size_t)length;
    }
    /* beginning and ending: the theme shape's plan names */
    const ThemeShapeHalf *beginning;
    const ThemeShapeHalf *ending;
    if (!read_text(reader, json, path, "beginning", true, text, sizeof(text), NULL)) {
        return false;
    }
    beginning = theme_shape_half_find(true, text);
    if (!beginning) {
        join(at, sizeof(at), path, "beginning");
        return fail(reader, at, "unknown beginning \"%s\"", text);
    }
    if (!read_text(reader, json, path, "ending", true, text, sizeof(text), NULL)) {
        return false;
    }
    ending = theme_shape_half_find(false, text);
    if (!ending) {
        join(at, sizeof(at), path, "ending");
        return fail(reader, at, "unknown ending \"%s\"", text);
    }
    out->pairing.beginning = (PlanThemeBeginning)beginning->value;
    out->pairing.ending = (PlanThemeEnding)ending->value;
    /* construction: a theme of any whole bars from 3 to 16 (R-468); a theme-like section of other bars than 4 or 8 needs it */
    const cJSON *construction = member_of(reader, json, path, "construction", false);
    if (construction) {
        join(at, sizeof(at), path, "construction");
        if (!form_role_is_theme(out->role)) {
            return fail(reader, at, "only a theme-like section has a construction");
        }
        if (!bars) {
            return fail(reader, at, "a construction needs bars, not beats");
        }
        if (!read_construction(reader, construction, at, out)) {
            return false;
        }
        if (out->bars < 3 || out->bars > 16) {
            join(at, sizeof(at), path, "bars");
            return fail(reader, at, "a section with a construction is 3 to 16 bars");
        }
        if (!out->construction_auto) {
            size_t total = grow_construction_extra_bars(&out->pairing, &out->construction);
            for (int idea = 0; idea < PLAN_CONSTRUCTION_IDEAS; idea++) {
                total += out->construction.idea_bars[idea];
            }
            if (total != out->bars) {
                join(at, sizeof(at), path, "construction");
                return fail(reader, at, "the construction adds up to %zu bars but the section is %zu", total, out->bars);
            }
        }
    } else if (bars && form_role_is_theme(out->role) && out->bars != 4 && out->bars != 8) {
        join(at, sizeof(at), path, "bars");
        return fail(reader, at, "a theme-like section of %zu bars needs a construction; only 4 or 8 bars have none", out->bars);
    }
    /* cadence: a name of the standard-harmony vocabulary whose core is PAC, IAC or HC */
    if (!read_cadence(reader, json, path, "cadence", true, &out->cadence, out->cadence_name, sizeof(out->cadence_name))) {
        return false;
    }
    if (!read_bool(reader, json, path, "closed", &out->closed)) {
        return false;
    }
    /* keys */
    const cJSON *keys = member_of(reader, json, path, "keys", false);
    if (keys) {
        static const char *const KEY_MEMBERS[] = {"start", "end"};
        char keys_at[64];
        join(keys_at, sizeof(keys_at), path, "keys");
        if (!cJSON_IsObject(keys)) {
            return fail(reader, keys_at, "must be an object");
        }
        if (!members(reader, keys, keys_at, KEY_MEMBERS, 2) ||
            !read_key(reader, form, keys, keys_at, "start", &out->key_start) ||
            !read_key(reader, form, keys, keys_at, "end", &out->key_end)) {
            return false;
        }
    }
    /* motive: as motive_relation_step_text writes it */
    const cJSON *motive = member_of(reader, json, path, "motive", false);
    if (motive) {
        join(at, sizeof(at), path, "motive");
        if (!cJSON_IsString(motive) || !motive->valuestring) {
            return fail(reader, at, "must be a motive relation such as \"invert tonal=1\"");
        }
        if (!motive_relation_step_parse(motive->valuestring, &out->motive)) {
            return fail(reader, at, "not a motive relation: \"%s\"", motive->valuestring);
        }
        out->has_motive = true;
    }
    /* returns and variation */
    const cJSON *returns = member_of(reader, json, path, "returns", false);
    bool has_variation = cJSON_GetObjectItemCaseSensitive(json, "variation") != NULL;
    if (returns) {
        join(at, sizeof(at), path, "returns");
        if (!cJSON_IsString(returns) || !returns->valuestring) {
            return fail(reader, at, "must be the name of an earlier section");
        }
        bool found = false;
        for (size_t i = 0; i < size->section_count; i++) {
            if (strcmp(size->sections[i].name, returns->valuestring) == 0) {
                out->returns = i;
                found = true;
            }
        }
        if (!found) {
            return fail(reader, at, "\"%s\" is not an earlier section of the size", returns->valuestring);
        }
        /* A return copies its source, so it keeps the source's construction. */
        if (!same_construction(out, &size->sections[out->returns])) {
            join(at, sizeof(at), path, "construction");
            return fail(reader, at, "a return keeps the construction of %s", size->sections[out->returns].name);
        }
    }
    if (has_variation) {
        const cJSON *variation = cJSON_GetObjectItemCaseSensitive(json, "variation");
        join(at, sizeof(at), path, "variation");
        if (!returns) {
            return fail(reader, at, "a variation needs returns");
        }
        if (!cJSON_IsString(variation) || !variation->valuestring) {
            return fail(reader, at, "must be a string");
        }
        if (!plan_return_variation_from_name(variation->valuestring, &out->variation)) {
            return fail(reader, at, "unknown variation \"%s\"", variation->valuestring);
        }
    }
    /* pedal: only "dominant", and only under a presentation */
    if (cJSON_GetObjectItemCaseSensitive(json, "pedal")) {
        static const char *const PEDAL_NAMES[] = {"dominant"};
        int pedal = -1;
        join(at, sizeof(at), path, "pedal");
        if (!read_choice(reader, json, path, "pedal", PEDAL_NAMES, 1, &pedal)) {
            return false;
        }
        if (out->pairing.beginning != PLAN_THEME_BEGINNING_PRESENTATION) {
            return fail(reader, at, "a pedal needs a presentation beginning");
        }
        out->pedal = 5;
    }
    return read_layers(reader, json, path, &out->layers);
}

/* ------------------------------------------------------------- a form whose sections the request gives */

bool form_spec_has_role(const FormSpec *form, FormRole role) {
    for (size_t i = 0; form && i < form->role_count; i++) {
        if (form->roles[i] == role) {
            return true;
        }
    }
    return false;
}

bool form_spec_role_in_loop(const FormSpec *form, FormRole role) {
    for (size_t i = 0; form && i < form->loop_role_count; i++) {
        if (form->loop_roles[i] == role) {
            return true;
        }
    }
    return false;
}

static bool is_part_role(FormRole role) {
    return role >= FORM_ROLE_INTRO && role <= FORM_ROLE_ENDING;
}

/*
 * A list of roles. With form NULL it is the form's own `roles`: each a part role (until P45), in the order intro, body,
 * turnaround, ending, none twice. With a form it is a list of roles the form has (the loop's, a cadence rule's).
 */
static bool read_role_list(Reader *reader, const cJSON *parent, const char *path, const char *key, const FormSpec *form,
                           FormRole *out, size_t *count) {
    char at[64];
    const cJSON *list = array_member(reader, parent, path, key, true, at, sizeof(at));
    if (!list) {
        return false;
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n < 1 || n > FORM_SPEC_MAX_PARTS) {
        return fail(reader, at, "must hold 1 to %d roles", FORM_SPEC_MAX_PARTS);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        const cJSON *entry = cJSON_GetArrayItem(list, (int)i);
        FormRole role;
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsString(entry) || !entry->valuestring) {
            return fail(reader, item, "must be a role name");
        }
        if (!form_role_find(entry->valuestring, &role)) {
            return fail(reader, item, "unknown role \"%s\"", entry->valuestring);
        }
        if (form && !form_spec_has_role(form, role)) {
            return fail(reader, item, "\"%s\" is not one of the form's roles", entry->valuestring);
        }
        if (!form && !is_part_role(role)) {
            return fail(reader, item, "a form whose sections the request gives has the roles intro, body, turnaround and ending");
        }
        if (!form && i > 0 && role <= out[i - 1]) {
            return fail(reader, item, "the roles are in the order intro, body, turnaround, ending, each once");
        }
        out[i] = role;
    }
    *count = n;
    return true;
}

static bool read_cadence_rules(Reader *reader, const cJSON *cadences, FormSpec *form) {
    static const char *const RULE_MEMBERS[] = {"roles", "cadence"};
    char at[64];
    const cJSON *list = array_member(reader, cadences, "/cadences", "last", false, at, sizeof(at));
    if (reader->failed || !list) {
        return !reader->failed;
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n > FORM_SPEC_MAX_CADENCE_RULES) {
        return fail(reader, at, "at most %d rules", FORM_SPEC_MAX_CADENCE_RULES);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        const cJSON *rule = cJSON_GetArrayItem(list, (int)i);
        FormCadenceRule *out = &form->last_cadences[i];
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsObject(rule)) {
            return fail(reader, item, "must be an object");
        }
        if (!members(reader, rule, item, RULE_MEMBERS, 2) ||
            !read_role_list(reader, rule, item, "roles", form, out->roles, &out->role_count) ||
            !read_cadence(reader, rule, item, "cadence", true, &out->cadence, out->cadence_name, sizeof(out->cadence_name))) {
            return false;
        }
        form->last_cadence_count = i + 1;
    }
    return true;
}

static bool read_default_ending(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const ALLOWED[] = {"id", "letter", "role", "bars", "cadence"};
    const cJSON *ending = member_of(reader, root, "", "default_ending", false);
    FormDefaultEnding *out = &form->default_ending;
    char text[FORM_SPEC_NAME_MAX];
    if (!ending) {
        return true;
    }
    if (!cJSON_IsObject(ending)) {
        return fail(reader, "/default_ending", "must be an object");
    }
    if (!members(reader, ending, "/default_ending", ALLOWED, 5) ||
        !read_text(reader, ending, "/default_ending", "id", true, out->id, sizeof(out->id), NULL) ||
        !read_text(reader, ending, "/default_ending", "letter", true, out->letter, sizeof(out->letter), NULL) ||
        !read_text(reader, ending, "/default_ending", "role", true, text, sizeof(text), NULL)) {
        return false;
    }
    if (!form_role_find(text, &out->role) || out->role != FORM_ROLE_ENDING || !form_spec_has_role(form, out->role)) {
        return fail(reader, "/default_ending/role", "must be the form's ending role");
    }
    const cJSON *bars = member_of(reader, ending, "/default_ending", "bars", true);
    long length;
    if (!bars || !whole(reader, bars, "/default_ending/bars", 1, 64, &length)) {
        return false;
    }
    out->bars = (size_t)length;
    if (!read_cadence(reader, ending, "/default_ending", "cadence", true, &out->cadence, out->cadence_name,
                      sizeof(out->cadence_name))) {
        return false;
    }
    form->has_default_ending = true;
    return true;
}

/* roles, loop, cadences and default_ending: the members a form whose sections the request gives needs. */
static bool read_request_members(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const LOOP_MEMBERS[] = {"roles"};
    static const char *const CADENCE_MEMBERS[] = {"default", "last"};
    if (!read_role_list(reader, root, "", "roles", NULL, form->roles, &form->role_count)) {
        return false;
    }
    if (!form_spec_has_role(form, FORM_ROLE_BODY)) {
        return fail(reader, "/roles", "a form whose sections the request gives has a body role");
    }
    const cJSON *loop = member_of(reader, root, "", "loop", true);
    if (!loop) {
        return false;
    }
    if (!cJSON_IsObject(loop)) {
        return fail(reader, "/loop", "must be an object");
    }
    if (!members(reader, loop, "/loop", LOOP_MEMBERS, 1) ||
        !read_role_list(reader, loop, "/loop", "roles", form, form->loop_roles, &form->loop_role_count)) {
        return false;
    }
    const cJSON *cadences = member_of(reader, root, "", "cadences", true);
    if (!cadences) {
        return false;
    }
    if (!cJSON_IsObject(cadences)) {
        return fail(reader, "/cadences", "must be an object");
    }
    if (!members(reader, cadences, "/cadences", CADENCE_MEMBERS, 2) ||
        !read_cadence(reader, cadences, "/cadences", "default", true, &form->default_cadence, form->default_cadence_name,
                      sizeof(form->default_cadence_name)) ||
        !read_cadence_rules(reader, cadences, form)) {
        return false;
    }
    return read_default_ending(reader, root, form);
}

/* ------------------------------------------------------------- sizes */

static bool read_sizes(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const ALLOWED[] = {"bars", "count", "default", "sections"};
    char at[64];
    const cJSON *list = array_member(reader, root, "", "sizes", false, at, sizeof(at));
    if (reader->failed) {
        return false;
    }
    if (!list) {
        if (!form->request_sections) {
            return fail(reader, "/sizes", "required member is missing");
        }
        return true;
    }
    if (form->request_sections) {
        return fail(reader, at, "a form whose sections the request gives has no sizes");
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n < 1) {
        return fail(reader, at, "must hold at least one size");
    }
    if (n > FORM_SPEC_MAX_SIZES) {
        return fail(reader, at, "at most %d sizes", FORM_SPEC_MAX_SIZES);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        char sections_at[64];
        const cJSON *json = cJSON_GetArrayItem(list, (int)i);
        FormSpecSize *size = &form->sizes[i];
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsObject(json)) {
            return fail(reader, item, "must be an object");
        }
        if (!members(reader, json, item, ALLOWED, 4)) {
            return false;
        }
        long bars;
        long count = 0;
        join(sections_at, sizeof(sections_at), item, "bars");
        const cJSON *bars_item = member_of(reader, json, item, "bars", true);
        if (!bars_item || !whole(reader, bars_item, sections_at, 1, 4096, &bars)) {
            return false;
        }
        const cJSON *count_item = member_of(reader, json, item, "count", false);
        if (count_item) {
            join(sections_at, sizeof(sections_at), item, "count");
            if (!whole(reader, count_item, sections_at, 1, 64, &count)) {
                return false;
            }
        }
        size->bars = (size_t)bars;
        size->count = (size_t)count;
        if (!read_bool(reader, json, item, "default", &size->is_default)) {
            return false;
        }
        const cJSON *sections = array_member(reader, json, item, "sections", true, sections_at, sizeof(sections_at));
        if (!sections) {
            return false;
        }
        size_t section_count = (size_t)cJSON_GetArraySize(sections);
        if (section_count < 1) {
            return fail(reader, sections_at, "must hold at least one section");
        }
        if (section_count > FORM_SPEC_MAX_SECTIONS) {
            return fail(reader, sections_at, "at most %d sections", FORM_SPEC_MAX_SECTIONS);
        }
        size_t total = 0;
        for (size_t s = 0; s < section_count; s++) {
            char section_at[64];
            join_index(section_at, sizeof(section_at), sections_at, s);
            if (!read_section(reader, form, size, cJSON_GetArrayItem(sections, (int)s), section_at, &size->sections[s])) {
                return false;
            }
            size->section_count = s + 1;
            total += size->sections[s].bars;
        }
        /* A size whose sections are given in bars adds up; a section in beats is checked against the meter at build. */
        bool all_bars = true;
        for (size_t s = 0; s < section_count; s++) {
            all_bars = all_bars && size->sections[s].bars > 0;
        }
        if (all_bars && total != size->bars) {
            join(sections_at, sizeof(sections_at), item, "bars");
            return fail(reader, sections_at, "bars is %zu but the sections add up to %zu", size->bars, total);
        }
        for (size_t other = 0; other < i; other++) {
            if (form->sizes[other].bars == size->bars && form->sizes[other].count == size->count) {
                return fail(reader, item, "a size with these bars and count is already listed");
            }
        }
        form->size_count = i + 1;
    }
    size_t defaults = 0;
    for (size_t i = 0; i < n; i++) {
        defaults += form->sizes[i].is_default ? 1 : 0;
    }
    if (defaults != 1) {
        return fail(reader, at, defaults ? "more than one size is the default" : "one size must be the default");
    }
    return true;
}

/* ------------------------------------------------------------- the document */

static bool read_checks(Reader *reader, const cJSON *root, FormSpec *form) {
    char at[64];
    const cJSON *list = array_member(reader, root, "", "checks", false, at, sizeof(at));
    if (reader->failed || !list) {
        return !reader->failed;
    }
    form->checks = 0;
    for (int i = 0; i < cJSON_GetArraySize(list); i++) {
        char item[64];
        const cJSON *entry = cJSON_GetArrayItem(list, i);
        bool known = false;
        join_index(item, sizeof(item), at, (size_t)i);
        if (!cJSON_IsString(entry) || !entry->valuestring) {
            return fail(reader, item, "must be a check name");
        }
        for (size_t c = 0; c < CHECK_COUNT; c++) {
            if (strcmp(entry->valuestring, CHECKS[c].name) == 0) {
                form->checks |= CHECKS[c].check;
                known = true;
            }
        }
        if (!known) {
            return fail(reader, item, "unknown check \"%s\"", entry->valuestring);
        }
    }
    return true;
}

static bool read_document(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const ALLOWED[] = {"schema", "version", "name",    "label",  "plan_name", "closing", "accompaniment",
                                          "regions", "checks", "sizes", "sections", "roles",   "loop",    "cadences",
                                          "default_ending"};
    static const char *const REQUEST_ONLY[] = {"roles", "loop", "cadences", "default_ending"};
    char text[FORM_SPEC_NAME_MAX];
    bool present;
    if (!cJSON_IsObject(root)) {
        return fail(reader, "", "the document must be an object");
    }
    if (!members(reader, root, "", ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0]))) {
        return false;
    }
    if (!read_text(reader, root, "", "schema", true, text, sizeof(text), NULL)) {
        return false;
    }
    if (strcmp(text, "ursatz-form") != 0) {
        return fail(reader, "/schema", "must be \"ursatz-form\"");
    }
    const cJSON *version = member_of(reader, root, "", "version", true);
    if (!version) {
        return false;
    }
    long number;
    if (!whole(reader, version, "/version", 1, 1, &number)) {
        return false;
    }
    if (!read_name(reader, root, "", "name", true, form->name, sizeof(form->name), NULL) ||
        !read_text(reader, root, "", "label", true, form->label, sizeof(form->label), NULL)) {
        return false;
    }
    if (!read_name(reader, root, "", "plan_name", false, form->plan_name, sizeof(form->plan_name), &present)) {
        return false;
    }
    if (!present) {
        snprintf(form->plan_name, sizeof(form->plan_name), "%s", form->name);
    }
    form->checks = FORM_CHECK_ALL;
    if (!read_bool(reader, root, "", "closing", &form->closing)) {
        return false;
    }
    if (member_of(reader, root, "", "accompaniment", false)) {
        if (!read_text(reader, root, "", "accompaniment", false, text, sizeof(text), NULL)) {
            return false;
        }
        if (strcmp(text, "required") == 0) {
            form->accompaniment_required = true;
        } else if (strcmp(text, "optional") != 0) {
            return fail(reader, "/accompaniment", "must be \"optional\" or \"required\"");
        }
    }
    if (!read_regions(reader, root, form) || !read_checks(reader, root, form)) {
        return false;
    }
    const cJSON *sections = member_of(reader, root, "", "sections", false);
    if (sections) {
        if (!cJSON_IsString(sections) || !sections->valuestring || strcmp(sections->valuestring, "request") != 0) {
            return fail(reader, "/sections", "must be \"request\"");
        }
        form->request_sections = true;
    }
    if (!read_sizes(reader, root, form)) {
        return false;
    }
    if (form->request_sections) {
        return read_request_members(reader, root, form);
    }
    for (size_t i = 0; i < sizeof(REQUEST_ONLY) / sizeof(REQUEST_ONLY[0]); i++) {
        if (cJSON_GetObjectItemCaseSensitive(root, REQUEST_ONLY[i])) {
            char at[64];
            join(at, sizeof(at), "", REQUEST_ONLY[i]);
            return fail(reader, at, "only a form whose sections the request gives has this member");
        }
    }
    return true;
}

FormSpecResult form_spec_parse(const char *json, FormSpec **out, FormSpecError *error) {
    if (out) {
        *out = NULL;
    }
    if (error) {
        memset(error, 0, sizeof(*error));
    }
    if (!json || !out) {
        return FORM_SPEC_ERR_NULL;
    }
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "not JSON");
        }
        return FORM_SPEC_ERR_PARSE;
    }
    FormSpec *form = calloc(1, sizeof(*form));
    if (!form) {
        cJSON_Delete(root);
        return FORM_SPEC_ERR_ALLOC;
    }
    Reader reader = {error, false};
    bool ok = read_document(&reader, root, form);
    cJSON_Delete(root);
    if (!ok) {
        free(form);
        return FORM_SPEC_ERR_INVALID;
    }
    *out = form;
    return FORM_SPEC_OK;
}

void form_spec_destroy(FormSpec *form) {
    free(form);
}
