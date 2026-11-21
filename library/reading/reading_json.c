#include "reading/reading_json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/json.h>

#include "model/time/musical_time.h"
#include "model/time/time_span.h"

#define FORMAT_NAME "ursatz-reading"

static const char *const SOURCE_NAMES[] = {"analyzer", "composer", "person"};
static const char *const STRENGTH_NAMES[] = {"established", "tonicized", "implied", "fragile", "absent"};
static const char *const ROLE_NAMES[] = {"functional", "embellishing"};
static const char *const GROUP_NAMES[] = {"section", "theme", "phrase", "subphrase", "idea"};
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* ------------------------------------------------------------------------------------------------ writing */

/* The first failure is kept; every builder still runs afterwards but its items are dropped, so no step needs its own cleanup. */
typedef struct {
    ReadingJsonError *error;
    bool failed;
} Writer;

static void set_error(ReadingJsonError *error, const char *path, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    snprintf(error->path, sizeof(error->path), "%s", path);
}

static void write_fail(Writer *w, const char *message) {
    if (!w->failed) {
        w->failed = true;
        set_error(w->error, "", "%s", message);
    }
}

static void put(Writer *w, cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        write_fail(w, "out of memory");
        return;
    }
    if (!object || !cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        write_fail(w, "out of memory");
    }
}

static void push(Writer *w, cJSON *array, cJSON *item) {
    if (!item) {
        write_fail(w, "out of memory");
        return;
    }
    if (!array || !cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        write_fail(w, "out of memory");
    }
}

static cJSON *number_item(long long value) {
    return cJSON_CreateNumber((double)value);
}

static cJSON *text_or_null(const char *text) {
    return text ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *rational_item(Rational value) {
    return common_json_rational_object(value.num, value.den);
}

/* The members every claim begins with. */
static cJSON *claim_item(Writer *w, const ReadingClaim *claim) {
    cJSON *object = cJSON_CreateObject();
    put(w, object, "id", cJSON_CreateString(claim->id));
    put(w, object, "start", rational_item(claim->start));
    put(w, object, "end", rational_item(claim->end));
    put(w, object, "confidence", claim->confidence.present ? rational_item(claim->confidence.value) : cJSON_CreateNull());
    cJSON *evidence = cJSON_CreateArray();
    for (size_t i = 0; i < claim->evidence.count; i++) {
        push(w, evidence, cJSON_CreateString(claim->evidence.ids[i]));
    }
    put(w, object, "evidence", evidence);
    return object;
}

static cJSON *chord_reading_item(Writer *w, const ReadingChord *chord) {
    if (!chord->has_reading) {
        return cJSON_CreateNull();
    }
    const ChordReading *reading = &chord->reading;
    cJSON *object = cJSON_CreateObject();
    put(w, object, "root", number_item(reading->has_root ? reading->root_pitch_class : -1));
    put(w, object, "type", text_or_null(reading->type ? reading->type->id : NULL));
    put(w, object, "pitch_classes", number_item(reading->sonority.pitch_classes));
    put(w, object, "bass", number_item(reading->bass_pitch_class));
    cJSON *members = cJSON_CreateArray();
    for (size_t i = 0; i < CHORD_READING_ROLE_COUNT; i++) {
        push(w, members, number_item(reading->members_by_role[i]));
    }
    put(w, object, "members", members);
    put(w, object, "borrowed_from", text_or_null(reading->borrowed_from ? reading->borrowed_from->id : NULL));
    put(w, object, "role", cJSON_CreateString(ROLE_NAMES[reading->role]));
    if (reading->has_expected) {
        cJSON *expected = cJSON_CreateObject();
        put(w, expected, "root", number_item(reading->expected_root_pitch_class));
        put(w, expected, "type", text_or_null(reading->expected_type ? reading->expected_type->id : NULL));
        put(w, object, "expected", expected);
    } else {
        put(w, object, "expected", cJSON_CreateNull());
    }
    return object;
}

static cJSON *cadence_array(Writer *w, const CadenceType *types, size_t count) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        push(w, array, cJSON_CreateString(cadence_type_name(types[i])));
    }
    return array;
}

