#include "plan/plan_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <common/json.h>
#include <model/pitch/pitch_arithmetic.h>
#include <theory/harmony/harmonic_function.h>
#include <theory/tonality/tonal_key.h>
#include "plan/plan_constraint.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "plan/section_json.h"
#include "plan/plan_constraint_kinds.h"
#include "plan/plan_internal.h"
#include "plan/plan_json_internal.h"
#include "plan/plan_common.h"

/* The constraints of a plan file: writing them, reading them (current and legacy forms), the key and section members, and the form record. */

/* A region's payload: a tonal region (a key), with the relation it is named by beside it (R-597); a span not yet resolved is only the relation. */
#define REGION_KIND_TONAL "tonal"

static const char *key_anchor_text(const PlanKeyRelation *relation) {
    return relation->anchor == PLAN_KEY_ANCHOR_SECTION ? relation->anchor_section : plan_json_KEY_ANCHOR_NAMES[relation->anchor];
}

bool plan_json_key_payload_json(cJSON *object, const SpelledKey *key, const PlanKeyRelation *relation) {
    if (relation && !relation->resolved) {
        return plan_json_add(object, "relation", cJSON_CreateString(relation->relation)) &&
               plan_json_add(object, "anchor", cJSON_CreateString(key_anchor_text(relation)));
    }
    return plan_json_add(object, "kind", cJSON_CreateString(REGION_KIND_TONAL)) && json_key_members(object, key) &&
           (!relation || (plan_json_add(object, "relation", cJSON_CreateString(relation->relation)) &&
                          plan_json_add(object, "anchor", cJSON_CreateString(key_anchor_text(relation)))));
}

