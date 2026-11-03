#include <ursatz_composer/analysis_load.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pitch/pitch_arithmetic.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "plan/plan_json_internal.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))


void analysis_load_report_destroy(AnalysisLoadReport *report) {
    if (!report) {
        return;
    }
    free(report->notes);
    report->notes = NULL;
    report->count = 0;
}

typedef struct {
    AnalysisLoadReport *report;
    size_t capacity;
    bool failed;
} Notes;

static void note(Notes *notes, const char *path, const char *format, ...) {
    if (!notes->report || notes->failed) {
        return;
    }
    if (notes->report->count == notes->capacity) {
        size_t capacity = notes->capacity ? notes->capacity * 2 : 8;
        AnalysisLoadNote *grown = realloc(notes->report->notes, capacity * sizeof(AnalysisLoadNote));
        if (!grown) {
            notes->failed = true;
            return;
        }
        notes->report->notes = grown;
        notes->capacity = capacity;
    }
    AnalysisLoadNote *entry = &notes->report->notes[notes->report->count++];
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    va_end(args);
}

typedef struct {
    PlanSpan span;
    PlanLanePayload payload;
    char path[PLAN_JSON_PATH_MAX];
    bool keep;
} Claim;

/* "Eb minor" -> tonic 3, minor. */
static bool parse_key_label(const char *label, PlanKey *key) {
    const char *space = strrchr(label, ' ');
    if (!space) {
        return false;
    }
    size_t tonic_length = (size_t)(space - label);
    int mode = -1;
    if (strcmp(space + 1, "major") == 0) {
        mode = PLAN_MODE_MAJOR;
    } else if (strcmp(space + 1, "minor") == 0) {
        mode = PLAN_MODE_MINOR;
    }
    if (mode < 0) {
        return false;
    }
    for (int pitch_class = 0; pitch_class < 12; pitch_class++) {
        const char *flat = pitch_class_name(pitch_class, true);
        const char *sharp = pitch_class_name(pitch_class, false);
        if ((strlen(flat) == tonic_length && strncmp(label, flat, tonic_length) == 0) ||
            (strlen(sharp) == tonic_length && strncmp(label, sharp, tonic_length) == 0)) {
            key->tonic_pitch_class = pitch_class;
            key->mode = (PlanKeyMode)mode;
            return true;
        }
    }
    return false;
}

/*
 * Reads one claims array. Claims with no usable label are left out and noted.
 * Returns false on a malformed claim.
 */
static bool read_claims(JsonReader *reader, Notes *notes, const cJSON *document, const char *section,
                        PlanLaneKind lane, Claim **out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    const cJSON *array = plan_json_member(reader, document, "", section, false);
    if (!array) {
        return true;
    }
    if (!cJSON_IsArray(array)) {
        return json_bad_format(reader, section, "expected an array");
    }
    size_t total = (size_t)cJSON_GetArraySize(array);
    if (total == 0) {
        return true;
    }
    Claim *claims = calloc(total, sizeof(Claim));
    if (!claims) {
        return json_fail(reader, PLAN_ERR_ALLOC, section, "out of memory");
    }
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, section, index++);
        if (!cJSON_IsObject(item)) {
            free(claims);
            return json_bad_format(reader, at, "expected an object");
        }
        const cJSON *label = plan_json_member(reader, item, at, "label", true);
        PlanSpan span;
        if (!label || !json_read_span(reader, item, at, &span)) {
            free(claims);
            return false;
        }
        char label_path[PLAN_JSON_PATH_MAX];
        json_child_path(label_path, at, "label");
        if (!cJSON_IsString(label)) {
            free(claims);
            return json_bad_format(reader, label_path, "expected a string");
        }

        Claim *claim = &claims[*out_count];
        memset(&claim->payload, 0, sizeof(claim->payload));
        if (lane == PLAN_LANE_KEY) {
            if (!parse_key_label(label->valuestring, &claim->payload.key)) {
                note(notes, label_path, "key label '%.40s' skipped", label->valuestring);
                continue;
            }
        } else {
            size_t length = strlen(label->valuestring);
            if (strcmp(label->valuestring, "no match") == 0 || length == 0 || length >= PLAN_ROMAN_NUMERAL_MAX) {
                note(notes, label_path, "chord label '%.40s' skipped", label->valuestring);
                continue;
            }
            memcpy(claim->payload.harmony.roman_numeral, label->valuestring, length + 1);
            claim->payload.harmony.has_function = false;
        }
        claim->span = span;
        claim->keep = true;
        snprintf(claim->path, sizeof(claim->path), "%s", at);
        (*out_count)++;
    }
    *out = claims;
    return true;
}