static cJSON *document_item(Writer *w, const Reading *reading) {
    cJSON *document = cJSON_CreateObject();
    put(w, document, "format", cJSON_CreateString(FORMAT_NAME));
    put(w, document, "format_version", number_item(URSATZ_READING_FORMAT_VERSION));
    const ReadingSource *source = reading_source(reading);
    cJSON *object = cJSON_CreateObject();
    put(w, object, "kind", cJSON_CreateString(SOURCE_NAMES[source->kind]));
    put(w, object, "name", cJSON_CreateString(source->name));
    put(w, object, "producer_key", cJSON_CreateString(source->producer_key));
    put(w, object, "stages", number_item((long long)source->stages));
    put(w, object, "knowledge_hash", cJSON_CreateString(source->knowledge_hash));
    put(w, document, "source", object);
    const ReadingTarget *target = reading_target(reading);
    object = cJSON_CreateObject();
    put(w, object, "work", cJSON_CreateString(target->work));
    put(w, object, "movement", cJSON_CreateString(target->movement));
    put(w, object, "digest", cJSON_CreateString(target->digest));
    put(w, document, "target", object);

    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_region_count(reading); i++) {
        const ReadingRegion *region = reading_region_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)region);
        put(w, item, "center", number_item(region->center));
        put(w, item, "material", cJSON_CreateString(region->material->id));
        put(w, item, "strength", cJSON_CreateString(STRENGTH_NAMES[region->strength]));
        put(w, item, "label", cJSON_CreateString(region->label));
        push(w, list, item);
    }
    put(w, document, "regions", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_chord_count(reading); i++) {
        const ReadingChord *chord = reading_chord_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)chord);
        put(w, item, "reading", chord_reading_item(w, chord));
        put(w, item, "numeral", cJSON_CreateString(chord->numeral));
        put(w, item, "function", cJSON_CreateString(chord->function));
        put(w, item, "label", cJSON_CreateString(chord->label));
        push(w, list, item);
    }
    put(w, document, "chords", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_cadence_count(reading); i++) {
        const ReadingCadence *cadence = reading_cadence_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)cadence);
        put(w, item, "type", cJSON_CreateString(cadence_type_name(cadence->type)));
        put(w, item, "arrival", rational_item(cadence->arrival));
        put(w, item, "alternatives", cadence_array(w, cadence->alternatives, cadence->alternative_count));
        push(w, list, item);
    }
    put(w, document, "cadences", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_group_count(reading); i++) {
        const ReadingGroup *group = reading_group_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)group);
        put(w, item, "kind", cJSON_CreateString(GROUP_NAMES[group->kind]));
        put(w, item, "parent", group->parent == READING_NO_PARENT ? cJSON_CreateNull() : number_item((long long)group->parent));
        put(w, item, "label", cJSON_CreateString(group->label));
        put(w, item, "form", cJSON_CreateString(group->form));
        put(w, item, "beginning", cJSON_CreateString(group->beginning));
        put(w, item, "ending", cJSON_CreateString(group->ending));
        put(w, item, "commonality", cJSON_CreateString(group->commonality));
        put(w, item, "first_measure", number_item(group->first_measure));
        put(w, item, "last_measure", number_item(group->last_measure));
        put(w, item, "phrase_count", number_item((long long)group->phrase_count));
        put(w, item, "part_cadences", cadence_array(w, group->part_cadences, group->part_cadence_count));
        put(w, item, "archetypal", cJSON_CreateBool(group->archetypal));
        put(w, item, "fragmentation", cJSON_CreateBool(group->fragmentation));
        push(w, list, item);
    }
    put(w, document, "groups", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_motive_occurrence_count(reading); i++) {
        const ReadingMotiveOccurrence *occurrence = reading_motive_occurrence_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)occurrence);
        put(w, item, "motive", number_item((long long)occurrence->motive));
        put(w, item, "kind", cJSON_CreateString(occurrence->kind));
        push(w, list, item);
    }
    put(w, document, "motive_occurrences", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_meter_count(reading); i++) {
        const ReadingMeter *meter = reading_meter_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)meter);
        put(w, item, "time_signature", cJSON_CreateString(meter->time_signature));
        push(w, list, item);
    }
    put(w, document, "meters", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_line_count(reading); i++) {
        const ReadingLine *line = reading_line_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)line);
        put(w, item, "role", cJSON_CreateString(line_role_name(line->role)));
        push(w, list, item);
    }
    put(w, document, "lines", list);
    list = cJSON_CreateArray();
    for (size_t i = 0; i < reading_voice_fault_count(reading); i++) {
        const ReadingVoiceFault *fault = reading_voice_fault_at(reading, i);
        cJSON *item = claim_item(w, (const ReadingClaim *)fault);
        put(w, item, "rule", cJSON_CreateString(fault->rule));
        put(w, item, "hard", cJSON_CreateBool(fault->hard));
        put(w, item, "measure", number_item(fault->measure));
        push(w, list, item);
    }
    put(w, document, "voice_faults", list);
    return document;
}