static cJSON *constraint_json(const Plan *plan, const PlanConstraint *constraint) {
    const ConstraintKindRow *row = constraint_kind_row(constraint->kind);
    cJSON *object = row ? cJSON_CreateObject() : NULL;
    if (!object) {
        return NULL;
    }
    if (!plan_json_add(object, "kind", cJSON_CreateString(row->name)) || !row->write(object, plan, constraint)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *plan_json_node_constraints_json(const Plan *plan, NodeId id) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < plan_node_constraint_count(plan, id); i++) {
        if (!plan_json_append(array, constraint_json(plan, plan_node_constraint_at(plan, id, i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

cJSON *plan_json_span_constraints_json(const Plan *plan, LaneSpanId id) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < plan_lane_span_constraint_count(plan, id); i++) {
        if (!plan_json_append(array, constraint_json(plan, plan_lane_span_constraint_at(plan, id, i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

/* A return constraint's variation, by the name the one table holds. */
static bool read_return_variation(JsonReader *reader, const cJSON *object, const char *path,
                                  SectionReturnVariation *out) {
    const cJSON *item = plan_json_member(reader, object, path, "variation", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "variation");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_return_variation_find(item->valuestring, out)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
    }
    return true;
}

/* A return's setting (R-518): required for a setting return, refused for any other. */
/* A return as plan formats 1 to 4 wrote it: the section it returns by id, how it varies it, the setting and whether it crosses modes. */
typedef struct {
    NodeId section;
    SectionReturnVariation variation;
    PlanAccompanimentPattern setting;
    bool across_modes;
} LegacyReturn;

static bool read_return_setting(JsonReader *reader, const cJSON *object, const char *path, LegacyReturn *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "setting");
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "setting");
    if (out->variation != SECTION_RETURN_SETTING) {
        return item ? json_bad_format(reader, at, "only a setting return has a setting") : true;
    }
    int value;
    if (!item) {
        return json_bad_format(reader, at, "a setting return names its setting");
    }
    if (!json_read_enum_member(reader, object, path, "setting", JSON_ACCOMPANIMENT_NAMES, COUNT_OF(JSON_ACCOMPANIMENT_NAMES), &value)) {
        return false;
    }
    if (value < (int)PLAN_ACCOMPANIMENT_BLOCK || value > (int)PLAN_ACCOMPANIMENT_OOM_PAH) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", JSON_ACCOMPANIMENT_NAMES[value]);
    }
    out->setting = (PlanAccompanimentPattern)value;
    return true;
}

/* A theme pairing's beginning or ending by its plan name (the section tables'). */
static bool read_theme_half(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool beginning,
                            int *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    SectionBeginning found_beginning;
    SectionEnding found_ending;
    if (beginning ? !section_beginning_find(item->valuestring, &found_beginning) : !section_ending_find(item->valuestring, &found_ending)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
    }
    *out = beginning ? (int)found_beginning : (int)found_ending;
    return true;
}

/* An idea by the name the section tables give it; `at` is its path. */
static bool read_idea_name(JsonReader *reader, const cJSON *item, const char *at, int *out) {
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected an idea name");
    }
    SectionIdea idea;
    if (!section_idea_find(item->valuestring, &idea)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown idea '%.40s'", item->valuestring);
    }
    *out = (int)idea;
    return true;
}

/* A construction constraint's ideas, extensions and reason. */
static bool read_construction(JsonReader *reader, const cJSON *item, const char *path, SectionConstruction *out) {
    char at[PLAN_JSON_PATH_MAX];
    int value;
    long number;
    const cJSON *ideas = plan_json_member(reader, item, path, "ideas", true);
    if (!ideas) {
        return false;
    }
    json_child_path(at, path, "ideas");
    if (!cJSON_IsObject(ideas)) {
        return json_bad_format(reader, at, "expected an object of idea names and bars");
    }
    for (const cJSON *member = ideas->child; member; member = member->next) {
        char member_at[PLAN_JSON_PATH_MAX];
        SectionIdea idea;
        json_child_path(member_at, at, member->string);
        if (!section_idea_find(member->string, &idea)) {
            return json_fail(reader, PLAN_ERR_FORMAT, member_at, "unknown idea '%.40s'", member->string);
        }
        if (!json_read_long(reader, member, member_at, 0, 16, &number)) {
            return false;
        }
        out->idea_bars[idea] = (unsigned char)number;
    }
    const cJSON *extensions = plan_json_member(reader, item, path, "extensions", true);
    if (!extensions) {
        return false;
    }
    json_child_path(at, path, "extensions");
    if (!cJSON_IsArray(extensions)) {
        return json_bad_format(reader, at, "expected an array");
    }
    if ((size_t)cJSON_GetArraySize(extensions) > SECTION_CONSTRUCTION_EXTENSIONS) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "at most %d extensions", SECTION_CONSTRUCTION_EXTENSIONS);
    }
    size_t index = 0;
    for (const cJSON *extension = extensions->child; extension; extension = extension->next, index++) {
        char extension_at[PLAN_JSON_PATH_MAX];
        json_index_path(extension_at, at, index);
        if (!cJSON_IsObject(extension)) {
            return json_bad_format(reader, extension_at, "expected an object");
        }
        SectionExtension *target = &out->extensions[index];
        const cJSON *idea;
        const cJSON *varied;
        const char *kind_names[] = {section_extension_kind_name(SECTION_EXTENSION_REPEAT),
                                    section_extension_kind_name(SECTION_EXTENSION_STRETCH),
                                    section_extension_kind_name(SECTION_EXTENSION_ADDITION),
                                    section_extension_kind_name(SECTION_EXTENSION_EVADED)};
        if (!json_read_enum_member(reader, extension, extension_at, "kind", kind_names, COUNT_OF(kind_names), &value)) {
            return false;
        }
        target->kind = (SectionExtensionKind)value;
        if (!(idea = plan_json_member(reader, extension, extension_at, "idea", true))) {
            return false;
        }
        char idea_at[PLAN_JSON_PATH_MAX];
        json_child_path(idea_at, extension_at, "idea");
        if (!read_idea_name(reader, idea, idea_at, &value)) {
            return false;
        }
        target->idea = (unsigned char)value;
        if (!json_read_int_member(reader, extension, extension_at, "bars", 0, 4, &number)) {
            return false;
        }
        target->bars = (unsigned char)number;
        if (!(varied = plan_json_member(reader, extension, extension_at, "varied", true))) {
            return false;
        }
        if (!cJSON_IsBool(varied)) {
            char varied_at[PLAN_JSON_PATH_MAX];
            json_child_path(varied_at, extension_at, "varied");
            return json_bad_format(reader, varied_at, "expected true or false");
        }
        target->varied = cJSON_IsTrue(varied);
        out->extension_count = index + 1;
    }
    const char *reason_names[] = {section_construction_reason_name(SECTION_CONSTRUCTION_GIVEN),
                                  section_construction_reason_name(SECTION_CONSTRUCTION_FROM_MOTIVE),
                                  section_construction_reason_name(SECTION_CONSTRUCTION_FROM_LENGTH)};
    if (!json_read_enum_member(reader, item, path, "reason", reason_names, COUNT_OF(reason_names), &value)) {
        return false;
    }
    out->reason = (SectionConstructionReason)value;
    return true;
}

/* The optional across_modes of a reharmonized return (R-524); true on any other variation is refused. */
static bool read_return_across_modes(JsonReader *reader, const cJSON *object, const char *path, LegacyReturn *out) {
    if (!json_read_optional_bool(reader, object, path, "across_modes", &out->across_modes)) {
        return false;
    }
    if (out->across_modes && out->variation != SECTION_RETURN_REHARMONIZE) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "across_modes");
        return json_bad_format(reader, at, "only a reharmonized return crosses modes");
    }
    return true;
}

/*
 * Plan formats 1 and 2 name a key's mode, "major" or "minor", where format 3 names a material (R-597). The migration reads them as
 * the materials the snapshot holds for those words: the major as omt.material.major, the minor as omt.material.natural-minor. A
 * file of format 1 or 2 has no relation beside a key either: a key span reads back with none.
 */
static bool read_legacy_material(JsonReader *reader, const cJSON *object, const char *path, const PitchMaterial **out) {
    int mode;
    if (!json_read_enum_member(reader, object, path, "mode", plan_json_MODE_NAMES, COUNT_OF(plan_json_MODE_NAMES), &mode)) {
        return false;
    }
    *out = knowledge_snapshot_material(reader->knowledge, mode == 1 ? MATERIAL_ID_MINOR : MATERIAL_ID_MAJOR);
    if (!*out) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "mode");
        return json_bad_format(reader, at, "the knowledge snapshot has no material for this mode");
    }
    return true;
}

