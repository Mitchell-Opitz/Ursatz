#include "realization/realization_json.h"
#include "realization/realization_json_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <common/hash.h>
#include <common/json.h>
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/time/meter_map.h"
#include "model/time/note_value.h"
#include "model/time/tempo_map.h"
#include "model/time/time_signature.h"

/* ------------------------------------------------------------------------------------------------ reading */

typedef struct {
    CommonJsonReader json;
    const KnowledgeSnapshot *knowledge;
} Reader;

/* What the group's own check said, as text at the member's path. */
static bool group_refused(Reader *r, RealizationResult result, const char *path, const char *what) {
    const char *reason = "the value is refused";
    switch (result) {
    case REALIZATION_ERR_BAD_ID:
        reason = "not a valid id";
        break;
    case REALIZATION_ERR_DUPLICATE_ID:
        reason = "the id is used twice";
        break;
    case REALIZATION_ERR_BAD_VALUE:
        reason = "a value is out of range";
        break;
    case REALIZATION_ERR_UNKNOWN_REFERENCE:
        reason = "it names something that is not there";
        break;
    case REALIZATION_ERR_NOT_BUILT:
        reason = "it is not built";
        break;
    case REALIZATION_ERR_ALLOC:
        return common_json_refuse(&r->json, result, path, "out of memory");
    default:
        break;
    }
    return common_json_refuse(&r->json, result, path, "%s: %s", what, reason);
}

static bool read_int_member(Reader *r, const cJSON *object, const char *path, const char *key, long long min, long long max, int *out) {
    const cJSON *item = common_json_require(&r->json, object, path, key);
    char at[REALIZATION_JSON_PATH_MAX];
    long long value;
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, key);
    if (!item || !common_json_read_whole(&r->json, item, at, min, max, &value)) {
        return false;
    }
    *out = (int)value;
    return true;
}

/* A text member; *out is NULL for a JSON null when allow_null. */
static bool read_text_member(Reader *r, const cJSON *object, const char *path, const char *key, bool allow_null, const char **out) {
    const cJSON *item = common_json_require(&r->json, object, path, key);
    if (!item) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, key);
    if (allow_null && cJSON_IsNull(item)) {
        *out = NULL;
        return true;
    }
    if (!cJSON_IsString(item) || !item->valuestring) {
        return common_json_bad(&r->json, at, allow_null ? "expected text or null" : "expected text");
    }
    *out = item->valuestring;
    return true;
}

static bool read_rational(Reader *r, const cJSON *item, const char *path, Rational *out) {
    long long num;
    long long den;
    return common_json_read_time(&r->json, item, path, 1000000000LL, "expected a time {\"num\", \"den\"}", &num, &den) &&
           (rational_make(num, den, out) == RATIONAL_OK || common_json_bad(&r->json, path, "not a time"));
}

static bool read_time_member(Reader *r, const cJSON *object, const char *path, const char *key, bool allow_null, bool *present, MusicalTime *out) {
    const cJSON *item = common_json_require(&r->json, object, path, key);
    if (!item) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, key);
    if (allow_null && cJSON_IsNull(item)) {
        *present = false;
        return true;
    }
    Rational value;
    if (!read_rational(r, item, at, &value)) {
        return false;
    }
    if (present) {
        *present = true;
    }
    if (value.num < 0) {
        return common_json_bad(&r->json, at, "a time is not below 0");
    }
    *out = musical_time_of(value);
    return true;
}

static bool read_duration_member(Reader *r, const cJSON *object, const char *path, const char *key, Duration *out) {
    const cJSON *item = common_json_require(&r->json, object, path, key);
    char at[REALIZATION_JSON_PATH_MAX];
    Rational value;
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, key);
    if (!item || !read_rational(r, item, at, &value)) {
        return false;
    }
    return duration_of(value, out) == DURATION_OK || common_json_bad(&r->json, at, "a duration is not below 0");
}

static bool read_name(Reader *r, const cJSON *object, const char *path, const char *key, const char *const *names, size_t count, int *out) {
    const char *text;
    if (!read_text_member(r, object, path, key, false, &text)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(text, names[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    char at[REALIZATION_JSON_PATH_MAX];
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, key);
    return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, at, "unknown name '%.40s'", text);
}

static bool read_spelling_member(Reader *r, const cJSON *object, const char *path, bool *spelled, PitchLetter *letter, int *accidental) {
    const char *text;
    if (!read_text_member(r, object, path, "spelling", true, &text)) {
        return false;
    }
    *spelled = text != NULL;
    if (text && !pitch_spelling_parse_name(text, letter, accidental, NULL)) {
        char at[REALIZATION_JSON_PATH_MAX];
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "spelling");
        return common_json_bad(&r->json, at, "expected a note name like \"F#\" or \"Bb\"");
    }
    return true;
}

