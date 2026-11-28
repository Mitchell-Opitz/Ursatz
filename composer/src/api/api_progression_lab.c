#include "api_progression_lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/tonality/modal_palette.h>
#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/harmony_phrase.h>

#include "api_internal.h"
#include "api_construction.h"
#include "api_vocabulary.h"
#include "api_workflow.h"
#include "harmony/harmony_palette.h"
#include "piece/request_chords.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"

#define LAB_BARS_MAX 64
#define LAB_CHORDS_MAX 64
#define LAB_SUGGEST_MAX 8

/* --- reading the request ------------------------------------------------------------------------------------- */

typedef struct {
    const char *key_text;
    const char *meter_text;
    SpelledKey key;
    PlanMeter meter;
    long bars;
    bool has_cadence; /* false for "none" */
    CadenceType cadence;
    unsigned long seed;
    unsigned long suggest; /* 0 for none */
    bool plans;
    ApiOutputs outputs;
    const PitchMaterial *with;
    int units_per_bar;
    HarmonyPaletteRequest operator_;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    const VoicingEntry *voicing;
    ComposerChords chords;
    CommonPracticeMaterials materials;
} LabRequest;

/* Whether every member of the object is one of `allowed` (NULL ends the list); else refuses the first that is not, at its path. */
static bool closed_object(const cJSON *object, const char *const *allowed, const char *base, const char *what, UrsatzError *error) {
    for (const cJSON *member = object->child; member != NULL; member = member->next) {
        bool known = false;
        for (size_t i = 0; allowed[i] != NULL; i++) {
            known = known || strcmp(member->string, allowed[i]) == 0;
        }
        if (!known) {
            api_fail(error, URSATZ_ERR_REQUEST, base, "%s takes only %s%s%s%s%s; '%s' is not one of them", what, allowed[0],
                     allowed[1] ? ", " : "", allowed[1] ? allowed[1] : "", allowed[1] && allowed[2] ? ", ..." : "", "", member->string);
            size_t used = (size_t)snprintf(error->path, sizeof(error->path), "%s", base);
            snprintf(error->path + used, sizeof(error->path) - used, "/%s", member->string);
            return false;
        }
    }
    return true;
}

static bool whole_member(const cJSON *object, const char *name, long minimum, long maximum, const char *path, long *out, UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble || (long)item->valuedouble < minimum || (long)item->valuedouble > maximum) {
        api_fail(error, URSATZ_ERR_REQUEST, path, "%s must be a whole number from %ld to %ld", name, minimum, maximum);
        return false;
    }
    *out = (long)item->valuedouble;
    return true;
}

/* The units a bar has: 1, the meter's harmony cells or its beats (4/4: 1, 2 or 4; 3/4: 1 or 3; 2/4: 1 or 2; 6/8: 1 or 2). */
static bool units_allowed(const PlanMeter *meter, long units) {
    if (units == 1) {
        return true;
    }
    return units == 2 ? meter->beats_per_bar % 2 == 0 || meter->beat_unit == 8 : units == meter->beats_per_bar && meter->beat_unit == 4;
}

static const char *const POOL_NAMES[] = {"first", "second", "shared", "color", "any"};