char *reading_to_json(const Reading *reading, ReadingJsonError *error) {
    if (!reading) {
        set_error(error, "", "no reading");
        return NULL;
    }
    if (reading_target(reading)->digest[0] == '\0') {
        set_error(error, "target.digest", "the reading names no realization version");
        return NULL;
    }
    Writer writer = {error, false};
    cJSON *document = document_item(&writer, reading);
    char *text = NULL;
    if (!writer.failed) {
        char *printed = cJSON_Print(document);
        if (printed) {
            text = common_json_indent(printed);
            cJSON_free(printed);
        }
        if (!text) {
            write_fail(&writer, "out of memory");
        }
    }
    cJSON_Delete(document);
    return text;
}

/* ------------------------------------------------------------------------------------------------ reading */

typedef struct {
    CommonJsonReader json;
    const KnowledgeSnapshot *knowledge;
} Reader;

static bool group_refused(Reader *r, RealizationResult result, const char *path) {
    switch (result) {
    case REALIZATION_ERR_BAD_ID:
        return common_json_refuse(&r->json, result, path, "not a valid id");
    case REALIZATION_ERR_DUPLICATE_ID:
        return common_json_refuse(&r->json, result, path, "the id is used twice");
    case REALIZATION_ERR_ALLOC:
        return common_json_refuse(&r->json, result, path, "out of memory");
    default:
        return common_json_refuse(&r->json, result, path, "the claim is refused: a value is out of range");
    }
}

static bool read_int_member(Reader *r, const cJSON *object, const char *path, const char *key, long long min, long long max, int *out) {
    long long value;
    if (!common_json_require_whole(&r->json, object, path, key, min, max, &value)) {
        return false;
    }
    *out = (int)value;
    return true;
}

/* A text member copied into out (size bytes); text longer than that is refused. */
static bool read_text_member(Reader *r, const cJSON *object, const char *path, const char *key, char *out, size_t size) {
    const cJSON *item = common_json_require(&r->json, object, path, key);
    if (!item) {
        return false;
    }
    char at[READING_JSON_PATH_MAX];
    common_json_child_path(at, READING_JSON_PATH_MAX, path, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return common_json_bad(&r->json, at, "expected text");
    }
    if (strlen(item->valuestring) >= size) {
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, at, "the text is longer than %zu characters", size - 1);
    }
    snprintf(out, size, "%s", item->valuestring);
    return true;
}

static bool read_rational(Reader *r, const cJSON *item, const char *path, Rational *out) {
    long long num;
    long long den;
    return common_json_read_time(&r->json, item, path, 1000000000LL, "expected a time {\"num\", \"den\"}", &num, &den) &&
           (rational_make(num, den, out) == RATIONAL_OK || common_json_bad(&r->json, path, "not a time"));
}