static bool read_derivation_member(Reader *r, const cJSON *object, const char *path, NoteDerivation *out) {
    memset(out, 0, sizeof(*out));
    const cJSON *item = common_json_require(&r->json, object, path, "derivation");
    if (!item) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "derivation");
    if (cJSON_IsNull(item)) {
        return true;
    }
    if (!common_json_expect_object(&r->json, item, at)) {
        return false;
    }
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *sources = common_json_require_array(&r->json, item, at, "sources", list_at, REALIZATION_JSON_PATH_MAX);
    if (!sources) {
        return false;
    }
    size_t index = 0;
    const cJSON *source;
    cJSON_ArrayForEach(source, sources) {
        char source_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(source_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!cJSON_IsString(source) || !source->valuestring) {
            return common_json_bad(&r->json, source_at, "expected a note id");
        }
        RealizationResult added = note_derivation_add_source(out, source->valuestring);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, source_at, "source");
        }
    }
    const char *operation;
    const char *run;
    if (!read_text_member(r, item, at, "operation", false, &operation) || !read_text_member(r, item, at, "run", false, &run)) {
        return false;
    }
    if (operation[0] != '\0' && note_derivation_set_operation(out, operation) != REALIZATION_OK) {
        common_json_child_path(list_at, REALIZATION_JSON_PATH_MAX, at, "operation");
        return common_json_bad(&r->json, list_at, "expected lowercase words joined by hyphens, at most 31 characters");
    }
    if (run[0] != '\0' && note_derivation_set_run(out, run) != REALIZATION_OK) {
        common_json_child_path(list_at, REALIZATION_JSON_PATH_MAX, at, "run");
        return common_json_bad(&r->json, list_at, "expected printable text of at most 63 characters");
    }
    return true;
}

static bool copy_id(Reader *r, const char *text, const char *path, char out[REALIZATION_ID_MAX]) {
    if (!realization_id_valid(text)) {
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_ID, path, "not a valid id: '%.40s'", text);
    }
    snprintf(out, REALIZATION_ID_MAX, "%s", text);
    return true;
}

static bool read_id_member(Reader *r, const cJSON *object, const char *path, const char *key, char out[REALIZATION_ID_MAX]) {
    const char *text;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_text_member(r, object, path, key, false, &text)) {
        return false;
    }
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, key);
    return copy_id(r, text, at, out);
}

/* "N/M" with M a power of two from 1 to 16. */
static bool parse_signature(const char *text, unsigned *top, unsigned *bottom) {
    char tail;
    return sscanf(text, "%u/%u%c", top, bottom, &tail) == 2 && *top >= 1 && *top <= TIME_SIGNATURE_MAX_TOP_NUMBER &&
           (*bottom == 1 || *bottom == 2 || *bottom == 4 || *bottom == 8 || *bottom == 16);
}

