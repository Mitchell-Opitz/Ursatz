#include <ursatz_composer/form_spec.h>

#include <cJSON.h>
#include <standard-harmony/harmony_cadence_kinds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/theme_shape.h>

#include <ursatz_composer/harmony_phrase.h>

#include "grow/grow_internal.h"
#include "util/request_parse.h"

/* ------------------------------------------------------------- the vocabularies the document names */

typedef struct {
    const char *name;
    const char *label;
    bool theme;
} RoleRow;

/* The role table: P46 to P53 add rows. */
static const RoleRow ROLES[FORM_ROLE_COUNT] = {
    {"theme", "Theme", true},         {"contrast", "Contrast", true},   {"return", "Return", true},
    {"variation", "Variation", true}, {"layer", "Layer", true},         {"pattern", "Pattern", true},
    {"sequence", "Sequence", true},   {"episode", "Episode", true},    {"codetta", "Codetta", true},
    {"liquidation", "Liquidation", true}, {"transition", "Transition", true}, {"retransition", "Retransition", true},
    {"subordinate", "Subordinate theme", true}, {"coda", "Coda", true}, {"elaboration", "Elaboration", true}, {"intro", "Intro", false},
    {"body", "Loop body", false}, {"turnaround", "Turnaround", false},  {"ending", "Ending", false}};

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


static const struct {
    const char *name;
    PlanLaneKind lane;
} LANES[] = {{"bass", PLAN_LANE_BASS}, {"inner_voice", PLAN_LANE_INNER_VOICE}, {"melody", PLAN_LANE_MELODY}};
#define LANE_COUNT (sizeof(LANES) / sizeof(LANES[0]))

/* ------------------------------------------------------------- reading */

typedef struct {
    FormSpecError *error;
    bool failed;
    bool part; /* reading a built-in form for the part of another: it is not itself compound */
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

/*
 * A figure (R-497, R-498): {"kind", "rhythm", "harmony", "change_at", "climax"}. kind and rhythm are required; harmony is
 * "kept" (default; "reharmonized" in the parallel mode, which refuses "kept", R-499) or "reharmonized"; change_at is a bar of the section from 1, before its last, and needs the harmony
 * reharmonized; climax is a bool, false by default.
 */
static bool read_figure(Reader *reader, const cJSON *figure, const char *path, size_t bars, bool parallel,
                        PlanFigure *out) {
    static const char *const MEMBERS[] = {"kind", "rhythm", "harmony", "change_at", "climax"};
    static const char *const KIND_NAMES[] = {"neighbor", "scale", "broken-chord"};
    static const char *const RHYTHM_NAMES[] = {"eighths", "sixteenths"};
    static const char *const HARMONY_NAMES[] = {"kept", "reharmonized"};
    int kind = -1;
    int rhythm = -1;
    int harmony = parallel ? 1 : 0;
    char at[64];
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsObject(figure)) {
        return fail(reader, path, "must be an object");
    }
    if (!members(reader, figure, path, MEMBERS, sizeof(MEMBERS) / sizeof(MEMBERS[0])) ||
        !member_of(reader, figure, path, "kind", true) || !member_of(reader, figure, path, "rhythm", true) ||
        !read_choice(reader, figure, path, "kind", KIND_NAMES, 3, &kind) ||
        !read_choice(reader, figure, path, "rhythm", RHYTHM_NAMES, 2, &rhythm) ||
        !read_choice(reader, figure, path, "harmony", HARMONY_NAMES, 2, &harmony) ||
        !read_bool(reader, figure, path, "climax", &out->climax)) {
        return false;
    }
    out->kind = (PlanFigureKind)kind;
    out->rhythm = (PlanFigureRhythm)rhythm;
    out->reharmonize = harmony == 1;
    if (parallel && !out->reharmonize) {
        join(at, sizeof(at), path, "harmony");
        return fail(reader, at, "a variation in the other mode is reharmonized");
    }
    const cJSON *change_at = member_of(reader, figure, path, "change_at", false);
    if (change_at) {
        long bar;
        join(at, sizeof(at), path, "change_at");
        if (!whole(reader, change_at, at, -4096, 4096, &bar)) {
            return false;
        }
        if (!out->reharmonize) {
            return fail(reader, at, "a root change needs the harmony reharmonized");
        }
        if (bar < 1 || (size_t)bar >= bars) {
            return fail(reader, at, "change_at must be a bar of the section before its last");
        }
        out->change_at = (unsigned)bar;
    }
    return true;
}