static bool read_rational_member(Reader *r, const cJSON *object, const char *path, const char *key, Rational *out) {
    const cJSON *item = common_json_require(&r->json, object, path, key);
    char at[READING_JSON_PATH_MAX];
    common_json_child_path(at, READING_JSON_PATH_MAX, path, key);
    return item && read_rational(r, item, at, out);
}

static bool read_name(Reader *r, const cJSON *object, const char *path, const char *key, const char *const *names, size_t count, int *out) {
    char text[40];
    if (!read_text_member(r, object, path, key, text, sizeof(text))) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(text, names[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    char at[READING_JSON_PATH_MAX];
    common_json_child_path(at, READING_JSON_PATH_MAX, path, key);
    return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, at, "unknown name '%.30s'", text);
}

static bool read_cadence_name(Reader *r, const cJSON *item, const char *path, CadenceType *out) {
    if (!cJSON_IsString(item) || !item->valuestring) {
        return common_json_bad(&r->json, path, "expected a cadence type name");
    }
    if (!cadence_type_from_name(item->valuestring, out)) {
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, path, "unknown cadence type '%.40s'", item->valuestring);
    }
    return true;
}

static bool read_cadence_array(Reader *r, const cJSON *object, const char *path, const char *key, size_t max, CadenceType *out, size_t *count) {
    char list_at[READING_JSON_PATH_MAX];
    const cJSON *list = common_json_require_array(&r->json, object, path, key, list_at, READING_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    if ((size_t)cJSON_GetArraySize(list) > max) {
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, list_at, "at most %zu cadence types", max);
    }
    *count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[READING_JSON_PATH_MAX];
        common_json_index_path(at, READING_JSON_PATH_MAX, list_at, *count);
        if (!read_cadence_name(r, item, at, &out[*count])) {
            return false;
        }
        (*count)++;
    }
    return true;
}

/* A claim's common members into `claim`; its evidence goes into `buffer`, which the caller frees (the reading copies it). */
static bool read_head(Reader *r, const cJSON *item, const char *path, ReadingClaim *claim, char (**buffer)[REALIZATION_ID_MAX]) {
    memset(claim, 0, sizeof(*claim));
    claim->detail = READING_NO_DETAIL;
    *buffer = NULL;
    if (!common_json_expect_object(&r->json, item, path) || !read_text_member(r, item, path, "id", claim->id, sizeof(claim->id)) ||
        !read_rational_member(r, item, path, "start", &claim->start) || !read_rational_member(r, item, path, "end", &claim->end)) {
        return false;
    }
    const cJSON *confidence = common_json_require(&r->json, item, path, "confidence");
    char at[READING_JSON_PATH_MAX];
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "confidence");
    if (!confidence) {
        return false;
    }
    if (!cJSON_IsNull(confidence)) {
        claim->confidence.present = true;
        if (!read_rational(r, confidence, at, &claim->confidence.value)) {
            return false;
        }
    }
    char list_at[READING_JSON_PATH_MAX];
    const cJSON *list = common_json_require_array(&r->json, item, path, "evidence", list_at, READING_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    size_t count = (size_t)cJSON_GetArraySize(list);
    if (count > 0) {
        *buffer = malloc(count * REALIZATION_ID_MAX);
        if (!*buffer) {
            return common_json_refuse(&r->json, REALIZATION_ERR_ALLOC, list_at, "out of memory");
        }
    }
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        common_json_index_path(at, READING_JSON_PATH_MAX, list_at, claim->evidence.count);
        if (!cJSON_IsString(entry) || !entry->valuestring || strlen(entry->valuestring) >= REALIZATION_ID_MAX) {
            return common_json_bad(&r->json, at, "expected a note id");
        }
        snprintf((*buffer)[claim->evidence.count++], REALIZATION_ID_MAX, "%s", entry->valuestring);
    }
    claim->evidence.ids = *buffer;
    return true;
}