static bool read_meter_map(Reader *r, const cJSON *object, const char *path, const char *key, const char *id_text, MeterMap **out) {
    *out = NULL;
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *list = common_json_require_array(&r->json, object, path, key, list_at, REALIZATION_JSON_PATH_MAX);
    size_t count = list ? (size_t)cJSON_GetArraySize(list) : 0;
    if (!list) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    TimeSignature **signatures = calloc(count, sizeof(TimeSignature *));
    MeterMapChangePoint *points = calloc(count, sizeof(MeterMapChangePoint));
    bool ok = signatures && points;
    if (!ok) {
        common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, list_at, "out of memory");
    }
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(at, REALIZATION_JSON_PATH_MAX, list_at, index);
        const char *text;
        unsigned top;
        unsigned bottom;
        if (!ok || !common_json_expect_object(&r->json, item, at) || !read_time_member(r, item, at, "position", false, NULL, &points[index].start) ||
            !read_text_member(r, item, at, "time_signature", false, &text)) {
            ok = false;
            break;
        }
        char sig_at[REALIZATION_JSON_PATH_MAX];
        common_json_child_path(sig_at, REALIZATION_JSON_PATH_MAX, at, "time_signature");
        if (!parse_signature(text, &top, &bottom)) {
            ok = common_json_bad(&r->json, sig_at, "expected a time signature like \"3/4\"");
            break;
        }
        int exponent = 0;
        while ((1u << exponent) < bottom) {
            exponent++;
        }
        NoteValue *unit = NULL;
        TimeSignatureResult made;
        if (note_value_create(exponent, &unit) != NOTE_VALUE_OK) {
            ok = common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, sig_at, "out of memory");
            break;
        }
        const cJSON *groups = cJSON_GetObjectItemCaseSensitive(item, "groups");
        if (groups && !cJSON_IsNull(groups)) {
            unsigned values[TIME_SIGNATURE_MAX_GROUPS];
            size_t group_count = 0;
            const cJSON *group;
            char groups_at[REALIZATION_JSON_PATH_MAX];
            common_json_child_path(groups_at, REALIZATION_JSON_PATH_MAX, at, "groups");
            if (!cJSON_IsArray(groups) || cJSON_GetArraySize(groups) < 1 || (size_t)cJSON_GetArraySize(groups) > TIME_SIGNATURE_MAX_GROUPS) {
                note_value_destroy(unit);
                ok = common_json_bad(&r->json, groups_at, "expected 1 to 16 groups");
                break;
            }
            cJSON_ArrayForEach(group, groups) {
                long long value;
                char group_at[REALIZATION_JSON_PATH_MAX];
                common_json_index_path(group_at, REALIZATION_JSON_PATH_MAX, groups_at, group_count);
                if (!common_json_read_whole(&r->json, group, group_at, 1, TIME_SIGNATURE_MAX_GROUP_UNITS, &value)) {
                    ok = false;
                    break;
                }
                values[group_count++] = (unsigned)value;
            }
            if (!ok) {
                note_value_destroy(unit);
                break;
            }
            made = time_signature_create_grouped(values, group_count, unit, &signatures[index]);
        } else {
            made = time_signature_create(top, unit, &signatures[index]);
        }
        note_value_destroy(unit);
        if (made != TIME_SIGNATURE_OK) {
            ok = common_json_bad(&r->json, sig_at, "not a time signature");
            break;
        }
        points[index].time_signature = signatures[index];
        index++;
    }
    MeterMap *map = NULL;
    EntityID *id = NULL;
    if (ok) {
        if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
            ok = common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, list_at, "out of memory");
        } else {
            MeterMapResult made = meter_map_create(id, points, count, &map);
            if (made != METER_MAP_OK) {
                entity_id_destroy(id);
                ok = made == METER_MAP_ERR_ALLOC ? common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, list_at, "out of memory")
                                                 : common_json_bad(&r->json, list_at, "the meter changes must start from 0 and ascend");
            }
        }
    }
    for (size_t i = 0; signatures && i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    free(signatures);
    free(points);
    *out = ok ? map : NULL;
    return ok;
}

static bool read_tempo_map(Reader *r, const cJSON *object, const char *path, const char *id_text, TempoMap **out) {
    *out = NULL;
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *list = common_json_require_array(&r->json, object, path, "tempo", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    size_t count = (size_t)cJSON_GetArraySize(list);
    if (count == 0) {
        return true;
    }
    TempoMapChangePoint *points = calloc(count, sizeof(TempoMapChangePoint));
    if (!points) {
        return common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, list_at, "out of memory");
    }
    bool ok = true;
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[REALIZATION_JSON_PATH_MAX];
        char quarters_at[REALIZATION_JSON_PATH_MAX];
        Rational quarters;
        common_json_index_path(at, REALIZATION_JSON_PATH_MAX, list_at, index);
        common_json_child_path(quarters_at, REALIZATION_JSON_PATH_MAX, at, "quarters_per_minute");
        const cJSON *tempo;
        if (!common_json_expect_object(&r->json, item, at) || !read_time_member(r, item, at, "position", false, NULL, &points[index].start) ||
            !(tempo = common_json_require(&r->json, item, at, "quarters_per_minute")) || !read_rational(r, tempo, quarters_at, &quarters)) {
            ok = false;
            break;
        }
        /* microseconds per quarter = 60,000,000 / quarters per minute, which has to be a whole number of at least 1 */
        long long scaled = quarters.num > 0 ? MICROSECONDS_PER_MINUTE * quarters.den : 0;
        if (quarters.num <= 0 || scaled % quarters.num != 0 || scaled / quarters.num < 1) {
            ok = common_json_bad(&r->json, quarters_at, "the tempo is not a whole number of microseconds per quarter note");
            break;
        }
        points[index].microseconds_per_quarter = (unsigned long)(scaled / quarters.num);
        points[index].kind = TEMPO_MAP_CHANGE_ABSOLUTE;
        index++;
    }
    if (ok) {
        EntityID *id = NULL;
        TempoMap *map = NULL;
        if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
            ok = common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, list_at, "out of memory");
        } else if (tempo_map_create(id, points, count, &map) != TEMPO_MAP_OK) {
            entity_id_destroy(id);
            ok = common_json_bad(&r->json, list_at, "the tempo changes must start from 0 and ascend");
        } else {
            *out = map;
        }
    }
    free(points);
    return ok;
}