/*
 * Version 1 files (R-260): a key is "tonic_pitch_class" and "mode", with no spelling. The tonic is spelled as the library
 * spells the key on that pitch class (standard_tonality_key_tonic_of_pitch_class_material: the fewest accidentals in its
 * signature, sharps on a tie), which is how every plan of version 1 was written.
 */
static bool read_key_v1(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    const PitchMaterial *material;
    long pitch_class;
    PitchLetter letter;
    int accidental;
    if (!read_legacy_material(reader, object, path, &material) ||
        !json_read_int_member(reader, object, path, "tonic_pitch_class", 0, 11, &pitch_class)) {
        return false;
    }
    if (!standard_tonality_key_tonic_of_pitch_class_material((int)pitch_class, material, &letter, &accidental)) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "tonic_pitch_class");
        return json_bad_format(reader, at, "no key on this pitch class");
    }
    SpelledKey key = {NULL, letter, accidental, material};
    *out = key;
    return true;
}

/* A tonic member: a spelled name ("C", "F#", "Bb"). */
static bool read_tonic_member(JsonReader *reader, const cJSON *object, const char *path, PitchLetter *letter, int *accidental) {
    const cJSON *tonic = plan_json_member(reader, object, path, "tonic", true);
    if (!tonic) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "tonic");
    if (!cJSON_IsString(tonic) || !pitch_spelling_parse_name(tonic->valuestring, letter, accidental, NULL)) {
        return json_bad_format(reader, at, "expected a tonic like \"C\", \"F#\" or \"Bb\"");
    }
    return true;
}

/* A version 3 key: "tonic" and "material", the id of a material the Composer composes in. */
static bool read_key_v3(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    const cJSON *id = plan_json_member(reader, object, path, "material", true);
    if (!id) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "material");
    if (!cJSON_IsString(id)) {
        return json_bad_format(reader, at, "expected a material id like \"omt.material.major\"");
    }
    const PitchMaterial *material = knowledge_snapshot_material(reader->knowledge, id->valuestring);
    if (!material) {
        return json_bad_format(reader, at, "no material has this id");
    }
    if (reader->hooks && reader->hooks->material_supported && !reader->hooks->material_supported(material, reader->hooks->data)) {
        return json_bad_format(reader, at, "the Composer composes in major and natural minor keys only");
    }
    PitchLetter letter;
    int accidental;
    if (!read_tonic_member(reader, object, path, &letter, &accidental)) {
        return false;
    }
    SpelledKey key = {NULL, letter, accidental, material};
    *out = key;
    return true;
}

/* A version 2 key: "tonic" and "mode". */
static bool read_key_v2(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    const PitchMaterial *material;
    PitchLetter letter;
    int accidental;
    if (!read_legacy_material(reader, object, path, &material) || !read_tonic_member(reader, object, path, &letter, &accidental)) {
        return false;
    }
    SpelledKey key = {NULL, letter, accidental, material};
    *out = key;
    return true;
}

/* A key's members, by the version of the file: see read_key_v1, read_key_v2 and read_key_v3. */
bool json_read_key_members(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    switch (reader->version) {
    case 1:
        return read_key_v1(reader, object, path, out);
    case 2:
        return read_key_v2(reader, object, path, out);
    default:
        return read_key_v3(reader, object, path, out);
    }
}