/* A claim that runs past the start of the next is cut off where the next begins. */
static void trim_overlaps(Notes *notes, Claim *claims, size_t count) {
    for (size_t i = 0; i + 1 < count; i++) {
        Rational end;
        if (!claims[i].keep || plan_span_end(claims[i].span, &end) != PLAN_OK) {
            continue;
        }
        if (rational_cmp(end, claims[i + 1].span.start) <= 0) {
            continue;
        }
        Rational negative_start = {-claims[i].span.start.num, claims[i].span.start.den};
        Rational length;
        rational_add(claims[i + 1].span.start, negative_start, &length);
        if (length.num <= 0) {
            claims[i].keep = false;
            note(notes, claims[i].path, "starts with the next claim; skipped");
        } else {
            claims[i].span.length = length;
            note(notes, claims[i].path, "runs past the next claim; trimmed to its start");
        }
    }
}

static void widen_extent(const Claim *claims, size_t count, bool *have, Rational *start, Rational *end) {
    for (size_t i = 0; i < count; i++) {
        Rational claim_end;
        if (!claims[i].keep || plan_span_end(claims[i].span, &claim_end) != PLAN_OK) {
            continue;
        }
        if (!*have || rational_cmp(claims[i].span.start, *start) < 0) {
            *start = claims[i].span.start;
        }
        if (!*have || rational_cmp(claim_end, *end) > 0) {
            *end = claim_end;
        }
        *have = true;
    }
}

static bool add_claims(JsonReader *reader, PiecePlan *plan, NodeId root, PlanLaneKind lane, const Claim *claims,
                       size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!claims[i].keep) {
            continue;
        }
        LaneSpanId id;
        PlanResult result = plan_add_lane_span(plan, root, lane, 0, claims[i].span, &claims[i].payload, &id);
        if (result == PLAN_ERR_ALLOC) {
            return json_fail(reader, PLAN_ERR_ALLOC, claims[i].path, "out of memory");
        }
        if (result != PLAN_OK) {
            return json_fail(reader, PLAN_ERR_FORMAT, claims[i].path, "claim cannot be placed on the timeline");
        }
        plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED);
    }
    return true;
}

static void note_unmapped_sections(Notes *notes, const cJSON *document) {
    static const char *const SECTIONS[] = {"meter",   "cadences", "phrases", "form",         "motives",
                                           "melody",  "rhythm",   "texture", "voice_leading", "schemas"};
    for (size_t i = 0; i < COUNT_OF(SECTIONS); i++) {
        const cJSON *section = cJSON_GetObjectItemCaseSensitive(document, SECTIONS[i]);
        bool has_content = section && !cJSON_IsNull(section) && !(cJSON_IsArray(section) && cJSON_GetArraySize(section) == 0);
        if (has_content) {
            note(notes, SECTIONS[i], "section has content the plan has no place for yet; not loaded");
        }
    }
}