static bool read_region(Reader *r, const cJSON *item, const char *path, Region *out) {
    if (!common_json_expect_object(&r->json, item, path)) {
        return false;
    }
    const char *kind;
    const char *material;
    int center;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_text_member(r, item, path, "kind", false, &kind)) {
        return false;
    }
    if (strcmp(kind, "tonal") != 0) {
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "kind");
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, at, "region kind '%.40s' is not built", kind);
    }
    if (!read_int_member(r, item, path, "center", 0, 11, &center) || !read_text_member(r, item, path, "material", false, &material)) {
        return false;
    }
    const PitchMaterial *found = knowledge_snapshot_material(r->knowledge, material);
    if (!found) {
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "material");
        return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "no material has the id '%.60s'", material);
    }
    *out = region_tonal(center, found);
    return true;
}

static bool read_span_members(Reader *r, const cJSON *object, const char *path, TimeSpan *out) {
    MusicalTime start;
    MusicalTime end;
    if (!read_time_member(r, object, path, "start", false, NULL, &start) || !read_time_member(r, object, path, "end", false, NULL, &end)) {
        return false;
    }
    return time_span_make(start, end, out) == TIME_SPAN_OK || common_json_bad(&r->json, path, "the span ends before it starts");
}

static bool read_line(Reader *r, const cJSON *item, const char *path, Movement *movement) {
    if (!common_json_expect_object(&r->json, item, path)) {
        return false;
    }
    char id[REALIZATION_ID_MAX];
    const char *name;
    int role;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_id_member(r, item, path, "id", id) || !read_text_member(r, item, path, "name", true, &name)) {
        return false;
    }
    const cJSON *role_item = common_json_require(&r->json, item, path, "role");
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "role");
    if (!role_item || !cJSON_IsString(role_item) || !line_role_from_name(role_item->valuestring, (LineRole *)&role)) {
        return role_item ? common_json_bad(&r->json, at, "unknown line role") : false;
    }
    Line *line = NULL;
    RealizationResult made = line_create(id, (LineRole)role, name, &line);
    if (made != REALIZATION_OK) {
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, name ? "name" : "role");
        return group_refused(r, made, at, "line");
    }
    bool ok = true;
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *relations = common_json_require_array(&r->json, item, path, "relations", list_at, REALIZATION_JSON_PATH_MAX);
    size_t index = 0;
    const cJSON *relation;
    ok = relations != NULL;
    cJSON_ArrayForEach(relation, relations) {
        char relation_at[REALIZATION_JSON_PATH_MAX];
        LineRelation value;
        int kind;
        const char *target;
        common_json_index_path(relation_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        memset(&value, 0, sizeof(value));
        MusicalTime start;
        MusicalTime end;
        bool has_start = false;
        bool has_end = false;
        if (!common_json_expect_object(&r->json, relation, relation_at) || !read_name(r, relation, relation_at, "kind", RELATION_NAMES, COUNT_OF(RELATION_NAMES), &kind) ||
            !read_text_member(r, relation, relation_at, "target", false, &target) ||
            !read_int_member(r, relation, relation_at, "interval", -1000, 1000, &value.interval) ||
            !read_duration_member(r, relation, relation_at, "delay", &value.delay) ||
            !read_time_member(r, relation, relation_at, "start", true, &has_start, &start) ||
            !read_time_member(r, relation, relation_at, "end", true, &has_end, &end)) {
            ok = false;
            break;
        }
        value.kind = (LineRelationKind)kind;
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, relation_at, "target");
        if (!copy_id(r, target, at, value.target)) {
            ok = false;
            break;
        }
        if (has_start != has_end) {
            ok = common_json_bad(&r->json, relation_at, "a relation names both ends of its span or neither");
            break;
        }
        value.has_span = has_start;
        if (has_start && time_span_make(start, end, &value.span) != TIME_SPAN_OK) {
            ok = common_json_bad(&r->json, relation_at, "the span ends before it starts");
            break;
        }
        RealizationResult added = line_add_relation(line, &value);
        if (added != REALIZATION_OK) {
            ok = group_refused(r, added, relation_at, "relation");
            break;
        }
    }
    const cJSON *notes = ok ? common_json_require_array(&r->json, item, path, "notes", list_at, REALIZATION_JSON_PATH_MAX) : NULL;
    ok = ok && notes != NULL;
    index = 0;
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        char note_at[REALIZATION_JSON_PATH_MAX];
        LineNote value;
        common_json_index_path(note_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        memset(&value, 0, sizeof(value));
        if (!ok || !common_json_expect_object(&r->json, note, note_at) || !read_id_member(r, note, note_at, "id", value.id) ||
            !read_time_member(r, note, note_at, "onset", false, NULL, &value.onset) ||
            !read_duration_member(r, note, note_at, "duration", &value.duration) || !read_int_member(r, note, note_at, "midi", 0, 127, &value.midi) ||
            !read_spelling_member(r, note, note_at, &value.spelled, &value.letter, &value.accidental) ||
            !read_derivation_member(r, note, note_at, &value.derivation)) {
            ok = false;
            break;
        }
        RealizationResult added = line_add_note(line, &value);
        if (added != REALIZATION_OK) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, note_at, added == REALIZATION_ERR_BAD_ID || added == REALIZATION_ERR_DUPLICATE_ID ? "id" : "onset");
            ok = group_refused(r, added, at, "note");
            break;
        }
    }
    if (ok) {
        RealizationResult added = movement_add_line(movement, line);
        if (added != REALIZATION_OK) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "id");
            ok = group_refused(r, added, at, "line");
        }
    }
    if (!ok) {
        line_destroy(line);
    }
    return ok;
}