/* A key span's optional "relation" and "anchor" beside its key (version 3): both or neither. *relation is left empty for neither. */
static bool read_key_relation(JsonReader *reader, const cJSON *object, const char *path, PlanKeyRelation *relation) {
    memset(relation, 0, sizeof(*relation));
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(object, "relation");
    const cJSON *anchor = cJSON_GetObjectItemCaseSensitive(object, "anchor");
    if (!id && !anchor) {
        return true;
    }
    char at[PLAN_JSON_PATH_MAX];
    if (!id || !anchor) {
        json_child_path(at, path, id ? "anchor" : "relation");
        return json_bad_format(reader, at, "a key names its relation and its anchor together");
    }
    json_child_path(at, path, "relation");
    if (!cJSON_IsString(id) || id->valuestring[0] == '\0' || strlen(id->valuestring) >= PLAN_KEY_RELATION_MAX) {
        return json_bad_format(reader, at, "expected a relation id like \"omt.relation.dominant\"");
    }
    json_child_path(at, path, "anchor");
    if (!cJSON_IsString(anchor) || anchor->valuestring[0] == '\0' || strlen(anchor->valuestring) >= PLAN_KEY_ANCHOR_SECTION_MAX) {
        return json_bad_format(reader, at, "expected \"home\", \"previous\" or the name of an earlier section");
    }
    /* "home" and "previous" are the plan's own; any other name is a section (format 5). */
    int found = PLAN_KEY_ANCHOR_SECTION;
    for (int i = 0; i < (int)COUNT_OF(plan_json_KEY_ANCHOR_NAMES); i++) {
        if (strcmp(anchor->valuestring, plan_json_KEY_ANCHOR_NAMES[i]) == 0) {
            found = i;
        }
    }
    if (found == PLAN_KEY_ANCHOR_SECTION && reader->version < 5) {
        return json_bad_format(reader, at, "expected \"home\" or \"previous\"");
    }
    snprintf(relation->relation, PLAN_KEY_RELATION_MAX, "%s", id->valuestring);
    relation->anchor = (PlanKeyAnchor)found;
    if (found == PLAN_KEY_ANCHOR_SECTION) {
        snprintf(relation->anchor_section, PLAN_KEY_ANCHOR_SECTION_MAX, "%s", anchor->valuestring);
    }
    relation->resolved = true;
    return true;
}

/*
 * The payload of a key span: its key and, in version 3, the relation it is named by. A version 3 span with a "relation" and an
 * "anchor" and neither a "tonic" nor a "material" is named only by them: its key is the zero key until plan_resolve_key_spans.
 */
bool plan_json_read_key_span(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *key, PlanKeyRelation *relation) {
    memset(relation, 0, sizeof(*relation));
    /* Format 6: a region names its kind; only the tonal one (a key) is built. */
    const cJSON *kind = reader->version >= 6 ? cJSON_GetObjectItemCaseSensitive(object, "kind") : NULL;
    if (kind) {
        char kind_path[PLAN_JSON_PATH_MAX];
        json_child_path(kind_path, path, "kind");
        if (!cJSON_IsString(kind)) {
            return json_bad_format(reader, kind_path, "expected a string");
        }
        if (strcmp(kind->valuestring, REGION_KIND_TONAL) != 0) {
            return json_fail(reader, PLAN_ERR_FORMAT, kind_path, "region kind '%.40s' is not built until P69", kind->valuestring);
        }
    }
    if (reader->version >= 3 && cJSON_GetObjectItemCaseSensitive(object, "relation") &&
        !cJSON_GetObjectItemCaseSensitive(object, "tonic") && !cJSON_GetObjectItemCaseSensitive(object, "material")) {
        SpelledKey zero;
        memset(&zero, 0, sizeof(zero));
        *key = zero;
        if (!read_key_relation(reader, object, path, relation)) {
            return false;
        }
        relation->resolved = false;
        return true;
    }
    return json_read_key_members(reader, object, path, key) &&
           (reader->version < 3 || read_key_relation(reader, object, path, relation));
}

/* A chord's "function": a harmonic_function_name, absent for none. Version 1 always wrote it, "unknown" for none. */
bool plan_json_read_function(JsonReader *reader, const cJSON *object, const char *path, PlanHarmony *harmony) {
    harmony->has_function = false;
    harmony->function = HARMONIC_FUNCTION_TONIC;
    const cJSON *item = plan_json_member(reader, object, path, "function", reader->version == 1);
    if (!item) {
        return !reader->json.failed;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "function");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (reader->version == 1 && strcmp(item->valuestring, LEGACY_FUNCTION_NONE) == 0) {
        return true;
    }
    for (int function = 0; function < (int)HARMONIC_FUNCTION_COUNT; function++) {
        if (strcmp(item->valuestring, harmonic_function_name((HarmonicFunction)function)) == 0) {
            harmony->has_function = true;
            harmony->function = (HarmonicFunction)function;
            return true;
        }
    }
    return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
}

/* A chord's optional "borrowed_from" (format 6): the id of the material a borrowed chord is taken from; absent for none. */
bool plan_json_read_borrowed_from(JsonReader *reader, const cJSON *object, const char *path, PlanHarmony *harmony) {
    harmony->borrowed_from = NULL;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "borrowed_from");
    if (!item) {
        return true;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "borrowed_from");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a material id like \"omt.material.natural-minor\"");
    }
    harmony->borrowed_from = knowledge_snapshot_material(reader->knowledge, item->valuestring);
    if (!harmony->borrowed_from) {
        return json_bad_format(reader, at, "no material has this id");
    }
    return true;
}