static UrsatzStatus read_arc(const cJSON *palette, const char *name, long bars, HarmonyPaletteArc *out, UrsatzError *error) {
    static const char *const SHAPES[] = {"rise", "fall", "hold", "peak", "step"};
    static const char *const MEMBERS[] = {"shape", "at", NULL};
    const cJSON *arc = cJSON_GetObjectItemCaseSensitive(palette, name);
    char path[64];
    snprintf(path, sizeof path, "/palette/%s", name);
    out->shape = HARMONY_SHAPE_NONE;
    out->at = 0;
    if (arc == NULL || cJSON_IsNull(arc)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsObject(arc)) {
        return api_fail(error, URSATZ_ERR_REQUEST, path, "%s is {\"shape\": rise|fall|hold|peak|step, \"at\": <bar>}", name);
    }
    if (!closed_object(arc, MEMBERS, path, name, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const cJSON *shape = cJSON_GetObjectItemCaseSensitive(arc, "shape");
    for (int i = 0; cJSON_IsString(shape) && i < 5; i++) {
        if (strcmp(shape->valuestring, SHAPES[i]) == 0) {
            out->shape = (HarmonyPaletteShape)(i + 1);
        }
    }
    if (out->shape == HARMONY_SHAPE_NONE) {
        return api_fail(error, URSATZ_ERR_REQUEST, path, "the shape of %s is rise, fall, hold, peak or step", name);
    }
    bool needs_at = out->shape == HARMONY_SHAPE_PEAK || out->shape == HARMONY_SHAPE_STEP;
    if (cJSON_GetObjectItemCaseSensitive(arc, "at") != NULL || needs_at) {
        long at;
        char at_path[80];
        snprintf(at_path, sizeof at_path, "%s/at", path);
        if (!whole_member(arc, "at", 1, bars, at_path, &at, error)) {
            return URSATZ_ERR_REQUEST;
        }
        out->at = (int)at;
    }
    return URSATZ_OK;
}

/* The chord of the key's tonic and the approach the cadence asks for: V in a major or minor key, the cadence kind's last approach degree in a modal one. */
static bool cadence_chords(const LabRequest *lab, PaletteChord *tonic, PaletteChord *approach) {
    int root;
    StandardTonalityTriadQuality quality;
    int intervals[3];
    memset(tonic, 0, sizeof(*tonic));
    memset(approach, 0, sizeof(*approach));
    if (!pitch_material_triad_on(lab->key.material, 1, &root, intervals, &quality)) {
        return false;
    }
    tonic->root_semitones = root;
    tonic->quality = quality;
    modal_palette_major_degree(root, &tonic->degree, &tonic->alteration);
    int approach_degree = 5;
    const VocabularyCadence *kind = composer_chords_cadence(&lab->chords, lab->key.material);
    if (kind != NULL && kind->approach_count > 0) {
        approach_degree = kind->approach_degrees[kind->approach_count - 1];
    }
    if (kind == NULL || kind->approach_count == 0) {
        /* A major or minor key's dominant is the major triad on its fifth degree. */
        approach->root_semitones = pitch_material_step(lab->key.material, 5);
        approach->quality = STANDARD_TONALITY_TRIAD_MAJOR;
    } else if (!pitch_material_triad_on(lab->key.material, approach_degree, &approach->root_semitones, intervals, &approach->quality)) {
        return false;
    }
    modal_palette_major_degree(approach->root_semitones, &approach->degree, &approach->alteration);
    return approach->root_semitones >= 0;
}

static UrsatzStatus read_spans(const cJSON *palette, const ComposerContext *context, LabRequest *lab, UrsatzError *error) {
    static const char *const MEMBERS[] = {"bars", "chords", "schema", "loop", NULL};
    const cJSON *spans = cJSON_GetObjectItemCaseSensitive(palette, "spans");
    HarmonyPaletteRequest *op = &lab->operator_;
    if (spans == NULL || cJSON_IsNull(spans)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsArray(spans) || cJSON_GetArraySize(spans) > HARMONY_PALETTE_SPANS_MAX) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/palette/spans", "spans is a list of 1 to %d spans", HARMONY_PALETTE_SPANS_MAX);
    }
    bool taken[LAB_BARS_MAX + 1] = {false};
    int index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, spans) {
        char path[96];
        snprintf(path, sizeof path, "/palette/spans/%d", index);
        if (!cJSON_IsObject(item) || !closed_object(item, MEMBERS, path, "a span", error)) {
            return cJSON_IsObject(item) ? URSATZ_ERR_REQUEST : api_fail(error, URSATZ_ERR_REQUEST, path, "a span is {\"bars\": [first, last], \"chords\": ..., \"schema\": ..., \"loop\": ...}");
        }
        const cJSON *bars = cJSON_GetObjectItemCaseSensitive(item, "bars");
        const cJSON *first = cJSON_GetArrayItem(bars, 0);
        const cJSON *last = cJSON_GetArrayItem(bars, 1);
        if (!cJSON_IsArray(bars) || cJSON_GetArraySize(bars) != 2 || !cJSON_IsNumber(first) || !cJSON_IsNumber(last) || first->valuedouble < 1 ||
            last->valuedouble < first->valuedouble || last->valuedouble > (double)lab->bars || first->valuedouble != (double)(long)first->valuedouble ||
            last->valuedouble != (double)(long)last->valuedouble) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "bars is [first, last], whole bars from 1 to %ld", lab->bars);
        }
        for (long b = (long)first->valuedouble; b <= (long)last->valuedouble; b++) {
            if (taken[b]) {
                return api_fail(error, URSATZ_ERR_REQUEST, path, "bar %ld is in two spans", b);
            }
            taken[b] = true;
        }
        HarmonyPaletteSpan *span = &op->spans[op->span_count++];
        memset(span, 0, sizeof(*span));
        span->first_unit = (size_t)(((long)first->valuedouble - 1) * lab->units_per_bar);
        span->unit_count = (size_t)(((long)last->valuedouble - (long)first->valuedouble + 1) * lab->units_per_bar);
        span->pool = HARMONY_POOL_ANY;
        const cJSON *pool = cJSON_GetObjectItemCaseSensitive(item, "chords");
        if (pool != NULL && !cJSON_IsNull(pool)) {
            bool known = false;
            for (int p = 0; cJSON_IsString(pool) && p < 5; p++) {
                if (strcmp(pool->valuestring, POOL_NAMES[p]) == 0) {
                    span->pool = (HarmonyPalettePool)p;
                    known = true;
                }
            }
            if (!known) {
                return api_fail(error, URSATZ_ERR_REQUEST, path, "chords is shared, color, first, second or any");
            }
            if (lab->with == NULL && (span->pool == HARMONY_POOL_SHARED || span->pool == HARMONY_POOL_COLOR || span->pool == HARMONY_POOL_SECOND)) {
                snprintf(path + strlen(path), sizeof path - strlen(path), "/chords");
                return api_fail(error, URSATZ_ERR_REQUEST, path, "'%s' needs a second mode; give palette.with", pool->valuestring);
            }
        }
        const cJSON *schema = cJSON_GetObjectItemCaseSensitive(item, "schema");
        const cJSON *loop = cJSON_GetObjectItemCaseSensitive(item, "loop");
        if (loop != NULL && !cJSON_IsNull(loop) && !cJSON_IsBool(loop)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "loop is true or false");
        }
        if (schema != NULL && !cJSON_IsNull(schema)) {
            char schema_path[112];
            snprintf(schema_path, sizeof schema_path, "%s/schema", path);
            const SchemaEntry *entry = cJSON_IsString(schema) ? knowledge_snapshot_schema(context->knowledge, schema->valuestring) : NULL;
            if (entry == NULL || entry->subtype != SCHEMA_PROGRESSION) {
                return api_fail(error, URSATZ_ERR_REQUEST, schema_path, "'%s' is not a progression schema; see list_schemas",
                                cJSON_IsString(schema) ? schema->valuestring : "?");
            }
            span->schema = entry;
            span->loop = cJSON_IsBool(loop) ? cJSON_IsTrue(loop) : entry->loop;
            size_t total = 0;
            for (size_t k = 0; k < entry->stage_count; k++) {
                total += entry->stages[k].units == 0 ? 1 : entry->stages[k].units;
            }
            if (!span->loop && total > span->unit_count) {
                return api_fail(error, URSATZ_ERR_REQUEST, schema_path, "the schema '%s' takes %zu units and the span has %zu; make the span longer or set loop",
                                entry->id, total, span->unit_count);
            }
        }
        index++;
    }
    return URSATZ_OK;
}