static bool read_chord_reading(Reader *r, const cJSON *item, const char *path, const Rational *start, const Rational *end, ReadingChord *chord) {
    if (cJSON_IsNull(item)) {
        chord->has_reading = false;
        return true;
    }
    if (!common_json_expect_object(&r->json, item, path)) {
        return false;
    }
    chord->has_reading = true;
    ChordReading *reading = &chord->reading;
    memset(reading, 0, sizeof(*reading));
    MusicalTime from = musical_time_of(*start);
    MusicalTime to = musical_time_of(*end);
    int root;
    int mask;
    int bass;
    char id[96];
    char at[READING_JSON_PATH_MAX];
    if (!read_int_member(r, item, path, "root", -1, 11, &root) || !read_int_member(r, item, path, "pitch_classes", 0, 4095, &mask) ||
        !read_int_member(r, item, path, "bass", -1, 11, &bass)) {
        return false;
    }
    reading->sonority.pitch_classes = (unsigned)mask;
    reading->sonority.bass_pitch_class = bass;
    if (time_span_make(from, to, &reading->sonority.span) != TIME_SPAN_OK) {
        return common_json_bad(&r->json, path, "the span ends before it starts");
    }
    reading->has_root = root >= 0;
    reading->root_pitch_class = root >= 0 ? root : 0;
    reading->bass_pitch_class = bass;
    const cJSON *type = common_json_require(&r->json, item, path, "type");
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "type");
    if (!type) {
        return false;
    }
    if (!cJSON_IsNull(type)) {
        if (!cJSON_IsString(type) || !type->valuestring) {
            return common_json_bad(&r->json, at, "expected a chord type id or null");
        }
        reading->type = knowledge_snapshot_chord_type(r->knowledge, type->valuestring);
        if (!reading->type) {
            return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "no chord type has the id '%.60s'", type->valuestring);
        }
    }
    char list_at[READING_JSON_PATH_MAX];
    const cJSON *members = common_json_require_array(&r->json, item, path, "members", list_at, READING_JSON_PATH_MAX);
    if (!members) {
        return false;
    }
    if ((size_t)cJSON_GetArraySize(members) != CHORD_READING_ROLE_COUNT) {
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, list_at, "expected %d member masks", CHORD_READING_ROLE_COUNT);
    }
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, members) {
        long long value;
        common_json_index_path(at, READING_JSON_PATH_MAX, list_at, index);
        if (!common_json_read_whole(&r->json, entry, at, 0, 4095, &value)) {
            return false;
        }
        reading->members_by_role[index++] = (unsigned)value;
    }
    const cJSON *borrowed = common_json_require(&r->json, item, path, "borrowed_from");
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "borrowed_from");
    if (!borrowed) {
        return false;
    }
    if (!cJSON_IsNull(borrowed)) {
        if (!cJSON_IsString(borrowed) || !borrowed->valuestring) {
            return common_json_bad(&r->json, at, "expected a material id or null");
        }
        reading->borrowed_from = knowledge_snapshot_material(r->knowledge, borrowed->valuestring);
        if (!reading->borrowed_from) {
            return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "no material has the id '%.60s'", borrowed->valuestring);
        }
    }
    int role;
    if (!read_name(r, item, path, "role", ROLE_NAMES, COUNT_OF(ROLE_NAMES), &role)) {
        return false;
    }
    reading->role = (ChordRole)role;
    const cJSON *expected = common_json_require(&r->json, item, path, "expected");
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "expected");
    if (!expected) {
        return false;
    }
    if (!cJSON_IsNull(expected)) {
        if (!common_json_expect_object(&r->json, expected, at)) {
            return false;
        }
        reading->has_expected = true;
        if (!read_int_member(r, expected, at, "root", 0, 11, &reading->expected_root_pitch_class)) {
            return false;
        }
        const cJSON *expected_type = common_json_require(&r->json, expected, at, "type");
        common_json_child_path(id, READING_JSON_PATH_MAX, at, "type");
        if (!expected_type) {
            return false;
        }
        if (!cJSON_IsNull(expected_type)) {
            if (!cJSON_IsString(expected_type) || !expected_type->valuestring) {
                return common_json_bad(&r->json, id, "expected a chord type id or null");
            }
            reading->expected_type = knowledge_snapshot_chord_type(r->knowledge, expected_type->valuestring);
            if (!reading->expected_type) {
                return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, id, "no chord type has the id '%.60s'", expected_type->valuestring);
            }
        }
    }
    return true;
}