static bool read_part(Reader *r, const cJSON *item, const char *path, Movement *movement) {
    if (!common_json_expect_object(&r->json, item, path)) {
        return false;
    }
    char id[REALIZATION_ID_MAX];
    char instrument[REALIZATION_ID_MAX];
    const char *name;
    int number;
    int players;
    int staves;
    int program;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_id_member(r, item, path, "id", id) || !read_id_member(r, item, path, "instrument", instrument) ||
        !read_text_member(r, item, path, "name", false, &name) || !read_int_member(r, item, path, "number", 1, 1000, &number) ||
        !read_int_member(r, item, path, "players", 1, 1000, &players) || !read_int_member(r, item, path, "staves", 1, 4, &staves) ||
        !read_int_member(r, item, path, "program", 0, 127, &program)) {
        return false;
    }
    PerformerPart *part = NULL;
    RealizationResult made = performer_part_create(id, instrument, name, number, players, staves, program, &part);
    if (made != REALIZATION_OK) {
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "name");
        return group_refused(r, made, at, "part");
    }
    bool ok = true;
    char list_at[REALIZATION_JSON_PATH_MAX];
    MeterMap *local = NULL;
    char map_id[REALIZATION_ID_MAX + 16];
    snprintf(map_id, sizeof(map_id), "%s-meter", id);
    ok = read_meter_map(r, item, path, "local_meter", map_id, &local);
    if (ok && local) {
        RealizationResult set = performer_part_set_local_meter_map(part, local);
        if (set != REALIZATION_OK) {
            meter_map_destroy(local);
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "local_meter");
            ok = group_refused(r, set, at, "local meter");
        }
    }
    const cJSON *pedals = ok ? common_json_require_array(&r->json, item, path, "pedals", list_at, REALIZATION_JSON_PATH_MAX) : NULL;
    ok = ok && pedals != NULL;
    size_t index = 0;
    const cJSON *pedal;
    cJSON_ArrayForEach(pedal, pedals) {
        char pedal_at[REALIZATION_JSON_PATH_MAX];
        PedalMarking marking;
        const char *kind;
        common_json_index_path(pedal_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!ok || !common_json_expect_object(&r->json, pedal, pedal_at) || !read_text_member(r, pedal, pedal_at, "kind", false, &kind) ||
            !read_span_members(r, pedal, pedal_at, &marking.span)) {
            ok = false;
            break;
        }
        if (strcmp(kind, "sustain") != 0) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, pedal_at, "kind");
            ok = common_json_bad(&r->json, at, "expected \"sustain\"");
            break;
        }
        marking.kind = PEDAL_SUSTAIN;
        RealizationResult added = performer_part_add_pedal(part, &marking);
        if (added != REALIZATION_OK) {
            ok = group_refused(r, added, pedal_at, "pedal");
            break;
        }
    }
    const cJSON *notes = ok ? common_json_require_array(&r->json, item, path, "notes", list_at, REALIZATION_JSON_PATH_MAX) : NULL;
    ok = ok && notes != NULL;
    index = 0;
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        char note_at[REALIZATION_JSON_PATH_MAX];
        PerformerNote value;
        common_json_index_path(note_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        memset(&value, 0, sizeof(value));
        if (!ok || !common_json_expect_object(&r->json, note, note_at) || !read_id_member(r, note, note_at, "id", value.id) ||
            !read_time_member(r, note, note_at, "onset", false, NULL, &value.onset) ||
            !read_duration_member(r, note, note_at, "duration", &value.duration) || !read_int_member(r, note, note_at, "midi", 0, 127, &value.midi) ||
            !read_spelling_member(r, note, note_at, &value.spelled, &value.letter, &value.accidental) ||
            !read_int_member(r, note, note_at, "staff", 1, 4, &value.staff) || !read_int_member(r, note, note_at, "voice", 1, 1000, &value.voice) ||
            !read_derivation_member(r, note, note_at, &value.derivation) || !common_json_require_bool(&r->json, note, note_at, "edited", &value.edited) ||
            !common_json_require_bool(&r->json, note, note_at, "detached", &value.detached) || !common_json_require_bool(&r->json, note, note_at, "review", &value.review)) {
            ok = false;
            break;
        }
        RealizationResult added = performer_part_add_note(part, &value);
        if (added != REALIZATION_OK) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, note_at, added == REALIZATION_ERR_BAD_ID || added == REALIZATION_ERR_DUPLICATE_ID ? "id" : "staff");
            ok = group_refused(r, added, at, "note");
            break;
        }
    }
    if (ok) {
        RealizationResult added = movement_add_part(movement, part);
        if (added != REALIZATION_OK) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "id");
            ok = group_refused(r, added, at, "part");
        }
    }
    if (!ok) {
        performer_part_destroy(part);
    }
    return ok;
}