static PlanResult build_plan(const cJSON *document, PiecePlan *plan, Notes *notes, PlanJsonError *error) {
    JsonReader reader = {error, PLAN_OK};
    if (!cJSON_IsObject(document)) {
        json_bad_format(&reader, "", "expected a JSON object");
        return reader.result;
    }
    long version;
    if (!json_read_int_member(&reader, document, "", "schema_version", 1, PLAN_JSON_SCHEMA_VERSION, &version)) {
        return reader.result;
    }

    Claim *keys = NULL;
    Claim *chords = NULL;
    size_t key_count = 0;
    size_t chord_count = 0;
    bool ok = read_claims(&reader, notes, document, "keys", PLAN_LANE_KEY, &keys, &key_count) &&
              read_claims(&reader, notes, document, "chords", PLAN_LANE_HARMONY, &chords, &chord_count);
    if (ok) {
        trim_overlaps(notes, keys, key_count);
        trim_overlaps(notes, chords, chord_count);
    }

    bool have_extent = false;
    Rational extent_start = {0, 1};
    Rational extent_end = {0, 1};
    if (ok) {
        widen_extent(keys, key_count, &have_extent, &extent_start, &extent_end);
        widen_extent(chords, chord_count, &have_extent, &extent_start, &extent_end);
        if (!have_extent) {
            ok = json_bad_format(&reader, "keys", "no key or chord claim to build a plan from");
        }
    }

    if (ok) {
        PlanSpan extent = {extent_start, {0, 1}};
        Rational negative_start = {-extent_start.num, extent_start.den};
        rational_add(extent_end, negative_start, &extent.length);

        const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
        const cJSON *source = cJSON_GetObjectItemCaseSensitive(metadata, "source_file");
        const char *name = cJSON_IsString(source) && source->valuestring[0] != '\0' ? source->valuestring : NULL;

        NodeId root;
        PlanResult result = plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, extent, name, &root);
        if (result != PLAN_OK) {
            ok = json_fail(&reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, "", "could not build the form root");
        } else {
            plan_node_set_state(plan, root, PLAN_STATE_REALIZED);
            ok = add_claims(&reader, plan, root, PLAN_LANE_KEY, keys, key_count) &&
                 add_claims(&reader, plan, root, PLAN_LANE_HARMONY, chords, chord_count);
        }
    }
    free(keys);
    free(chords);
    if (ok) {
        note_unmapped_sections(notes, document);
        if (notes->failed) {
            return json_fail(&reader, PLAN_ERR_ALLOC, "", "out of memory");
        }
    }
    return reader.result;
}

PlanResult plan_from_analysis_document(const char *text, PiecePlan **out, AnalysisLoadReport *report,
                                       PlanJsonError *error) {
    if (!text || !out) {
        json_set_error(error, -1, "", "text or out is NULL");
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    if (report) {
        report->notes = NULL;
        report->count = 0;
    }
    const char *end = NULL;
    cJSON *document = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!document) {
        json_set_error(error, end ? (long)(end - text) : -1, "", "not valid JSON");
        return PLAN_ERR_PARSE;
    }
    PiecePlan *plan = plan_create();
    if (!plan) {
        cJSON_Delete(document);
        json_set_error(error, -1, "", "out of memory");
        return PLAN_ERR_ALLOC;
    }
    Notes notes = {report, 0, false};
    PlanResult result = build_plan(document, plan, &notes, error);
    cJSON_Delete(document);
    if (result != PLAN_OK) {
        plan_destroy(plan);
        analysis_load_report_destroy(report);
        return result;
    }
    *out = plan;
    return PLAN_OK;
}

PlanResult plan_read_analysis_document_file(const char *path, PiecePlan **out, AnalysisLoadReport *report,
                                            PlanJsonError *error) {
    if (!path || !out) {
        json_set_error(error, -1, "", "path or out is NULL");
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        json_set_error(error, -1, "", "could not open '%s'", path);
        return PLAN_ERR_IO;
    }
    char *text = NULL;
    size_t size = 0;
    size_t capacity = 0;
    bool ok = true;
    while (ok && !feof(file)) {
        if (size + 4096 + 1 > capacity) {
            capacity = capacity ? capacity * 2 : 8192;
            char *grown = realloc(text, capacity);
            ok = grown != NULL;
            text = grown ? grown : text;
        }
        if (ok) {
            size += fread(text + size, 1, 4096, file);
            ok = !ferror(file);
        }
    }
    fclose(file);
    if (!ok) {
        free(text);
        json_set_error(error, -1, "", "could not read '%s'", path);
        return PLAN_ERR_IO;
    }
    text[size] = '\0';
    PlanResult result = plan_from_analysis_document(text, out, report, error);
    free(text);
    return result;
}