/* Each claim of an array: the head, then the members of its kind through `fill`, then the add call. */
typedef bool (*FillFn)(Reader *r, const cJSON *item, const char *path, void *claim);
typedef RealizationResult (*AddFn)(Reading *reading, const void *claim);

static bool read_claims(Reader *r, Reading *reading, const cJSON *document, const char *key, void *claim, size_t claim_size, FillFn fill, AddFn add) {
    char list_at[READING_JSON_PATH_MAX];
    const cJSON *list = common_json_require_array(&r->json, document, "", key, list_at, READING_JSON_PATH_MAX);
    if (!list) {
        return false;
    }
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[READING_JSON_PATH_MAX];
        char(*evidence)[REALIZATION_ID_MAX] = NULL;
        common_json_index_path(at, READING_JSON_PATH_MAX, list_at, index++);
        ReadingClaim *head = claim;
        memset(claim, 0, claim_size);
        bool ok = read_head(r, item, at, head, &evidence) && fill(r, item, at, claim);
        if (ok) {
            RealizationResult added = add(reading, claim);
            if (added != REALIZATION_OK) {
                char id_at[READING_JSON_PATH_MAX];
                const char *member_name = added == REALIZATION_ERR_DUPLICATE_ID ? "id" : added == REALIZATION_ERR_BAD_ID ? (realization_id_valid(head->id) ? "evidence" : "id") : "start";
                common_json_child_path(id_at, READING_JSON_PATH_MAX, at, member_name);
                ok = group_refused(r, added, id_at);
            }
        }
        free(evidence);
        if (!ok) {
            return false;
        }
    }
    return true;
}

static bool fill_region(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingRegion *region = claim;
    char id[96];
    char at[READING_JSON_PATH_MAX];
    int strength;
    if (!read_int_member(r, item, path, "center", 0, 11, &region->center) || !read_text_member(r, item, path, "material", id, sizeof(id)) ||
        !read_name(r, item, path, "strength", STRENGTH_NAMES, COUNT_OF(STRENGTH_NAMES), &strength) ||
        !read_text_member(r, item, path, "label", region->label, sizeof(region->label))) {
        return false;
    }
    region->material = knowledge_snapshot_material(r->knowledge, id);
    if (!region->material) {
        common_json_child_path(at, READING_JSON_PATH_MAX, path, "material");
        return common_json_refuse(&r->json, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "no material has the id '%.60s'", id);
    }
    region->strength = (RegionStrength)strength;
    return true;
}

static bool fill_chord(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingChord *chord = claim;
    const cJSON *reading = common_json_require(&r->json, item, path, "reading");
    char at[READING_JSON_PATH_MAX];
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "reading");
    return reading && read_chord_reading(r, reading, at, &chord->start, &chord->end, chord) &&
           read_text_member(r, item, path, "numeral", chord->numeral, sizeof(chord->numeral)) &&
           read_text_member(r, item, path, "function", chord->function, sizeof(chord->function)) &&
           read_text_member(r, item, path, "label", chord->label, sizeof(chord->label));
}

static bool fill_cadence(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingCadence *cadence = claim;
    const cJSON *type = common_json_require(&r->json, item, path, "type");
    char at[READING_JSON_PATH_MAX];
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "type");
    return type && read_cadence_name(r, type, at, &cadence->type) && read_rational_member(r, item, path, "arrival", &cadence->arrival) &&
           read_cadence_array(r, item, path, "alternatives", 4, cadence->alternatives, &cadence->alternative_count);
}