static bool read_assignment(Reader *r, const cJSON *item, const char *path, Movement *movement) {
    if (!common_json_expect_object(&r->json, item, path)) {
        return false;
    }
    ScoringAssignment value;
    memset(&value, 0, sizeof(value));
    int kind;
    if (!read_id_member(r, item, path, "id", value.id) || !read_id_member(r, item, path, "line", value.line) ||
        !read_id_member(r, item, path, "part", value.part) || !common_json_require_bool(&r->json, item, path, "whole", &value.whole)) {
        return false;
    }
    if (!value.whole && !read_span_members(r, item, path, &value.span)) {
        return false;
    }
    if (!read_int_member(r, item, path, "staff", 1, 4, &value.staff) || !read_int_member(r, item, path, "voice", 1, 1000, &value.voice) ||
        !common_json_require_bool(&r->json, item, path, "divisi", &value.divisi)) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    const cJSON *transform = common_json_require(&r->json, item, path, "transform");
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "transform");
    if (!transform || !common_json_expect_object(&r->json, transform, at) || !read_name(r, transform, at, "kind", TRANSFORM_NAMES, COUNT_OF(TRANSFORM_NAMES), &kind) ||
        !read_int_member(r, transform, at, "octaves", -4, 4, &value.transform.octaves) ||
        !read_int_member(r, transform, at, "semitones", -127, 127, &value.transform.semitones) ||
        !read_int_member(r, transform, at, "steps", -127, 127, &value.transform.steps)) {
        return false;
    }
    value.transform.kind = (ScoringTransformKind)kind;
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "id");
    RealizationResult added = movement_add_assignment(movement, &value);
    if (added != REALIZATION_OK) {
        return group_refused(r, added, at, "assignment");
    }
    /* The line and the part are named by id: they are read by now. */
    if (!movement_line_named(movement, value.line)) {
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "line");
        return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "the movement has no line '%.40s'", value.line);
    }
    if (!movement_part_named(movement, value.part)) {
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "part");
        return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "the movement has no part '%.40s'", value.part);
    }
    return true;
}