static UrsatzStatus read_chords(const cJSON *palette, LabRequest *lab, UrsatzError *error) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(palette, "chords");
    HarmonyPaletteRequest *op = &lab->operator_;
    GivenChord given[LAB_CHORDS_MAX];
    char numerals[LAB_CHORDS_MAX][PLAN_ROMAN_NUMERAL_MAX];
    size_t count = 0;
    if (list == NULL || cJSON_IsNull(list)) {
        return URSATZ_OK;
    }
    UrsatzStatus status = request_read_chords(&lab->chords, list, "/palette/chords", "the request", 1, &lab->meter, &lab->key, lab->bars, given, numerals,
                                              LAB_CHORDS_MAX, &count, error);
    if (status != URSATZ_OK) {
        return status;
    }
    Rational bar_length;
    plan_meter_bar_length(&lab->meter, &bar_length);
    for (size_t i = 0; i < count; i++) {
        Rational unit_length;
        Rational at;
        char path[64];
        snprintf(path, sizeof path, "/palette/chords/%zu", i);
        if (rational_div_int(bar_length, lab->units_per_bar, &unit_length) != RATIONAL_OK || rational_div(given[i].onset, unit_length, &at) != RATIONAL_OK || at.den != 1) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "a chord starts on a unit of the palette's chords_per_bar (%d a bar)", lab->units_per_bar);
        }
        HarmonyPaletteGiven *given_chord = &op->given[op->given_count++];
        int root;
        StandardTonalityTriadQuality quality;
        if (!composer_given_chord_triad(&lab->chords, &lab->key, numerals[i], &root, &quality)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "'%s' is not a triad the palette places", numerals[i]);
        }
        memset(given_chord, 0, sizeof(*given_chord));
        given_chord->unit = (size_t)at.num;
        given_chord->chord.root_semitones = root;
        given_chord->chord.quality = quality;
        modal_palette_major_degree(root, &given_chord->chord.degree, &given_chord->chord.alteration);
    }
    return URSATZ_OK;
}

