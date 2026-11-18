#include <ursatz_composer/form_spec.h>

#include <cJSON.h>
#include <plan/section_json.h>
#include <theory/harmony/harmony_cadence_kinds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/theme_shape.h>

#include <ursatz_composer/harmony_phrase.h>

#include "grow/grow_internal.h"
#include "util/request_parse.h"

/* ------------------------------------------------------------- the vocabularies the document names */

static const struct {
    const char *name;
    FormCheck check;
} CHECKS[] = {{"sections", FORM_CHECK_SECTIONS},
              {"returns", FORM_CHECK_RETURNS},
              {"keys", FORM_CHECK_KEYS},
              {"cadences", FORM_CHECK_CADENCES},
              {"seam", FORM_CHECK_SEAM}};
#define CHECK_COUNT (sizeof(CHECKS) / sizeof(CHECKS[0]))


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

/* A list of relation ids of the knowledge snapshot (K6): 1 to FORM_SPEC_MAX_DEGREES of them. */
static bool read_relations(Reader *reader, const cJSON *region, const char *path, const char *key, char (*out)[SECTION_WORD_MAX],
                           size_t *count) {
    char at[64];
    const cJSON *list = array_member(reader, region, path, key, true, at, sizeof(at));
    if (!list) {
        return false;
    }
    size_t n = (size_t)cJSON_GetArraySize(list);
    if (n < 1 || n > FORM_SPEC_MAX_DEGREES) {
        return fail(reader, at, "must hold 1 to %d relations", FORM_SPEC_MAX_DEGREES);
    }
    for (size_t i = 0; i < n; i++) {
        char item[64];
        const cJSON *id = cJSON_GetArrayItem(list, (int)i);
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsString(id) || !id->valuestring) {
            return fail(reader, item, "must be a relation id such as \"omt.relation.dominant\"");
        }
        if (id->valuestring[0] == '\0' || strlen(id->valuestring) >= SECTION_WORD_MAX) {
            return fail(reader, item, "must be 1 to %d characters", SECTION_WORD_MAX - 1);
        }
        snprintf(out[i], SECTION_WORD_MAX, "%s", id->valuestring);
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
            !read_relations(reader, region, item, "major", out->major, &out->major_count) ||
            !read_relations(reader, region, item, "minor", out->minor, &out->minor_count)) {
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

int form_spec_region_of(const FormSpec *form, const SectionRegion *region) {
    if (region->kind != SECTION_REGION_FORM) {
        return FORM_SPEC_HOME;
    }
    int found = form_spec_region_find(form, region->form_region);
    return found < 0 ? FORM_SPEC_HOME : found;
}

size_t form_spec_section_index(const FormSpecSize *size, const char *name) {
    for (size_t i = 0; size && name && name[0] && i < size->section_count; i++) {
        if (strcmp(size->sections[i].name, name) == 0) {
            return i;
        }
    }
    return FORM_SPEC_NO_RETURN;
}

bool form_spec_section_motive(const SectionDescription *section, MotiveRelationStep *out) {
    if (section->function.lyric) {
        memset(out, 0, sizeof(*out));
        out->kind = MOTIVE_RELATION_AUGMENT;
        return true;
    }
    return section->motive[0] != '\0' && motive_relation_step_parse(section->motive, out);
}

/* ------------------------------------------------------------- sections */

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

/* A compound form's part key: "home" or the name of a region. */
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

/* ------------------------------------------------------------- a section */

/* The member's path under the section's. */
static const char *member_at(char *out, size_t cap, const char *path, const char *member) {
    join(out, cap, path, member);
    return out;
}

/* Whether two sections' constructions are the same: both none, both "auto", or both the same ideas and extensions. */
static bool same_construction(const SectionDescription *a, const SectionDescription *b) {
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
        const SectionExtension *x = &a->construction.extensions[k];
        const SectionExtension *y = &b->construction.extensions[k];
        if (x->kind != y->kind || x->idea != y->idea || x->bars != y->bars || x->varied != y->varied) {
            return false;
        }
    }
    return true;
}