/* A figure constraint: every member required; change_at is a bar from 1 or null for none. */
static bool read_figure(JsonReader *reader, const cJSON *item, const char *path, SectionFigure *out) {
    const cJSON *kind = plan_json_member(reader, item, path, "figure", true);
    const cJSON *rhythm = kind ? plan_json_member(reader, item, path, "rhythm", true) : NULL;
    const cJSON *harmony = rhythm ? plan_json_member(reader, item, path, "harmony", true) : NULL;
    const cJSON *change_at = harmony ? plan_json_member(reader, item, path, "change_at", true) : NULL;
    const cJSON *climax = change_at ? plan_json_member(reader, item, path, "climax", true) : NULL;
    if (!climax) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "figure");
    if (!cJSON_IsString(kind)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_figure_kind_find(kind->valuestring, &out->kind)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", kind->valuestring);
    }
    json_child_path(at, path, "rhythm");
    if (!cJSON_IsString(rhythm)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_figure_rhythm_find(rhythm->valuestring, &out->rhythm)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", rhythm->valuestring);
    }
    json_child_path(at, path, "harmony");
    if (!cJSON_IsString(harmony)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (strcmp(harmony->valuestring, "kept") == 0) {
        out->reharmonize = false;
    } else if (strcmp(harmony->valuestring, "reharmonized") == 0) {
        out->reharmonize = true;
    } else {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", harmony->valuestring);
    }
    json_child_path(at, path, "change_at");
    out->change_at = 0;
    if (!cJSON_IsNull(change_at)) {
        long value;
        if (!json_read_long(reader, change_at, at, 1, TIME_LIMIT, &value)) {
            return false;
        }
        out->change_at = (unsigned)value;
    }
    json_child_path(at, path, "climax");
    if (!cJSON_IsBool(climax)) {
        return json_bad_format(reader, at, "expected true or false");
    }
    out->climax = cJSON_IsTrue(climax);
    return true;
}

/* A section function constraint: function, independent and lyric, all required. */
static bool read_section_function(JsonReader *reader, const cJSON *item, const char *path, SectionFunction *out) {
    const cJSON *function = plan_json_member(reader, item, path, "function", true);
    const cJSON *independent = function ? plan_json_member(reader, item, path, "independent", true) : NULL;
    const cJSON *lyric = independent ? plan_json_member(reader, item, path, "lyric", true) : NULL;
    if (!lyric) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "function");
    if (!cJSON_IsString(function)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_function_kind_find(function->valuestring, &out->function)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", function->valuestring);
    }
    json_child_path(at, path, "independent");
    if (!cJSON_IsBool(independent)) {
        return json_bad_format(reader, at, "expected true or false");
    }
    out->independent = cJSON_IsTrue(independent);
    json_child_path(at, path, "lyric");
    if (!cJSON_IsBool(lyric)) {
        return json_bad_format(reader, at, "expected true or false");
    }
    out->lyric = cJSON_IsTrue(lyric);
    return true;
}

/*
 * The constraints plan formats 1 to 4 wrote on a section node, which are the members of its description from format 5 on (R-607):
 * archetype (derived from the pairing), material, theme_pairing, return, layers, section_letter, piece_part, free_phrase, hold,
 * seam, pedal, construction, connective, liquidation, figure and section_function; a section's cadence constraints are its cadence
 * and, second, its first cadence. section_from_old_constraints is the one place that reads them: apply_old_constraint takes
 * one into the description, and the reader sets the description once the node's constraints are read.
 */
static const char *const LEGACY_KINDS[] = {"archetype", "material", "theme_pairing", "return", "layers", "section_letter",
                                              "piece_part", "free_phrase", "hold", "seam", "pedal", "construction", "connective",
                                              "liquidation", "figure", "section_function"};

#define LEGACY_COUNT COUNT_OF(LEGACY_KINDS)