static UrsatzStatus read_palette(const cJSON *request, const ComposerContext *context, LabRequest *lab, UrsatzError *error) {
    static const char *const MEMBERS[] = {"with", "chords_per_bar", "spans", "chords", "brightness", "tension", NULL};
    const cJSON *palette = cJSON_GetObjectItemCaseSensitive(request, "palette");
    HarmonyPaletteRequest *op = &lab->operator_;
    if (!cJSON_IsObject(palette) || !closed_object(palette, MEMBERS, "/palette", "palette", error)) {
        return cJSON_IsObject(palette) ? URSATZ_ERR_REQUEST : api_fail(error, URSATZ_ERR_REQUEST, "/palette", "palette is an object");
    }
    const cJSON *with = cJSON_GetObjectItemCaseSensitive(palette, "with");
    if (with != NULL && !cJSON_IsNull(with)) {
        lab->with = cJSON_IsString(with) ? knowledge_snapshot_material_named(context->knowledge, with->valuestring) : NULL;
        if (lab->with == NULL || !composer_material_supported(&lab->materials, lab->with)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/palette/with", "'%s' is not a mode the Composer writes; write major, minor, dorian, phrygian, lydian, mixolydian or locrian",
                            cJSON_IsString(with) ? with->valuestring : "?");
        }
        if (lab->with == lab->key.material) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/palette/with", "%s is the key's own mode; leave with out or name another", lab->with->key_word);
        }
    }
    lab->units_per_bar = 1;
    const cJSON *per_bar = cJSON_GetObjectItemCaseSensitive(palette, "chords_per_bar");
    if (per_bar != NULL && !cJSON_IsNull(per_bar)) {
        long units;
        if (!whole_member(palette, "chords_per_bar", 1, 4, "/palette/chords_per_bar", &units, error)) {
            return URSATZ_ERR_REQUEST;
        }
        if (!units_allowed(&lab->meter, units)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/palette/chords_per_bar", "a bar of %s takes 1, 2 or its beats as many chords; %ld is not one of them", lab->meter_text, units);
        }
        lab->units_per_bar = (int)units;
    }
    memset(op, 0, sizeof(*op));
    op->key = &lab->key;
    op->second = lab->with;
    op->major = lab->materials.major;
    op->bars = lab->bars;
    op->units_per_bar = lab->units_per_bar;
    op->seed = lab->seed;
    UrsatzStatus status = read_spans(palette, context, lab, error);
    if (status == URSATZ_OK) {
        status = read_chords(palette, lab, error);
    }
    if (status == URSATZ_OK) {
        status = read_arc(palette, "brightness", lab->bars, &op->brightness, error);
    }
    if (status == URSATZ_OK) {
        status = read_arc(palette, "tension", lab->bars, &op->tension, error);
    }
    return status;
}