static bool read_movement_body(Reader *r, const cJSON *item, const char *path, Movement *movement, const char *id) {
    char list_at[REALIZATION_JSON_PATH_MAX];
    char at[REALIZATION_JSON_PATH_MAX];
    char map_id[REALIZATION_ID_MAX + 16];
    MeterMap *meter = NULL;
    TempoMap *tempo = NULL;
    snprintf(map_id, sizeof(map_id), "%s-meter", id);
    if (!read_meter_map(r, item, path, "meter", map_id, &meter)) {
        return false;
    }
    if (meter && movement_set_meter_map(movement, meter) != REALIZATION_OK) {
        meter_map_destroy(meter);
        return common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, path, "out of memory");
    }
    snprintf(map_id, sizeof(map_id), "%s-tempo", id);
    if (!read_tempo_map(r, item, path, map_id, &tempo)) {
        return false;
    }
    if (tempo && movement_set_tempo_map(movement, tempo) != REALIZATION_OK) {
        tempo_map_destroy(tempo);
        return common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, path, "out of memory");
    }
    const cJSON *home = common_json_require(&r->json, item, path, "home");
    common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "home");
    if (!home) {
        return false;
    }
    if (!cJSON_IsNull(home)) {
        Region region;
        if (!read_region(r, home, at, &region)) {
            return false;
        }
        RealizationResult set = movement_set_home(movement, region);
        if (set != REALIZATION_OK) {
            return group_refused(r, set, at, "home");
        }
    }
    const char *ensemble;
    if (!read_text_member(r, item, path, "ensemble", true, &ensemble)) {
        return false;
    }
    if (ensemble) {
        RealizationResult set = movement_set_ensemble(movement, ensemble);
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, path, "ensemble");
        if (set != REALIZATION_OK) {
            return group_refused(r, set, at, "ensemble");
        }
    }
    const cJSON *list = common_json_require_array(&r->json, item, path, "key_signatures", list_at, REALIZATION_JSON_PATH_MAX);
    size_t index = 0;
    const cJSON *entry;
    if (!list) {
        return false;
    }
    cJSON_ArrayForEach(entry, list) {
        MovementKeySignature key;
        int mode;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        memset(&key, 0, sizeof(key));
        if (!common_json_expect_object(&r->json, entry, entry_at) || !read_time_member(r, entry, entry_at, "position", false, NULL, &key.position) ||
            !read_int_member(r, entry, entry_at, "sharps_flats", -7, 7, &key.sharps_flats) ||
            !read_name(r, entry, entry_at, "mode", KEY_MODE_NAMES, COUNT_OF(KEY_MODE_NAMES), &mode)) {
            return false;
        }
        key.mode = (MovementKeyMode)mode;
        RealizationResult added = movement_add_key_signature(movement, &key);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "key signature");
        }
    }
    list = common_json_require_array(&r->json, item, path, "markers", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        MovementMarker marker;
        const char *text;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        memset(&marker, 0, sizeof(marker));
        if (!common_json_expect_object(&r->json, entry, entry_at) || !read_time_member(r, entry, entry_at, "position", false, NULL, &marker.position) ||
            !read_text_member(r, entry, entry_at, "text", false, &text)) {
            return false;
        }
        if (strlen(text) >= sizeof(marker.text)) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, entry_at, "text");
            return common_json_bad(&r->json, at, "a marker is at most 63 characters");
        }
        snprintf(marker.text, sizeof(marker.text), "%s", text);
        RealizationResult added = movement_add_marker(movement, &marker);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "marker");
        }
    }
    list = common_json_require_array(&r->json, item, path, "played", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        TimeSpan span;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!common_json_expect_object(&r->json, entry, entry_at) || !read_span_members(r, entry, entry_at, &span)) {
            return false;
        }
        RealizationResult added = movement_add_played_span(movement, span);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "played span");
        }
    }
    list = common_json_require_array(&r->json, item, path, "compound_parts", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        MovementPart part;
        const char *name;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        memset(&part, 0, sizeof(part));
        if (!common_json_expect_object(&r->json, entry, entry_at) || !read_id_member(r, entry, entry_at, "id", part.id) ||
            !read_text_member(r, entry, entry_at, "name", false, &name) || !read_span_members(r, entry, entry_at, &part.span)) {
            return false;
        }
        if (strlen(name) >= sizeof(part.name)) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, entry_at, "name");
            return common_json_bad(&r->json, at, "a name is at most 63 characters");
        }
        snprintf(part.name, sizeof(part.name), "%s", name);
        const cJSON *part_home = common_json_require(&r->json, entry, entry_at, "home");
        if (!part_home) {
            return false;
        }
        if (!cJSON_IsNull(part_home)) {
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, entry_at, "home");
            part.has_home = true;
            if (!read_region(r, part_home, at, &part.home)) {
                return false;
            }
        }
        RealizationResult added = movement_add_compound_part(movement, &part);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "compound part");
        }
    }
    list = common_json_require_array(&r->json, item, path, "lines", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!read_line(r, entry, entry_at, movement)) {
            return false;
        }
    }
    list = common_json_require_array(&r->json, item, path, "parts", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!read_part(r, entry, entry_at, movement)) {
            return false;
        }
    }
    list = common_json_require_array(&r->json, item, path, "assignments", list_at, REALIZATION_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!read_assignment(r, entry, entry_at, movement)) {
            return false;
        }
    }
    return true;
}