/*
 * The rules about a section that need the Composer: the form's regions, the sections before it in its size, the motive
 * grammar, the lengths and cadences each role has, and the construction's bars. The library codec has read the section
 * and checked what is about the section itself (R-605). `path` is the section's, "/sizes/0/sections/1".
 */
static bool form_section_check(Reader *reader, const FormSpec *form, const FormSpecSize *size, const SectionDescription *d,
                               size_t index, const char *path) {
    char at[96];
    const bool liquidation = d->role == SECTION_ROLE_LIQUIDATION;
    const bool retransition = d->role == SECTION_ROLE_RETRANSITION;
    const bool coda = d->role == SECTION_ROLE_CODA;
    const bool transition = d->role == SECTION_ROLE_TRANSITION;
    /* a name is unique in its size */
    for (size_t i = 0; i < index; i++) {
        if (strcmp(size->sections[i].name, d->name) == 0) {
            return fail(reader, member_at(at, sizeof(at), path, "name"), "section name \"%s\" repeated in the size", d->name);
        }
    }
    /* a liquidation (R-494), a retransition (R-505) and a coda (R-509) have the length of their operator */
    if ((liquidation || retransition) && d->bars != 4) {
        return fail(reader, member_at(at, sizeof(at), path, "bars"), "a %s is 4 bars", liquidation ? "liquidation" : "retransition");
    }
    if (coda && d->bars != 8) {
        return fail(reader, member_at(at, sizeof(at), path, "bars"), "a coda is 8 bars");
    }
    if (transition && d->pairing.beginning != SECTION_BEGINNING_PRESENTATION &&
        d->pairing.beginning != SECTION_BEGINNING_COMPOUND_BASIC_IDEA) {
        return fail(reader, member_at(at, sizeof(at), path, "beginning"), "a transition begins with a presentation or a compound basic idea");
    }
    if (transition && d->pairing.ending != SECTION_ENDING_CONTINUATION) {
        return fail(reader, member_at(at, sizeof(at), path, "ending"), "a transition ends with a continuation");
    }
    /* construction: a theme of any whole bars from 3 to 16 (R-468); a theme-like section of other bars than 4 or 8 needs it */
    if (d->has_construction) {
        if (d->bars < 3 || d->bars > 16) {
            return fail(reader, member_at(at, sizeof(at), path, "bars"), "a section with a construction is 3 to 16 bars");
        }
        if (!d->construction_auto) {
            size_t total = grow_construction_extra_bars(&d->pairing, &d->construction);
            for (int idea = 0; idea < SECTION_CONSTRUCTION_IDEAS; idea++) {
                total += d->construction.idea_bars[idea];
            }
            if (total != d->bars) {
                return fail(reader, member_at(at, sizeof(at), path, "construction"),
                            "the construction adds up to %zu bars but the section is %zu", total, d->bars);
            }
        }
    } else if (d->bars > 0 && section_role_is_theme(d->role) && d->bars != 4 && d->bars != 8) {
        return fail(reader, member_at(at, sizeof(at), path, "bars"),
                    "a theme-like section of %zu bars needs a construction; only 4 or 8 bars have none", d->bars);
    }
    /* cadence: a section ends in a perfect authentic, imperfect authentic or half cadence; the roles have their own */
    if (d->cadence != CADENCE_TYPE_PERFECT_AUTHENTIC && d->cadence != CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
        d->cadence != CADENCE_TYPE_HALF) {
        return fail(reader, member_at(at, sizeof(at), path, "cadence"),
                    "a section ends in a perfect authentic, imperfect authentic or half cadence");
    }
    if ((liquidation || retransition || transition) && d->cadence != CADENCE_TYPE_HALF) {
        return fail(reader, member_at(at, sizeof(at), path, "cadence"), "a %s ends in a half cadence",
                    liquidation ? "liquidation" : retransition ? "retransition" : "transition");
    }
    if (coda && d->cadence != CADENCE_TYPE_PERFECT_AUTHENTIC) {
        return fail(reader, member_at(at, sizeof(at), path, "cadence"), "a coda ends in a perfect authentic cadence");
    }
    /* keys name regions of the form */
    const SectionRegion *keys[2] = {&d->key_start, &d->key_end};
    const char *key_names[2] = {"start", "end"};
    for (int k = 0; k < 2; k++) {
        if (keys[k]->kind == SECTION_REGION_FORM && form_spec_region_find(form, keys[k]->form_region) < 0) {
            char keys_at[64];
            join(keys_at, sizeof(keys_at), path, "keys");
            return fail(reader, member_at(at, sizeof(at), keys_at, key_names[k]), "unknown region \"%s\"", keys[k]->form_region);
        }
    }
    if (transition && form_spec_region_of(form, &d->key_end) != form_spec_region_of(form, &d->key_start) && d->bars != 8) {
        return fail(reader, member_at(at, sizeof(at), path, "bars"), "a transition that modulates is 8 bars");
    }
    /* motive: as motive_relation_step_text writes it */
    if (d->motive[0] != '\0') {
        MotiveRelationStep step;
        if (!motive_relation_step_parse(d->motive, &step)) {
            return fail(reader, member_at(at, sizeof(at), path, "motive"), "not a motive relation: \"%s\"", d->motive);
        }
    }
    /* returns: an earlier section of the size, whose construction it keeps */
    if (d->returns[0] != '\0') {
        size_t source = form_spec_section_index(size, d->returns);
        if (source == FORM_SPEC_NO_RETURN || source >= index) {
            return fail(reader, member_at(at, sizeof(at), path, "returns"), "\"%s\" is not an earlier section of the size", d->returns);
        }
        if (!same_construction(d, &size->sections[source])) {
            return fail(reader, member_at(at, sizeof(at), path, "construction"), "a return keeps the construction of %s",
                        size->sections[source].name);
        }
    }
    return true;
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
            if (strcmp(size->sections[other].returns, size->sections[s].name) == 0) {
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

bool form_spec_has_role(const FormSpec *form, SectionRole role) {
    for (size_t i = 0; form && i < form->role_count; i++) {
        if (form->roles[i] == role) {
            return true;
        }
    }
    return false;
}

bool form_spec_role_in_loop(const FormSpec *form, SectionRole role) {
    for (size_t i = 0; form && i < form->loop_role_count; i++) {
        if (form->loop_roles[i] == role) {
            return true;
        }
    }
    return false;
}

static bool is_part_role(SectionRole role) {
    return role >= SECTION_ROLE_INTRO && role <= SECTION_ROLE_ENDING;
}

/*
 * A list of roles. With form NULL it is the form's own `roles`: each a part role (until P45), in the order intro, body,
 * turnaround, ending, none twice. With a form it is a list of roles the form has (the loop's, a cadence rule's).
 */
static bool read_role_list(Reader *reader, const cJSON *parent, const char *path, const char *key, const FormSpec *form,
                           SectionRole *out, size_t *count) {
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
        SectionRole role;
        join_index(item, sizeof(item), at, i);
        if (!cJSON_IsString(entry) || !entry->valuestring) {
            return fail(reader, item, "must be a role name");
        }
        if (!section_role_find(entry->valuestring, &role)) {
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
    if (!section_role_find(text, &out->role) || out->role != SECTION_ROLE_ENDING || !form_spec_has_role(form, out->role)) {
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
    if (!form_spec_has_role(form, SECTION_ROLE_BODY)) {
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
            SectionError section_error;
            if (!section_from_json(cJSON_GetArrayItem(sections, (int)s), SECTION_USE_FORM, NULL, 0, &size->sections[s],
                                   &section_error)) {
                char at_member[192];
                snprintf(at_member, sizeof(at_member), "%s%s", section_at, section_error.path);
                return fail(reader, at_member, "%s", section_error.message);
            }
            if (size->sections[s].cadence_name[0] == '\0') {
                const HarmonyCadenceKind *kind = harmony_cadence_kind_of(size->sections[s].cadence);
                snprintf(size->sections[s].cadence_name, sizeof(size->sections[s].cadence_name), "%s", kind ? kind->name : "");
            }
            if (!form_section_check(reader, form, size, &size->sections[s], s, section_at)) {
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
    if (cJSON_IsNumber(version) && version->valuedouble == 1) {
        return fail(reader, "/version", "form documents are version 2");
    }
    if (!whole(reader, version, "/version", 2, 2, &number)) {
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