static UrsatzStatus read_lab(const cJSON *request, const ComposerContext *context, LabRequest *lab, UrsatzError *error) {
    static const char *const MEMBERS[] = {"api_version", "key", "meter", "bars", "cadence", "seed", "suggest", "plans", "midi", "musicxml", "tempo",
                                          "plain_midi", "palette", "accompaniment", "layers", "bass", NULL};
    static const char *const REFUSED[] = {"schema", "sequence", "embellish"};
    const char *cadence_text = NULL;
    bool found;
    memset(lab, 0, sizeof(*lab));
    for (size_t i = 0; i < 3; i++) {
        if (cJSON_GetObjectItemCaseSensitive(request, REFUSED[i]) != NULL) {
            char path[32];
            snprintf(path, sizeof path, "/%s", REFUSED[i]);
            return api_fail(error, URSATZ_ERR_REQUEST, path, "a palette writes its own chords; leave out schema, sequence and embellish");
        }
    }
    if (!closed_object(request, MEMBERS, "", "a progression with a palette", error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!api_has_member(request, "key") || !api_has_member(request, "meter") || !api_has_member(request, "bars") || !api_has_member(request, "cadence")) {
        return api_fail(error, URSATZ_ERR_REQUEST, "", "progression needs key, meter, bars and cadence");
    }
    UrsatzStatus status = api_require_knowledge(context, &lab->materials, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!composer_chords_init(&lab->chords, context->knowledge, NULL, 0)) {
        return api_fail(error, URSATZ_ERR_INTERNAL, "", "the knowledge has no chords");
    }
    if (!api_member_string(request, "key", &lab->key_text, &found, error) || !api_member_string(request, "meter", &lab->meter_text, &found, error) ||
        !api_member_string(request, "cadence", &cadence_text, &found, error) || !api_member_bool(request, "plans", &lab->plans, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!request_parse_key(lab->key_text, context->knowledge, &lab->key)) {
        if (api_fail_unsupported_key(error, "/key", context->knowledge, lab->key_text)) {
            return URSATZ_ERR_REQUEST;
        }
        return api_fail(error, URSATZ_ERR_REQUEST, "/key", "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, for example \"D minor\"", lab->key_text);
    }
    if (!request_parse_meter(lab->meter_text, &lab->meter) || !harmony_meter_supported(&lab->meter)) {
        return api_fail_meter(error, "/meter", lab->meter_text);
    }
    unsigned long bars = 0;
    if (!api_member_whole(request, "bars", 1, LAB_BARS_MAX, &bars, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    lab->bars = (long)bars;
    if (strcmp(cadence_text, "none") == 0 || strcmp(cadence_text, "NONE") == 0 || strcmp(cadence_text, "None") == 0) {
        lab->has_cadence = false;
    } else if (request_parse_cadence(cadence_text, &lab->cadence) &&
               (lab->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || lab->cadence == CADENCE_TYPE_HALF)) {
        lab->has_cadence = true;
    } else {
        return api_fail(error, URSATZ_ERR_REQUEST, "/cadence", "a palette's cadence is none, PAC or HC, not '%s'", cadence_text);
    }
    unsigned long seed = 0;
    if (!api_member_whole(request, "seed", 0, API_SEED_MAX, &seed, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    lab->seed = found ? seed : api_default_seed();
    if (!api_member_whole(request, "suggest", 1, LAB_SUGGEST_MAX, &lab->suggest, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool bass = true;
    if (!api_member_bool(request, "bass", &bass, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!bass) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/bass", "a palette progression has its bass; leave bass out");
    }
    status = api_read_outputs(request, false, &lab->outputs, error);
    if (status != URSATZ_OK) {
        return status;
    }
    status = read_palette(request, context, lab, error);
    if (status != URSATZ_OK) {
        return status;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(request, "accompaniment");
    if (accompaniment != NULL && !cJSON_IsNull(accompaniment)) {
        static const char *const ACCOMPANIMENT_MEMBERS[] = {"pattern", "voicing", NULL};
        const char *pattern = NULL;
        if (!cJSON_IsObject(accompaniment) || !closed_object(accompaniment, ACCOMPANIMENT_MEMBERS, "/accompaniment", "accompaniment", error)) {
            return cJSON_IsObject(accompaniment) ? URSATZ_ERR_REQUEST : api_fail(error, URSATZ_ERR_REQUEST, "/accompaniment", "accompaniment is {\"pattern\": ...}");
        }
        if (!api_request_string(accompaniment, "/accompaniment", "pattern", true, &pattern, error)) {
            return URSATZ_ERR_REQUEST;
        }
        status = api_read_accompaniment(pattern, NULL, lab->meter_text, &lab->meter, context->knowledge, &lab->accompaniment, error);
        if (status != URSATZ_OK) {
            return status;
        }
        const cJSON *voicing = cJSON_GetObjectItemCaseSensitive(accompaniment, "voicing");
        if (voicing != NULL && !cJSON_IsNull(voicing)) {
            if (!cJSON_IsString(voicing) || lab->accompaniment.pattern != PLAN_ACCOMPANIMENT_BLOCK) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/accompaniment/voicing", "a voicing is the id of a voicing and needs the pattern \"block\"");
            }
            lab->voicing = knowledge_snapshot_voicing(context->knowledge, voicing->valuestring);
            if (lab->voicing == NULL) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/accompaniment/voicing", "'%s' is not a voicing; see the vocabulary's voicings", voicing->valuestring);
            }
        }
        lab->has_accompaniment = true;
    }
    if (lab->has_cadence) {
        HarmonyPaletteRequest *op = &lab->operator_;
        op->has_cadence = true;
        op->cadence = lab->cadence;
        if (!cadence_chords(lab, &op->tonic, &op->approach)) {
            return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/cadence", "the key has no chords for a %s", lab->cadence == CADENCE_TYPE_HALF ? "HC" : "PAC");
        }
    }
    return URSATZ_OK;
}

/* --- the entry ----------------------------------------------------------------------------------------------- */

UrsatzStatus api_progression_lab(const ComposerContext *context, const cJSON *request, const UrsatzJob *job, ComposerResponse *out) {
    (void)job;
    LabRequest lab;
    UrsatzStatus status = read_lab(request, context, &lab, &out->error);
    if (status != URSATZ_OK) {
        return status;
    }
    return api_fail(&out->error, URSATZ_ERR_UNSATISFIABLE, "/palette", "the palette progression is not written yet");
}