static bool read_work(Reader *r, const cJSON *document, Work **out) {
    const cJSON *item = common_json_require(&r->json, document, "", "work");
    if (!item || !common_json_expect_object(&r->json, item, "work")) {
        return false;
    }
    char id[REALIZATION_ID_MAX];
    const char *title;
    const char *composer;
    const char *ensemble;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_id_member(r, item, "work", "id", id) || !read_text_member(r, item, "work", "title", true, &title) ||
        !read_text_member(r, item, "work", "composer", true, &composer) || !read_text_member(r, item, "work", "ensemble", true, &ensemble)) {
        return false;
    }
    Work *work = NULL;
    RealizationResult made = work_create(id, title, composer, &work);
    if (made != REALIZATION_OK) {
        return group_refused(r, made, "work", "work");
    }
    bool ok = true;
    if (ensemble) {
        RealizationResult set = work_set_ensemble(work, ensemble);
        common_json_child_path(at, REALIZATION_JSON_PATH_MAX, "work", "ensemble");
        ok = set == REALIZATION_OK || group_refused(r, set, at, "ensemble");
    }
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *list = ok ? common_json_require_array(&r->json, item, "work", "material", list_at, REALIZATION_JSON_PATH_MAX) : NULL;
    ok = ok && list != NULL;
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!cJSON_IsString(entry) || !entry->valuestring) {
            ok = common_json_bad(&r->json, entry_at, "expected a material reference like \"form:binary\"");
            break;
        }
        RealizationResult added = work_add_material(work, entry->valuestring);
        if (added != REALIZATION_OK) {
            ok = group_refused(r, added, entry_at, "material");
            break;
        }
    }
    list = ok ? common_json_require_array(&r->json, item, "work", "movements", list_at, REALIZATION_JSON_PATH_MAX) : NULL;
    ok = ok && list != NULL;
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        char movement_id[REALIZATION_ID_MAX];
        const char *movement_title;
        common_json_index_path(entry_at, REALIZATION_JSON_PATH_MAX, list_at, index++);
        if (!ok || !common_json_expect_object(&r->json, entry, entry_at) || !read_id_member(r, entry, entry_at, "id", movement_id) ||
            !read_text_member(r, entry, entry_at, "title", true, &movement_title)) {
            ok = false;
            break;
        }
        Movement *movement = NULL;
        RealizationResult started = movement_create(movement_id, movement_title, &movement);
        if (started != REALIZATION_OK) {
            ok = group_refused(r, started, entry_at, "movement");
            break;
        }
        if (!read_movement_body(r, entry, entry_at, movement, movement_id)) {
            movement_destroy(movement);
            ok = false;
            break;
        }
        RealizationResult added = work_add_movement(work, movement);
        if (added != REALIZATION_OK) {
            movement_destroy(movement);
            common_json_child_path(at, REALIZATION_JSON_PATH_MAX, entry_at, "id");
            ok = group_refused(r, added, at, "movement");
            break;
        }
    }
    if (!ok) {
        work_destroy(work);
        return false;
    }
    *out = work;
    return true;
}

RealizationResult realization_work_from_json(const KnowledgeSnapshot *knowledge, const char *text, Work **out, RealizationJsonError *error) {
    if (out) {
        *out = NULL;
    }
    if (!knowledge || !text || !out) {
        realization_json_set_error(error, "", "a knowledge snapshot, a text and an out are required");
        return REALIZATION_ERR_NULL;
    }
    Reader reader;
    common_json_reader_init(&reader.json, error ? error->path : NULL, REALIZATION_JSON_PATH_MAX, error ? error->message : NULL,
                            error ? sizeof(error->message) : 0, REALIZATION_ERR_BAD_VALUE, "missing required member");
    reader.knowledge = knowledge;
    const char *end = NULL;
    cJSON *document = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!document) {
        realization_json_set_error(error, "", "not valid JSON (byte %ld)", end ? (long)(end - text) : 0L);
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!cJSON_IsObject(document)) {
        common_json_bad(&reader.json, "", "expected a JSON object");
    } else {
        const cJSON *format = cJSON_GetObjectItemCaseSensitive(document, "format");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(document, "format_version");
        if (!cJSON_IsString(format) || strcmp(format->valuestring, FORMAT_NAME) != 0) {
            common_json_bad(&reader.json, "format", "expected \"" FORMAT_NAME "\"");
        } else if (!cJSON_IsNumber(version) || version->valuedouble != (double)URSATZ_REALIZATION_FORMAT_VERSION) {
            common_json_bad(&reader.json, "format_version", "expected the format version 1");
        } else {
            read_work(&reader, document, out);
        }
    }
    cJSON_Delete(document);
    return reader.json.failed ? (RealizationResult)reader.json.code : REALIZATION_OK;
}