/* The index of the kind a constraint object names in LEGACY_KINDS, or -1 for any other kind. */
static int old_constraint_kind(const cJSON *item) {
    const cJSON *kind = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "kind") : NULL;
    for (size_t i = 0; cJSON_IsString(kind) && i < LEGACY_COUNT; i++) {
        if (strcmp(kind->valuestring, LEGACY_KINDS[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}


static bool apply_old_constraint(JsonReader *reader, const Plan *plan, const cJSON *item, const char *path, int legacy,
                                 SectionDescription *d) {
    int value;
    long number;
    char at[PLAN_JSON_PATH_MAX];
    switch (legacy) {
    case 0: /* archetype: the pairing says it */
        return json_read_enum_member(reader, item, path, "type", plan_json_ARCHETYPE_NAMES, COUNT_OF(plan_json_ARCHETYPE_NAMES), &value);
    case 1: /* material */
        d->has_role = true;
        d->role = SECTION_ROLE_MATERIAL;
        return true;
    case 2: { /* theme_pairing */
        int ending;
        if (!read_theme_half(reader, item, path, "beginning", true, &value) ||
            !read_theme_half(reader, item, path, "ending", false, &ending)) {
            return false;
        }
        d->has_pairing = true;
        d->pairing.beginning = (SectionBeginning)value;
        d->pairing.ending = (SectionEnding)ending;
        return true;
    }
    case 3: { /* return */
        LegacyReturn ret;
        memset(&ret, 0, sizeof(ret));
        if (!json_read_int_member(reader, item, path, "section", 1, TIME_LIMIT, &number) ||
            !read_return_variation(reader, item, path, &ret.variation)) {
            return false;
        }
        ret.section = (NodeId)number;
        if (!read_return_setting(reader, item, path, &ret) || !read_return_across_modes(reader, item, path, &ret)) {
            return false;
        }
        if (!plan_has_node(plan, ret.section) || plan_node_kind(plan, ret.section) != PLAN_NODE_SECTION) {
            return json_fail(reader, PLAN_ERR_FORMAT, path, "constraint %s", plan_json_result_name(PLAN_ERR_BAD_PAYLOAD));
        }
        const char *name = plan_node_name(plan, ret.section);
        snprintf(d->returns, sizeof(d->returns), "%s", name ? name : "");
        d->variation = ret.variation;
        d->setting = (SectionSetting)ret.setting;
        d->mode = ret.across_modes ? SECTION_MODE_HOME : SECTION_MODE_NONE;
        return true;
    }
    case 4: { /* layers */
        const cJSON *lanes = plan_json_member(reader, item, path, "lanes", true);
        if (!lanes) {
            return false;
        }
        char lanes_path[PLAN_JSON_PATH_MAX];
        json_child_path(lanes_path, path, "lanes");
        if (!cJSON_IsArray(lanes)) {
            return json_bad_format(reader, lanes_path, "expected an array of lane names");
        }
        size_t index = 0;
        const cJSON *lane;
        d->layers = 0;
        cJSON_ArrayForEach(lane, lanes) {
            json_index_path(at, lanes_path, index++);
            if (!json_read_enum(reader, lane, at, plan_lane_names, PLAN_LANE_NAME_COUNT, &value)) {
                return false;
            }
            d->layers |= PLAN_LAYER_BIT(value);
        }
        return d->layers != 0 ? true : json_bad_format(reader, lanes_path, "a layer sounds at least one lane");
    }
    case 5: { /* section_letter */
        const cJSON *letter = plan_json_member(reader, item, path, "letter", true);
        if (!letter) {
            return false;
        }
        json_child_path(at, path, "letter");
        if (!cJSON_IsString(letter) || letter->valuestring[0] == '\0' || strlen(letter->valuestring) >= SECTION_LETTER_MAX) {
            return json_bad_format(reader, at, "expected one to three characters");
        }
        snprintf(d->letter, sizeof(d->letter), "%s", letter->valuestring);
        return true;
    }
    case 6: { /* piece_part */
        const cJSON *id = NULL;
        if (!json_read_enum_member(reader, item, path, "part", plan_json_PIECE_PART_NAMES, COUNT_OF(plan_json_PIECE_PART_NAMES), &value) ||
            !(id = plan_json_member(reader, item, path, "id", true))) {
            return false;
        }
        json_child_path(at, path, "id");
        if (!cJSON_IsString(id) || id->valuestring[0] == '\0' || strlen(id->valuestring) >= SECTION_ID_MAX) {
            return json_bad_format(reader, at, "expected one to fifteen characters");
        }
        d->has_role = true;
        d->role = (SectionRole)(SECTION_ROLE_INTRO + value);
        snprintf(d->id, sizeof(d->id), "%s", id->valuestring);
        return true;
    }
    case 7: /* free_phrase */
        d->free_phrase = true;
        return true;
    case 8: /* hold */
        d->leads_in = true;
        return true;
    case 9: /* seam */
        d->has_role = true;
        d->role = SECTION_ROLE_SEAM;
        return true;
    case 10: /* pedal */
        if (!json_read_int_member(reader, item, path, "degree", 1, 7, &number)) {
            return false;
        }
        if (number != 5) {
            return json_fail(reader, PLAN_ERR_FORMAT, path, "constraint %s", plan_json_result_name(PLAN_ERR_BAD_PAYLOAD));
        }
        d->pedal = 5;
        return true;
    case 11: /* construction */
        d->has_construction = true;
        return read_construction(reader, item, path, &d->construction);
    case 12: { /* connective */
        const cJSON *written = plan_json_member(reader, item, path, "written", true);
        if (!written) {
            return false;
        }
        if (!cJSON_IsBool(written)) {
            json_child_path(at, path, "written");
            return json_bad_format(reader, at, "expected true or false");
        }
        d->connective = SECTION_CONNECTIVE_ALWAYS; /* the plan holds that it was asked for; sometimes was drawn at build */
        d->connective_written = cJSON_IsTrue(written);
        return true;
    }
    case 13: /* liquidation */
        d->has_role = true;
        d->role = SECTION_ROLE_LIQUIDATION;
        return true;
    case 14: /* figure */
        d->has_figure = true;
        return read_figure(reader, item, path, &d->figure);
    case 15: /* section_function */
        d->has_function = true;
        return read_section_function(reader, item, path, &d->function);
    }
    return false;
}

static bool read_constraint(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, path, "expected an object");
    }
    memset(out, 0, sizeof(*out));
    const cJSON *named = plan_json_member(reader, item, path, "kind", true);
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "kind");
    if (!named) {
        return false;
    }
    if (!cJSON_IsString(named)) {
        return json_bad_format(reader, at, "expected a string");
    }
    /* Formats before 6 called the constraint on a region "key". */
    const char *kind_name = reader->version < 6 && strcmp(named->valuestring, "key") == 0 ? "region" : named->valuestring;
    const ConstraintKindRow *row = constraint_kind_named(kind_name);
    if (!row) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", named->valuestring);
    }
    out->kind = row->kind;
    return row->read(reader, item, path, out);
}