static bool fill_group(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingGroup *group = claim;
    int kind;
    int phrase_count;
    const cJSON *parent = common_json_require(&r->json, item, path, "parent");
    char at[READING_JSON_PATH_MAX];
    if (!parent) {
        return false;
    }
    common_json_child_path(at, READING_JSON_PATH_MAX, path, "parent");
    group->parent = READING_NO_PARENT;
    if (!cJSON_IsNull(parent)) {
        long long value;
        if (!common_json_read_whole(&r->json, parent, at, 0, 100000000LL, &value)) {
            return false;
        }
        group->parent = (size_t)value;
    }
    if (!read_name(r, item, path, "kind", GROUP_NAMES, COUNT_OF(GROUP_NAMES), &kind) ||
        !read_text_member(r, item, path, "label", group->label, sizeof(group->label)) ||
        !read_text_member(r, item, path, "form", group->form, sizeof(group->form)) ||
        !read_text_member(r, item, path, "beginning", group->beginning, sizeof(group->beginning)) ||
        !read_text_member(r, item, path, "ending", group->ending, sizeof(group->ending)) ||
        !read_text_member(r, item, path, "commonality", group->commonality, sizeof(group->commonality)) ||
        !read_int_member(r, item, path, "first_measure", -100000, 100000000, &group->first_measure) ||
        !read_int_member(r, item, path, "last_measure", -100000, 100000000, &group->last_measure) ||
        !read_int_member(r, item, path, "phrase_count", 0, 1000000, &phrase_count) ||
        !read_cadence_array(r, item, path, "part_cadences", 2, group->part_cadences, &group->part_cadence_count) ||
        !common_json_require_bool(&r->json, item, path, "archetypal", &group->archetypal) || !common_json_require_bool(&r->json, item, path, "fragmentation", &group->fragmentation)) {
        return false;
    }
    group->kind = (ReadingGroupKind)kind;
    group->phrase_count = (size_t)phrase_count;
    return true;
}

static bool fill_motive(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingMotiveOccurrence *occurrence = claim;
    int motive;
    if (!read_int_member(r, item, path, "motive", 0, 100000000, &motive) || !read_text_member(r, item, path, "kind", occurrence->kind, sizeof(occurrence->kind))) {
        return false;
    }
    occurrence->motive = (size_t)motive;
    return true;
}

static bool fill_meter(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingMeter *meter = claim;
    return read_text_member(r, item, path, "time_signature", meter->time_signature, sizeof(meter->time_signature));
}

static bool fill_line(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingLine *line = claim;
    char role[40];
    char at[READING_JSON_PATH_MAX];
    if (!read_text_member(r, item, path, "role", role, sizeof(role))) {
        return false;
    }
    LineRole found;
    if (!line_role_from_name(role, &found)) {
        common_json_child_path(at, READING_JSON_PATH_MAX, path, "role");
        return common_json_refuse(&r->json, REALIZATION_ERR_BAD_VALUE, at, "unknown line role '%.30s'", role);
    }
    line->role = found;
    return true;
}

static bool fill_fault(Reader *r, const cJSON *item, const char *path, void *claim) {
    ReadingVoiceFault *fault = claim;
    return read_text_member(r, item, path, "rule", fault->rule, sizeof(fault->rule)) && common_json_require_bool(&r->json, item, path, "hard", &fault->hard) &&
           read_int_member(r, item, path, "measure", -100000, 100000000, &fault->measure);
}