static bool read_section(Reader *reader, const FormSpec *form, const FormSpecSize *size, const cJSON *json,
                         const char *path, FormSectionSpec *out) {
    static const char *const ALLOWED[] = {"name",    "letter",  "role",   "bars",   "beats",   "beginning", "ending",
                                          "cadence", "closed",  "keys",   "motive", "returns", "variation", "layers",
                                          "pedal",   "construction", "leads_in", "connective", "figure", "mode",
                                          "independent", "lyric", "setting"};
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
    /* a liquidation (R-494) is the pattern liquidated over a dominant pedal, a retransition (R-505) and a coda (R-509) are
     * grown by an operator of their own: their shape is the operator's, not the form's */
    const bool liquidation = out->role == FORM_ROLE_LIQUIDATION;
    const bool retransition = out->role == FORM_ROLE_RETRANSITION;
    const bool coda = out->role == FORM_ROLE_CODA;
    const bool operator_section = liquidation || retransition || coda;
    const bool transition = out->role == FORM_ROLE_TRANSITION;
    if (liquidation) {
        static const char *const REFUSED[] = {"beginning", "ending",  "construction", "motive", "returns", "variation",
                                              "pedal",     "leads_in", "connective",   "layers", "beats",
                                              "figure"};
        for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
            if (cJSON_GetObjectItemCaseSensitive(json, REFUSED[i])) {
                join(at, sizeof(at), path, REFUSED[i]);
                return i < 2 ? fail(reader, at, "a liquidation has no beginning or ending")
                             : fail(reader, at, "a liquidation has no %s", REFUSED[i]);
            }
        }
    }
    if (retransition || coda) {
        static const char *const REFUSED[] = {"beginning", "ending",  "construction", "motive", "returns", "variation",
                                              "pedal",     "leads_in", "connective",   "layers", "beats",
                                              "figure",    "mode",     "keys",         "independent", "lyric"};
        const char *noun = retransition ? "retransition" : "coda";
        for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
            if (cJSON_GetObjectItemCaseSensitive(json, REFUSED[i])) {
                join(at, sizeof(at), path, REFUSED[i]);
                return i < 2 ? fail(reader, at, "a %s has no beginning or ending", noun)
                             : fail(reader, at, "a %s has no %s", noun, REFUSED[i]);
            }
        }
    }
    if (transition) {
        static const char *const REFUSED[] = {"returns", "variation", "figure", "mode", "pedal"};
        for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
            if (cJSON_GetObjectItemCaseSensitive(json, REFUSED[i])) {
                join(at, sizeof(at), path, REFUSED[i]);
                return fail(reader, at, "a transition has no %s", REFUSED[i]);
            }
        }
    }
    /* independent (R-504) and lyric (R-506) */
    if (!transition && cJSON_GetObjectItemCaseSensitive(json, "independent")) {
        join(at, sizeof(at), path, "independent");
        return fail(reader, at, "only a transition is independent");
    }
    if (out->role != FORM_ROLE_SUBORDINATE && cJSON_GetObjectItemCaseSensitive(json, "lyric")) {
        join(at, sizeof(at), path, "lyric");
        return fail(reader, at, "only a subordinate theme is lyric");
    }
    if (!read_bool(reader, json, path, "independent", &out->independent) || !read_bool(reader, json, path, "lyric", &out->lyric)) {
        return false;
    }
    if (out->independent && cJSON_GetObjectItemCaseSensitive(json, "motive")) {
        join(at, sizeof(at), path, "motive");
        return fail(reader, at, "an independent transition has its own motive");
    }
    if (out->lyric) {
        if (cJSON_GetObjectItemCaseSensitive(json, "motive")) {
            join(at, sizeof(at), path, "motive");
            return fail(reader, at, "a lyric theme's motive is m0 augmented");
        }
        if (cJSON_GetObjectItemCaseSensitive(json, "returns")) {
            join(at, sizeof(at), path, "returns");
            return fail(reader, at, "a lyric theme returns nothing");
        }
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
    if (liquidation && out->bars != 4) {
        join(at, sizeof(at), path, "bars");
        return fail(reader, at, "a liquidation is 4 bars");
    }
    if (retransition && out->bars != 4) {
        join(at, sizeof(at), path, "bars");
        return fail(reader, at, "a retransition is 4 bars");
    }
    if (coda && out->bars != 8) {
        join(at, sizeof(at), path, "bars");
        return fail(reader, at, "a coda is 8 bars");
    }
    /* beginning and ending: the theme shape's plan names (a liquidation, retransition or coda has none: its pairing stays zero) */
    if (!operator_section) {
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
        if (transition && out->pairing.beginning != PLAN_THEME_BEGINNING_PRESENTATION &&
            out->pairing.beginning != PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA) {
            join(at, sizeof(at), path, "beginning");
            return fail(reader, at, "a transition begins with a presentation or a compound basic idea");
        }
        if (transition && out->pairing.ending != PLAN_THEME_ENDING_CONTINUATION) {
            join(at, sizeof(at), path, "ending");
            return fail(reader, at, "a transition ends with a continuation");
        }
        if (out->lyric && (out->pairing.beginning != PLAN_THEME_BEGINNING_ANTECEDENT ||
                           out->pairing.ending != PLAN_THEME_ENDING_CONSEQUENT)) {
            join(at, sizeof(at), path, "beginning");
            return fail(reader, at, "a lyric theme is a period");
        }
    }
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
    if (liquidation && out->cadence != CADENCE_TYPE_HALF) {
        join(at, sizeof(at), path, "cadence");
        return fail(reader, at, "a liquidation ends in a half cadence");
    }
    if ((retransition || transition) && out->cadence != CADENCE_TYPE_HALF) {
        join(at, sizeof(at), path, "cadence");
        return fail(reader, at, retransition ? "a retransition ends in a half cadence" : "a transition ends in a half cadence");
    }
    if (coda && out->cadence != CADENCE_TYPE_PERFECT_AUTHENTIC) {
        join(at, sizeof(at), path, "cadence");
        return fail(reader, at, "a coda ends in a perfect authentic cadence");
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
    if (transition && out->key_end != out->key_start && out->bars != 8) {
        join(at, sizeof(at), path, "bars");
        return fail(reader, at, "a transition that modulates is 8 bars");
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
    /* a lyric theme's motive is m0 augmented, the open ratio the builder fills (R-506) */
    if (out->lyric) {
        out->motive.kind = MOTIVE_RELATION_AUGMENT;
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
    /* setting (R-518): the pattern of a setting return's new accompaniment, required by that variation and only for it */
    const bool setting_return = returns && out->variation == PLAN_RETURN_SETTING;
    if (cJSON_GetObjectItemCaseSensitive(json, "setting")) {
        static const char *const SETTING_NAMES[] = {"block", "arpeggiated", "alberti", "oom-pah"};
        int setting = -1;
        join(at, sizeof(at), path, "setting");
        if (!read_choice(reader, json, path, "setting", SETTING_NAMES, 4, &setting)) {
            return false;
        }
        if (!setting_return) {
            return fail(reader, at, "only a setting return has a setting");
        }
        out->setting = (PlanAccompanimentPattern)(PLAN_ACCOMPANIMENT_BLOCK + setting);
    } else if (setting_return) {
        join(at, sizeof(at), path, "variation");
        return fail(reader, at, "a setting return names its setting");
    }
    /* mode: "parallel" (R-499), the section is in the home key's parallel key, only a figurate variation, with no keys;
     * "major" (R-517), the section is in the home tonic's major key, only a return, with no keys, not figurate;
     * "home" (R-524), the section is at home in the home mode, its source copied across modes when it is in the other,
     * only a reharmonize return, with no keys */
    const cJSON *mode = member_of(reader, json, path, "mode", false);
    const bool figurate = returns && out->variation == PLAN_RETURN_FIGURATE;
    if (mode) {
        static const char *const MODE_NAMES[] = {"parallel", "major", "home"};
        int value = -1;
        join(at, sizeof(at), path, "mode");
        if (!read_choice(reader, json, path, "mode", MODE_NAMES, 3, &value)) {
            return false;
        }
        if (value == 2) {
            if (keys) {
                return fail(reader, at, "a section in the home mode has no keys");
            }
            if (!returns) {
                return fail(reader, at, "the home mode is for a returning section");
            }
            if (out->variation != PLAN_RETURN_REHARMONIZE) {
                return fail(reader, at, "a return in the home mode is reharmonized");
            }
            out->home_mode = true;
        } else if (value == 0) {
            if (keys) {
                return fail(reader, at, "a section in the parallel mode has no keys");
            }
            if (!figurate) {
                return fail(reader, at, "the parallel mode needs a figurate variation");
            }
            out->parallel = true;
        } else {
            if (keys) {
                return fail(reader, at, "a section in the tonic major has no keys");
            }
            if (!returns) {
                return fail(reader, at, "the tonic major is for a returning section");
            }
            if (figurate) {
                return fail(reader, at, "a figurate variation takes the parallel mode, not the tonic major");
            }
            out->tonic_major = true;
        }
    }
    /* figure (R-497): the motive of a figurate return, and only of one */
    const cJSON *figure = member_of(reader, json, path, "figure", false);
    if (figure) {
        join(at, sizeof(at), path, "figure");
        if (!figurate) {
            return fail(reader, at, "a figure needs the variation figurate");
        }
        if (!bars) {
            return fail(reader, at, "a figure needs bars");
        }
        if (!read_figure(reader, figure, at, out->bars, out->parallel, &out->figure)) {
            return false;
        }
        out->has_figure = true;
    } else if (figurate) {
        join(at, sizeof(at), path, "variation");
        return fail(reader, at, "a figurate variation needs a figure");
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
    /* leads_in and connective (R-486): the rules that look at other sections are checked once the size is read */
    if (!read_bool(reader, json, path, "leads_in", &out->leads_in)) {
        return false;
    }
    if (out->leads_in && out->returns != FORM_SPEC_NO_RETURN) {
        join(at, sizeof(at), path, "leads_in");
        return fail(reader, at, "a section that returns another cannot lead in");
    }
    if (cJSON_GetObjectItemCaseSensitive(json, "connective")) {
        static const char *const CONNECTIVE_NAMES[] = {"sometimes", "always"};
        int choice = -1;
        join(at, sizeof(at), path, "connective");
        if (!read_choice(reader, json, path, "connective", CONNECTIVE_NAMES, 2, &choice)) {
            return false;
        }
        if (!out->leads_in) {
            return fail(reader, at, "a connective needs leads_in");
        }
        out->connective = choice == 0 ? FORM_CONNECTIVE_SOMETIMES : FORM_CONNECTIVE_ALWAYS;
    }
    return read_layers(reader, json, path, &out->layers);
}

/* leads_in across the sections of a size: not on the last, not on two in a row, not on one another section returns. */
static bool check_lead_ins(Reader *reader, const FormSpecSize *size, const char *sections_at) {
    char section_at[64];
    char at[64];
    for (size_t s = 0; s < size->section_count; s++) {
        if (!size->sections[s].leads_in) {
            continue;
        }
        join_index(section_at, sizeof(section_at), sections_at, s);
        join(at, sizeof(at), section_at, "leads_in");
        if (s + 1 == size->section_count) {
            return fail(reader, at, "the last section of a size has no section to lead into");
        }
        if (size->sections[s + 1].leads_in) {
            return fail(reader, at, "the next section also leads in");
        }
        for (size_t other = 0; other < size->section_count; other++) {
            if (size->sections[other].returns == s) {
                return fail(reader, at, "a section that another section returns cannot lead in");
            }
        }
    }
    return true;
}

/* repeats (R-485): groups of consecutive sections, by name, the same indexes in every size. */
static bool read_repeats(Reader *reader, const cJSON *root, FormSpec *form) {
    char at[64];
    if (form->request_sections && cJSON_GetObjectItemCaseSensitive(root, "repeats")) {
        return fail(reader, "/repeats", "only a form with sizes has this member");
    }
    const cJSON *list = array_member(reader, root, "", "repeats", false, at, sizeof(at));
    if (reader->failed || !list) {
        return !reader->failed;
    }
    int n = cJSON_GetArraySize(list);
    if (n < 1 || n > FORM_SPEC_MAX_SECTIONS) {
        return fail(reader, at, "must list 1 to %d groups", FORM_SPEC_MAX_SECTIONS);
    }
    size_t next_free = 0; /* the first section index a group may start at: groups are in order and do not overlap */
    for (int i = 0; i < n; i++) {
        char group_at[64];
        const cJSON *group = cJSON_GetArrayItem(list, i);
        join_index(group_at, sizeof(group_at), at, (size_t)i);
        if (!cJSON_IsArray(group) || cJSON_GetArraySize(group) < 1) {
            return fail(reader, group_at, "must list the names of one or more sections");
        }
        FormRepeat repeat = {0, (size_t)cJSON_GetArraySize(group)};
        for (int j = 0; j < cJSON_GetArraySize(group); j++) {
            char item[64];
            const cJSON *entry = cJSON_GetArrayItem(group, j);
            join_index(item, sizeof(item), group_at, (size_t)j);
            if (!cJSON_IsString(entry) || !entry->valuestring) {
                return fail(reader, item, "must be a section name");
            }
            for (size_t z = 0; z < form->size_count; z++) {
                const FormSpecSize *size = &form->sizes[z];
                size_t index = size->section_count;
                for (size_t s = 0; s < size->section_count; s++) {
                    if (strcmp(size->sections[s].name, entry->valuestring) == 0) {
                        index = s;
                        break;
                    }
                }
                if (index == size->section_count) {
                    return fail(reader, item, "\"%s\" is not a section of every size", entry->valuestring);
                }
                if (j == 0 && z == 0) {
                    repeat.first = index;
                }
                if (index != repeat.first + (size_t)j) {
                    return fail(reader, item, "the sections of a group must be consecutive and in order in every size");
                }
            }
            if (repeat.first + (size_t)j < next_free) {
                return fail(reader, item, "groups must be in order and not overlap");
            }
        }
        next_free = repeat.first + repeat.count;
        form->repeats[form->repeat_count++] = repeat;
    }
    return true;
}

/* ------------------------------------------------------------- parts */

static FormSpecResult parse_form(const char *json, FormSpec **out, FormSpecError *error, bool part);

/* A built-in form read for the part of another: its own parts are not allowed. */
static FormSpecResult builtin_part_form(const char *name, FormSpec **out) {
    for (size_t i = 0; form_spec_builtin_name(i); i++) {
        if (strcmp(name, form_spec_builtin_name(i)) == 0) {
            return parse_form(form_spec_builtin_document(i), out, NULL, true);
        }
    }
    *out = NULL;
    return FORM_SPEC_ERR_UNKNOWN;
}

/* parts (R-485): the parts of a compound form, each a piece of a built-in form with sizes, or an earlier part again. */
static bool read_parts(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const ALLOWED[] = {"name", "form", "key", "motive", "again", "repeats", "bars"};
    char at[64];
    const cJSON *list = array_member(reader, root, "", "parts", false, at, sizeof(at));
    if (reader->failed || !list) {
        return !reader->failed;
    }
    if (form->request_sections) {
        return fail(reader, at, "only a form with sizes has this member");
    }
    if (reader->part) {
        return fail(reader, at, "a part's form is not itself compound");
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n < 1) {
        return fail(reader, at, "must hold at least one part");
    }
    if (n > FORM_SPEC_MAX_PARTS_OF) {
        return fail(reader, at, "at most %d parts", FORM_SPEC_MAX_PARTS_OF);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        char member_at[64];
        const cJSON *json = cJSON_GetArrayItem(list, (int)i);
        FormPart *part = &form->parts[i];
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsObject(json)) {
            return fail(reader, item, "must be an object");
        }
        memset(part, 0, sizeof(*part));
        part->key = FORM_SPEC_HOME;
        part->again = FORM_SPEC_NO_RETURN;
        part->repeats = true;
        if (!members(reader, json, item, ALLOWED, sizeof(ALLOWED) / sizeof(ALLOWED[0])) ||
            !read_text(reader, json, item, "name", true, part->name, sizeof(part->name), NULL)) {
            return false;
        }
        for (size_t other = 0; other < i; other++) {
            if (strcmp(form->parts[other].name, part->name) == 0) {
                join(member_at, sizeof(member_at), item, "name");
                return fail(reader, member_at, "part name \"%s\" repeated", part->name);
            }
        }
        const cJSON *again = member_of(reader, json, item, "again", false);
        if (again) {
            join(member_at, sizeof(member_at), item, "again");
            if (!cJSON_IsString(again) || !again->valuestring) {
                return fail(reader, member_at, "must be the name of an earlier part");
            }
            for (size_t other = 0; other < i; other++) {
                if (strcmp(form->parts[other].name, again->valuestring) == 0) {
                    part->again = other;
                }
            }
            if (part->again == FORM_SPEC_NO_RETURN) {
                return fail(reader, member_at, "\"%s\" is not an earlier part", again->valuestring);
            }
            if (form->parts[part->again].again != FORM_SPEC_NO_RETURN) {
                return fail(reader, member_at, "\"%s\" is itself an again part", again->valuestring);
            }
            static const char *const NOT_WITH_AGAIN[] = {"form", "key", "motive", "bars"};
            for (size_t m = 0; m < 4; m++) {
                if (cJSON_GetObjectItemCaseSensitive(json, NOT_WITH_AGAIN[m])) {
                    join(member_at, sizeof(member_at), item, NOT_WITH_AGAIN[m]);
                    return fail(reader, member_at, "a part that performs another again takes no %s", NOT_WITH_AGAIN[m]);
                }
            }
            part->repeats = false;
        } else {
            FormSpec *sub = NULL;
            bool known = true;
            if (!read_name(reader, json, item, "form", true, part->form, sizeof(part->form), &known)) {
                return false;
            }
            join(member_at, sizeof(member_at), item, "form");
            if (strcmp(part->form, form->name) == 0) {
                return fail(reader, member_at, "a part's form is not itself compound");
            }
            FormSpecResult found = builtin_part_form(part->form, &sub);
            if (found == FORM_SPEC_ERR_UNKNOWN) {
                return fail(reader, member_at, "\"%s\" is not a built-in form", part->form);
            }
            if (found != FORM_SPEC_OK || sub->request_sections || sub->size_count == 0) {
                form_spec_destroy(sub);
                return fail(reader, member_at, "\"%s\" is not a form with sizes that is not itself compound", part->form);
            }
            const cJSON *own_bars = member_of(reader, json, item, "bars", false);
            if (own_bars) {
                long bars;
                char bars_at[64];
                join(bars_at, sizeof(bars_at), item, "bars");
                if (!whole(reader, own_bars, bars_at, 1, 4096, &bars)) {
                    form_spec_destroy(sub);
                    return false;
                }
                if (!form_spec_size(sub, (size_t)bars, 0)) {
                    form_spec_destroy(sub);
                    return fail(reader, bars_at, "\"%s\" has no size of %ld bars", part->form, bars);
                }
                part->bars = (size_t)bars;
            }
            for (size_t z = 0; z < form->size_count && !part->bars; z++) {
                if (!form_spec_size(sub, form->sizes[z].bars, 0)) {
                    form_spec_destroy(sub);
                    return fail(reader, member_at, "\"%s\" has no size of %zu bars", part->form, form->sizes[z].bars);
                }
            }
            form_spec_destroy(sub);
            if (!read_key(reader, form, json, item, "key", &part->key)) {
                return false;
            }
            const cJSON *motive = member_of(reader, json, item, "motive", false);
            if (motive) {
                join(member_at, sizeof(member_at), item, "motive");
                if (!cJSON_IsString(motive) || !motive->valuestring ||
                    (strcmp(motive->valuestring, "piece") != 0 && strcmp(motive->valuestring, "new") != 0)) {
                    return fail(reader, member_at, "must be \"piece\" or \"new\"");
                }
                part->new_motive = strcmp(motive->valuestring, "new") == 0;
            }
        }
        if (!read_bool(reader, json, item, "repeats", &part->repeats)) {
            return false;
        }
        form->part_count = i + 1;
    }
    return true;
}

bool form_spec_performed(const FormSpec *form) {
    return form && (form->repeat_count > 0 || form->part_count > 0);
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

static bool read_sizes(Reader *reader, const cJSON *root, FormSpec *form, bool compound) {
    static const char *const ALLOWED[] = {"bars", "count", "default", "sections"};
    static const char *const COMPOUND_ALLOWED[] = {"bars", "default", "sections"};
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
        if (compound && cJSON_GetObjectItemCaseSensitive(json, "sections")) {
            join(sections_at, sizeof(sections_at), item, "sections");
            return fail(reader, sections_at, "a compound form's sizes have bars and no sections");
        }
        if (!members(reader, json, item, compound ? COMPOUND_ALLOWED : ALLOWED, compound ? 3 : 4)) {
            return false;
        }
        long bars;
        long count = 0;
        join(sections_at, sizeof(sections_at), item, "bars");
        const cJSON *bars_item = member_of(reader, json, item, "bars", true);
        if (!bars_item || !whole(reader, bars_item, sections_at, 1, 4096, &bars)) {
            return false;
        }
        const cJSON *count_item = compound ? NULL : member_of(reader, json, item, "count", false);
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
        if (compound) {
            for (size_t other = 0; other < i; other++) {
                if (form->sizes[other].bars == size->bars) {
                    return fail(reader, item, "a size with these bars is already listed");
                }
            }
            form->size_count = i + 1;
            continue;
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
        if (!check_lead_ins(reader, size, sections_at)) {
            return false;
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

/* meters and tempo: a form with sizes may list the meters it is written in and give a tempo (R-487). */
static bool read_meters_and_tempo(Reader *reader, const cJSON *root, FormSpec *form) {
    char at[64];
    for (size_t i = 0; i < 2; i++) {
        const char *key = i == 0 ? "meters" : "tempo";
        if (form->request_sections && cJSON_GetObjectItemCaseSensitive(root, key)) {
            join(at, sizeof(at), "", key);
            return fail(reader, at, "only a form with sizes has this member");
        }
    }
    const cJSON *list = array_member(reader, root, "", "meters", false, at, sizeof(at));
    if (reader->failed) {
        return false;
    }
    if (list) {
        int n = cJSON_GetArraySize(list);
        if (n < 1 || n > FORM_SPEC_MAX_METERS) {
            return fail(reader, at, "must list 1 to %d meters", FORM_SPEC_MAX_METERS);
        }
        for (int i = 0; i < n; i++) {
            char item[64];
            PlanMeter meter;
            const cJSON *entry = cJSON_GetArrayItem(list, i);
            join_index(item, sizeof(item), at, (size_t)i);
            if (!cJSON_IsString(entry) || !entry->valuestring) {
                return fail(reader, item, "must be a meter such as \"3/4\"");
            }
            if (!request_parse_meter(entry->valuestring, &meter) || !harmony_meter_supported(&meter)) {
                return fail(reader, item, "\"%s\" is not a meter the builder supports", entry->valuestring);
            }
            for (size_t other = 0; other < form->meter_count; other++) {
                if (form->meters[other].beats_per_bar == meter.beats_per_bar &&
                    form->meters[other].beat_unit == meter.beat_unit) {
                    return fail(reader, item, "the meter is already listed");
                }
            }
            form->meters[form->meter_count++] = meter;
        }
    }
    const cJSON *tempo = member_of(reader, root, "", "tempo", false);
    if (tempo) {
        long number;
        if (!whole(reader, tempo, "/tempo", 30, 300, &number)) {
            return false;
        }
        form->tempo = (unsigned)number;
    }
    return true;
}

bool form_spec_meter_allowed(const FormSpec *form, const PlanMeter *meter) {
    if (!form || !meter || form->meter_count == 0) {
        return true;
    }
    for (size_t i = 0; i < form->meter_count; i++) {
        if (form->meters[i].beats_per_bar == meter->beats_per_bar && form->meters[i].beat_unit == meter->beat_unit) {
            return true;
        }
    }
    return false;
}

static bool read_document(Reader *reader, const cJSON *root, FormSpec *form) {
    static const char *const ALLOWED[] = {"schema", "version", "name",    "label",  "plan_name", "closing", "accompaniment",
                                          "regions", "checks", "sizes", "sections", "roles",   "loop",    "cadences",
                                          "default_ending", "meters", "tempo", "repeats", "parts", "modulates"};
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
    if (!read_bool(reader, root, "", "closing", &form->closing) ||
        !read_bool(reader, root, "", "modulates", &form->modulates)) {
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
    if (form->modulates && form->request_sections) {
        return fail(reader, "/modulates", "a form whose sections the request gives cannot always modulate");
    }
    if (form->modulates && form->region_count == 0) {
        return fail(reader, "/modulates", "a form that always modulates needs regions");
    }
    bool compound = cJSON_GetObjectItemCaseSensitive(root, "parts") != NULL;
    if (compound && cJSON_GetObjectItemCaseSensitive(root, "repeats")) {
        return fail(reader, "/repeats", "a compound form has parts, not repeats");
    }
    if (!read_meters_and_tempo(reader, root, form) || !read_sizes(reader, root, form, compound) ||
        !read_repeats(reader, root, form) || !read_parts(reader, root, form)) {
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

static FormSpecResult parse_form(const char *json, FormSpec **out, FormSpecError *error, bool part) {
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
    Reader reader = {error, false, part};
    bool ok = read_document(&reader, root, form);
    cJSON_Delete(root);
    if (!ok) {
        free(form);
        return FORM_SPEC_ERR_INVALID;
    }
    *out = form;
    return FORM_SPEC_OK;
}

FormSpecResult form_spec_parse(const char *json, FormSpec **out, FormSpecError *error) {
    return parse_form(json, out, error, false);
}

void form_spec_destroy(FormSpec *form) {
    free(form);
}