const char *plan_json_result_name(PlanResult result) {
    switch (result) {
    case PLAN_ERR_BAD_TIME:
        return "invalid time";
    case PLAN_ERR_BAD_SPAN:
        return "span is empty or not inside its parent";
    case PLAN_ERR_BAD_KIND:
        return "this kind cannot go here";
    case PLAN_ERR_ROOT_EXISTS:
        return "a plan has one root";
    case PLAN_ERR_NOT_FOUND:
        return "refers to a missing node";
    case PLAN_ERR_LANE_OVERLAP:
        return "overlaps another span in the same lane";
    case PLAN_ERR_BAD_PAYLOAD:
        return "value is not valid here";
    case PLAN_ERR_GIVEN_LOCKED:
        return "the item is Given";
    default:
        return "could not be added";
    }
}

PlanResult plan_json_add_node_constraint(Plan *plan, size_t id, const PlanConstraint *constraint) {
    return plan_node_add_constraint(plan, (NodeId)id, constraint);
}

PlanResult plan_json_add_span_constraint(Plan *plan, size_t id, const PlanConstraint *constraint) {
    return plan_lane_span_add_constraint(plan, (LaneSpanId)id, constraint);
}

bool plan_json_read_constraints(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path,
                             size_t id, AddConstraintFn add_constraint) {
    const cJSON *list = plan_json_member(reader, composer, composer_path, "constraints", true);
    if (!list) {
        return false;
    }
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, composer_path, "constraints");
    if (!cJSON_IsArray(list)) {
        return json_bad_format(reader, list_path, "expected an array");
    }
    /* Formats 1 to 4 wrote a section's attributes as constraints: they are read into its description. */
    const bool old_section = add_constraint == plan_json_add_node_constraint && reader->version < 5 &&
                             plan_node_kind(plan, (NodeId)id) == PLAN_NODE_SECTION;
    SectionDescription description;
    section_description_init(&description);
    size_t cadences = 0;
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (old_section) {
            const cJSON *kind = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "kind") : NULL;
            int legacy = old_constraint_kind(item);
            if (legacy >= 0) {
                if (!apply_old_constraint(reader, plan, item, at, legacy, &description)) {
                    return false;
                }
                continue;
            }
            if (cJSON_IsString(kind) && strcmp(kind->valuestring, "cadence") == 0) {
                PlanConstraint cadence;
                if (!read_constraint(reader, item, at, &cadence)) {
                    return false;
                }
                /* The first is the section's, a second the weak one an antecedent ends in. */
                if (cadences == 0) {
                    description.has_cadence = true;
                    description.cadence = cadence.params.cadence;
                } else if (cadences == 1) {
                    description.has_first_cadence = true;
                    description.first_cadence = cadence.params.cadence;
                }
                cadences++;
                continue;
            }
        }
        PlanConstraint constraint;
        if (!read_constraint(reader, item, at, &constraint)) {
            return false;
        }
        if (add_constraint == plan_json_add_node_constraint && constraint.kind == PLAN_CONSTRAINT_MOTIVE_TONES) {
            /* The labels are one to a note of the node's melody span. */
            LaneSpanId spans[16];
            size_t found = plan_node_lane_spans(plan, (NodeId)id, spans, 16);
            for (size_t k = 0; k < found && k < 16; k++) {
                size_t notes = plan_lane_span_payload(plan, spans[k])->notes.note_count;
                if (plan_lane_span_lane(plan, spans[k]) == PLAN_LANE_MELODY && notes != constraint.params.motive_tones.label_count) {
                    return json_fail(reader, PLAN_ERR_FORMAT, at, "motive_tones on %zu: %zu labels for %zu notes", id,
                                     constraint.params.motive_tones.label_count, notes);
                }
            }
        }
        /* The melody constraints (R-746) go on a melody span, below its length, one to an offset; one climax in the plan. */
        const char *refusal = constraint_melody_refusal(
            plan, add_constraint == plan_json_add_span_constraint ? (LaneSpanId)id : PLAN_LANE_SPAN_NONE, &constraint);
        if (refusal) {
            return json_fail(reader, PLAN_ERR_FORMAT, at, "%s", refusal);
        }
        PlanResult result = add_constraint(plan, id, &constraint);
        if (result == PLAN_ERR_ALLOC) {
            return json_fail(reader, PLAN_ERR_ALLOC, at, "out of memory");
        }
        if (result != PLAN_OK) {
            return json_fail(reader, PLAN_ERR_FORMAT, at, "constraint %s", plan_json_result_name(result));
        }
    }
    if (old_section) {
        PlanResult result = plan_node_set_section(plan, (NodeId)id, &description);
        if (result != PLAN_OK) {
            return json_fail(reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, list_path, "section %s",
                             plan_json_result_name(result));
        }
    }
    return true;
}