static bool read_reading(Reader *r, const cJSON *document, Reading **out) {
    const cJSON *source_item = common_json_require(&r->json, document, "", "source");
    const cJSON *target_item = common_json_require(&r->json, document, "", "target");
    if (!source_item || !target_item || !common_json_expect_object(&r->json, source_item, "source") || !common_json_expect_object(&r->json, target_item, "target")) {
        return false;
    }
    ReadingSource source;
    ReadingTarget target;
    memset(&source, 0, sizeof(source));
    memset(&target, 0, sizeof(target));
    int kind;
    int stages;
    if (!read_name(r, source_item, "source", "kind", SOURCE_NAMES, COUNT_OF(SOURCE_NAMES), &kind) ||
        !read_text_member(r, source_item, "source", "name", source.name, sizeof(source.name)) ||
        !read_text_member(r, source_item, "source", "producer_key", source.producer_key, sizeof(source.producer_key)) ||
        !read_int_member(r, source_item, "source", "stages", 0, 2147483647LL, &stages) ||
        !read_text_member(r, source_item, "source", "knowledge_hash", source.knowledge_hash, sizeof(source.knowledge_hash)) ||
        !read_text_member(r, target_item, "target", "work", target.work, sizeof(target.work)) ||
        !read_text_member(r, target_item, "target", "movement", target.movement, sizeof(target.movement)) ||
        !read_text_member(r, target_item, "target", "digest", target.digest, sizeof(target.digest))) {
        return false;
    }
    source.kind = (ReadingSourceKind)kind;
    source.stages = (unsigned long)stages;
    Reading *reading = NULL;
    RealizationResult made = reading_create(&source, &target, &reading);
    if (made != REALIZATION_OK) {
        return common_json_refuse(&r->json, made, made == REALIZATION_ERR_BAD_ID ? "target" : "source", "the source or the target is refused");
    }
    union {
        ReadingRegion region;
        ReadingChord chord;
        ReadingCadence cadence;
        ReadingGroup group;
        ReadingMotiveOccurrence motive;
        ReadingMeter meter;
        ReadingLine line;
        ReadingVoiceFault fault;
    } claim;
    bool ok = read_claims(r, reading, document, "regions", &claim, sizeof(claim), fill_region, (AddFn)reading_add_region) &&
              read_claims(r, reading, document, "chords", &claim, sizeof(claim), fill_chord, (AddFn)reading_add_chord) &&
              read_claims(r, reading, document, "cadences", &claim, sizeof(claim), fill_cadence, (AddFn)reading_add_cadence) &&
              read_claims(r, reading, document, "groups", &claim, sizeof(claim), fill_group, (AddFn)reading_add_group) &&
              read_claims(r, reading, document, "motive_occurrences", &claim, sizeof(claim), fill_motive, (AddFn)reading_add_motive_occurrence) &&
              read_claims(r, reading, document, "meters", &claim, sizeof(claim), fill_meter, (AddFn)reading_add_meter) &&
              read_claims(r, reading, document, "lines", &claim, sizeof(claim), fill_line, (AddFn)reading_add_line) &&
              read_claims(r, reading, document, "voice_faults", &claim, sizeof(claim), fill_fault, (AddFn)reading_add_voice_fault);
    if (!ok) {
        reading_destroy(reading);
        return false;
    }
    *out = reading;
    return true;
}

RealizationResult reading_from_json(const KnowledgeSnapshot *knowledge, const char *text, Reading **out, ReadingJsonError *error) {
    if (out) {
        *out = NULL;
    }
    if (!knowledge || !text || !out) {
        set_error(error, "", "a knowledge snapshot, a text and an out are required");
        return REALIZATION_ERR_NULL;
    }
    Reader reader;
    common_json_reader_init(&reader.json, error ? error->path : NULL, READING_JSON_PATH_MAX, error ? error->message : NULL,
                            error ? sizeof(error->message) : 0, REALIZATION_ERR_BAD_VALUE, "missing required member");
    reader.knowledge = knowledge;
    const char *end = NULL;
    cJSON *document = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!document) {
        set_error(error, "", "not valid JSON (byte %ld)", end ? (long)(end - text) : 0L);
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!cJSON_IsObject(document)) {
        common_json_bad(&reader.json, "", "expected a JSON object");
    } else {
        const cJSON *format = cJSON_GetObjectItemCaseSensitive(document, "format");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(document, "format_version");
        if (!cJSON_IsString(format) || strcmp(format->valuestring, FORMAT_NAME) != 0) {
            common_json_bad(&reader.json, "format", "expected \"" FORMAT_NAME "\"");
        } else if (!cJSON_IsNumber(version) || version->valuedouble != (double)URSATZ_READING_FORMAT_VERSION) {
            common_json_bad(&reader.json, "format_version", "expected the format version 1");
        } else {
            read_reading(&reader, document, out);
        }
    }
    cJSON_Delete(document);
    return reader.json.failed ? (RealizationResult)reader.json.code : REALIZATION_OK;
}