/* Format 5: a section node's "section" member is its description. */
bool plan_json_read_section_member(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path, size_t id) {
    const cJSON *item = plan_json_member(reader, composer, composer_path, "section", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, composer_path, "section");
    SectionDescription description;
    SectionError error;
    if (!section_from_json(item, SECTION_USE_PLAN, NULL, 0, &description, &error)) {
        char member[PLAN_JSON_PATH_MAX + 128];
        snprintf(member, sizeof(member), "%s%s", at, error.path);
        return json_fail(reader, PLAN_ERR_FORMAT, member, "%s", error.message);
    }
    PlanResult result = plan_node_set_section(plan, (NodeId)id, &description);
    if (result != PLAN_OK) {
        return json_fail(reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, at, "section %s", plan_json_result_name(result));
    }
    return true;
}

/* Format 5: the form node's optional "form" member is the record of the form the plan was built from. */
bool plan_json_read_form_record(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(composer, "form");
    if (!item) {
        return true;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, composer_path, "form");
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, at, "expected an object");
    }
    PlanFormRecord record;
    memset(&record, 0, sizeof(record));
    const cJSON *name = plan_json_member(reader, item, at, "name", true);
    const cJSON *hash = name ? plan_json_member(reader, item, at, "hash", true) : NULL;
    const cJSON *checks = hash ? plan_json_member(reader, item, at, "checks", true) : NULL;
    const cJSON *closing = checks ? plan_json_member(reader, item, at, "closing", true) : NULL;
    long version;
    char member[PLAN_JSON_PATH_MAX];
    if (!closing || !json_read_int_member(reader, item, at, "version", 1, 1000, &version)) {
        return false;
    }
    json_child_path(member, at, "name");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0' || strlen(name->valuestring) >= PLAN_FORM_NAME_MAX) {
        return json_bad_format(reader, member, "expected a form name of 1 to 31 characters");
    }
    json_child_path(member, at, "hash");
    if (!cJSON_IsString(hash) || strlen(hash->valuestring) > PLAN_FORM_HASH_LEN) {
        return json_bad_format(reader, member, "expected a hash of 16 lowercase hexadecimal digits or none");
    }
    json_child_path(member, at, "closing");
    if (!cJSON_IsBool(closing)) {
        return json_bad_format(reader, member, "expected true or false");
    }
    json_child_path(member, at, "checks");
    if (!cJSON_IsArray(checks)) {
        return json_bad_format(reader, member, "expected an array of check names");
    }
    size_t index = 0;
    const cJSON *check;
    cJSON_ArrayForEach(check, checks) {
        char entry[PLAN_JSON_PATH_MAX];
        int bit;
        json_index_path(entry, member, index++);
        if (!json_read_enum(reader, check, entry, plan_json_FORM_CHECK_NAMES, COUNT_OF(plan_json_FORM_CHECK_NAMES), &bit)) {
            return false;
        }
        if ((record.checks & (1u << bit)) != 0) {
            return json_bad_format(reader, entry, "a check is named twice");
        }
        record.checks |= 1u << bit;
    }
    snprintf(record.name, sizeof(record.name), "%s", name->valuestring);
    snprintf(record.hash, sizeof(record.hash), "%s", hash->valuestring);
    record.version = (unsigned)version;
    record.closing = cJSON_IsTrue(closing);
    PlanResult result = plan_set_form_record(plan, &record);
    if (result != PLAN_OK) {
        return json_fail(reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, at, "form %s", plan_json_result_name(result));
    }
    return true;
}
