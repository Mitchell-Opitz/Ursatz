#include "api_progression_lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/dimension/dimension.h>
#include <plan/plan_constraint.h>
#include <plan/plan_json.h>
#include <theory/tonality/modal_palette.h>
#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/texture_check.h>

#include "api_internal.h"
#include "api_piece.h"
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

/* --- writing the progression -------------------------------------------------------------------------------- */

typedef struct {
    Plan *plan;
    PlanSpan span;
    Rational bar_length;
    HarmonyPaletteChoice choice;
    char bar_text[LAB_BARS_MAX][96]; /* the chord text of each bar: "I V@2" */
} Written;

static Rational time_of(long num, unsigned long den) {
    Rational time;
    rational_make(num, den, &time);
    return time;
}

/* The chord text of a unit's chord: the first the Composer reads as exactly that chord. */
static bool unit_text(const LabRequest *lab, const PaletteChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    return composer_chord_text_of(&lab->chords, &lab->key, chord->degree, chord->alteration, chord->quality, lab->with, out);
}

/* The text of each bar, neighbouring equal units of a bar joined, onsets as @ beats from 0. False for a chord no text of the Composer reads. */
static bool write_bar_texts(const LabRequest *lab, Written *written, UrsatzError *error) {
    int per_bar = lab->units_per_bar;
    int beats_per_unit = lab->meter.beats_per_bar / per_bar;
    for (long bar = 0; bar < lab->bars; bar++) {
        char *text = written->bar_text[bar];
        size_t used = 0;
        text[0] = '\0';
        for (int u = 0; u < per_bar; u++) {
            const PaletteChord *chord = &written->choice.chords[bar * per_bar + u];
            if (u > 0 && chord->root_semitones == written->choice.chords[bar * per_bar + u - 1].root_semitones &&
                chord->quality == written->choice.chords[bar * per_bar + u - 1].quality) {
                continue;
            }
            char chord_text[PLAN_ROMAN_NUMERAL_MAX];
            if (!unit_text(lab, chord, chord_text)) {
                api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/palette", "the chord %s has no text the Composer reads in %s", chord->numeral, lab->key_text);
                return false;
            }
            if (u == 0) {
                used += (size_t)snprintf(text + used, sizeof(written->bar_text[0]) - used, "%s", chord_text);
            } else {
                used += (size_t)snprintf(text + used, sizeof(written->bar_text[0]) - used, " %s@%d", chord_text, u * beats_per_unit);
            }
        }
    }
    return true;
}

/* The inner voices an accompaniment is written in, over the phrase: voice 1 with the constraint, 2 and 3 for the patterns that sound chords. */
static bool add_accompaniment(const LabRequest *lab, Written *written, NodeId phrase) {
    PlanAccompanimentPattern pattern = lab->accompaniment.pattern;
    int voices = pattern == PLAN_ACCOMPANIMENT_ARPEGGIATED || pattern == PLAN_ACCOMPANIMENT_ALBERTI || pattern == PLAN_ACCOMPANIMENT_MOTIVE ? 1 : 3;
    bool ok = true;
    for (int voice = 1; ok && voice <= voices; voice++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        memset(&note, 0, sizeof(note));
        note.midi = INNER_PLACEHOLDER;
        note.offset = time_of(0, 1);
        note.duration = written->span.length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        PlanResult added = plan_add_lane_span(written->plan, phrase, PLAN_LANE_INNER_VOICE, voice, written->span, &payload, &id);
        ok = added == PLAN_OK;
        if (ok && voice == 1) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
            constraint.params.accompaniment = lab->accompaniment;
            ok = plan_lane_span_add_constraint(written->plan, id, &constraint) == PLAN_OK;
        }
    }
    return ok;
}

/* The plan: a form, a section and a phrase of the bars asked for, the meter and the key Given, the chords Given from the texts, an open bass. */
static UrsatzStatus build_lab_plan(const LabRequest *lab, Written *written, UrsatzError *error) {
    written->plan = plan_create();
    if (written->plan == NULL) {
        return api_fail_alloc(error);
    }
    plan_meter_bar_length(&lab->meter, &written->bar_length);
    written->span.start = time_of(0, 1);
    written->span.length = time_of(lab->bars * (long)written->bar_length.num, written->bar_length.den);
    NodeId form;
    NodeId section;
    NodeId phrase;
    PlanLanePayload payload;
    LaneSpanId id;
    bool ok = plan_add_node(written->plan, PLAN_NODE_NONE, PLAN_NODE_FORM, written->span, "form", &form) == PLAN_OK &&
              plan_add_node(written->plan, form, PLAN_NODE_SECTION, written->span, "section", &section) == PLAN_OK &&
              plan_add_node(written->plan, section, PLAN_NODE_PHRASE, written->span, "phrase", &phrase) == PLAN_OK;
    PlanConstraint length = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    length.params.bars = (int)lab->bars;
    ok = ok && plan_node_add_constraint(written->plan, phrase, &length) == PLAN_OK;
    if (ok && lab->has_cadence) {
        PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
        cadence.params.cadence = lab->cadence;
        ok = plan_node_add_constraint(written->plan, phrase, &cadence) == PLAN_OK;
    }
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = lab->meter.beats_per_bar;
    payload.meter.beat_unit = lab->meter.beat_unit;
    ok = ok && plan_add_lane_span(written->plan, phrase, PLAN_LANE_METER, 0, written->span, &payload, &id) == PLAN_OK &&
         plan_lane_span_set_state(written->plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.region = lab->key;
    ok = ok && plan_add_lane_span(written->plan, phrase, PLAN_LANE_REGION, 0, written->span, &payload, &id) == PLAN_OK &&
         plan_lane_span_set_state(written->plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    ok = ok && plan_node_set_state(written->plan, form, PLAN_STATE_GIVEN) == PLAN_OK && plan_node_set_state(written->plan, section, PLAN_STATE_GIVEN) == PLAN_OK &&
         plan_node_set_state(written->plan, phrase, PLAN_STATE_GIVEN) == PLAN_OK;
    if (!ok) {
        return api_fail_alloc(error);
    }
    /* The chords, read back from the texts as a request's chords are, then Given. */
    cJSON *list = cJSON_CreateArray();
    GivenChord *given = calloc(LAB_BARS_MAX * 4, sizeof(*given));
    char(*numerals)[PLAN_ROMAN_NUMERAL_MAX] = calloc(LAB_BARS_MAX * 4, PLAN_ROMAN_NUMERAL_MAX);
    size_t count = 0;
    UrsatzStatus status = list != NULL && given != NULL && numerals != NULL ? URSATZ_OK : api_fail_alloc(error);
    for (long bar = 0; status == URSATZ_OK && bar < lab->bars; bar++) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL || cJSON_AddNumberToObject(entry, "bar", (double)(bar + 1)) == NULL || cJSON_AddStringToObject(entry, "text", written->bar_text[bar]) == NULL ||
            !cJSON_AddItemToArray(list, entry)) {
            cJSON_Delete(entry);
            status = api_fail_alloc(error);
        }
    }
    if (status == URSATZ_OK) {
        status = request_read_chords(&lab->chords, list, "/palette", "the request", 1, &lab->meter, &lab->key, lab->bars, given, numerals, LAB_BARS_MAX * 4, &count, error);
    }
    if (status == URSATZ_OK && plan_give_chords_in(written->plan, &lab->chords, section, given, count) != GIVEN_OK) {
        status = api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/palette", "the plan refused the chords the palette drew");
    }
    cJSON_Delete(list);
    free(given);
    free(numerals);
    if (status != URSATZ_OK) {
        return status;
    }
    PlanNote note = plan_note(40, time_of(0, 1), written->span.length);
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    PlanResult bass_result = plan_add_lane_span(written->plan, phrase, PLAN_LANE_BASS, 0, written->span, &payload, &id);
    ok = bass_result == PLAN_OK;
    if (ok && lab->has_accompaniment) {
        ok = add_accompaniment(lab, written, phrase);
    }
    return ok ? URSATZ_OK : api_fail_alloc(error);
}

/* One run of the fill loop with the operators of a stage. */
static bool run_stage(Written *written, const LabRequest *lab, const UrsatzJob *job, bool accompaniment) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    HarmonyBassParams bass = harmony_bass_params_default();
    AccompanimentParams texture = accompaniment_params_default();
    texture.knowledge = lab->chords.knowledge;
    texture.voicing = lab->voicing;
    if (registry == NULL) {
        return false;
    }
    memset(&config, 0, sizeof(config));
    config.seed = lab->seed;
    config.job = job;
    config.checker = accompaniment ? texture_checker : harmony_phrase_checker;
    bool ok = (accompaniment ? accompaniment_register(registry, &texture, &lab->chords) : harmony_bass_register(registry, &bass, &lab->chords)) == FILL_OK &&
              fill_run(written->plan, registry, &config, &outcome, NULL) == FILL_OK;
    if (ok) {
        fill_outcome_destroy(&outcome);
    }
    fill_registry_destroy(registry);
    return ok;
}

static bool lane_written(const Plan *plan, PlanLaneKind lane, int voice, PlanSpan span) {
    LaneSpanId ids[LAB_CHORDS_MAX * 4];
    size_t count = plan_lane_find(plan, lane, voice, span, ids, LAB_CHORDS_MAX * 4);
    for (size_t i = 0; i < count && i < LAB_CHORDS_MAX * 4; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
    }
    return count > 0;
}

/* The progression of suggestion `suggestion` for the seed: false when the seed gives no bass or no accompaniment for the chords drawn. */
static UrsatzStatus make_lab(LabRequest *lab, const UrsatzJob *job, unsigned long seed, size_t suggestion, Written *written, bool *made, UrsatzError *error) {
    memset(written, 0, sizeof(*written));
    *made = false;
    lab->seed = seed;
    lab->operator_.seed = seed;
    if (!harmony_palette_draw(&lab->operator_, suggestion, &written->choice)) {
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/palette", "the palette has no chord for a unit of the request (a pool is empty, or a schema does not fit its span)");
    }
    UrsatzStatus status = write_bar_texts(lab, written, error) ? build_lab_plan(lab, written, error) : URSATZ_ERR_UNSATISFIABLE;
    if (status != URSATZ_OK) {
        plan_destroy(written->plan);
        written->plan = NULL;
        return status;
    }
    /* The bass is written in one stage over Given chords, which the stage's checker has judged; harmony_check_bass reads the chords without the snapshot and so
       cannot read a borrowed one, which a palette draws. */
    bool ok = run_stage(written, lab, job, false) && lane_written(written->plan, PLAN_LANE_BASS, 0, written->span);
    if (ok && lab->has_accompaniment) {
        ok = run_stage(written, lab, job, true) && lane_written(written->plan, PLAN_LANE_INNER_VOICE, 1, written->span);
    }
    if (!ok) {
        plan_destroy(written->plan);
        written->plan = NULL;
    }
    *made = ok;
    return URSATZ_OK;
}

/* --- the result ---------------------------------------------------------------------------------------------- */

static const char *rational_text(Rational value, char *buffer, size_t size) {
    if (value.den == 1) {
        snprintf(buffer, size, "%lld", (long long)value.num);
    } else {
        snprintf(buffer, size, "%lld/%lld", (long long)value.num, (long long)value.den);
    }
    return buffer;
}

/* The measure of one bar over the notes of its written bass and inner voices: false when no note sounds in it. */
static bool bar_measure(const Written *written, const LabRequest *lab, long bar, Dimension dimension, Rational *out) {
    DimensionNote *notes = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PlanSpan span;
    Rational bar_start;
    Rational bar_end;
    rational_mul_int(written->bar_length, bar, &bar_start);
    rational_add(bar_start, written->bar_length, &bar_end);
    span.start = bar_start;
    span.length = written->bar_length;
    for (int lane = 0; lane < 4; lane++) {
        PlanLaneKind kind = lane == 0 ? PLAN_LANE_BASS : PLAN_LANE_INNER_VOICE;
        int voice = lane == 0 ? 0 : lane;
        size_t found = plan_lane_find(written->plan, kind, voice, span, NULL, 0);
        LaneSpanId *ids = found ? calloc(found, sizeof(*ids)) : NULL;
        if (found && !ids) {
            free(notes);
            return false;
        }
        plan_lane_find(written->plan, kind, voice, span, ids, found);
        for (size_t s = 0; s < found; s++) {
            PlanSpan note_span = plan_lane_span_span(written->plan, ids[s]);
            const PlanNotes *payload = &plan_lane_span_payload(written->plan, ids[s])->notes;
            for (size_t n = 0; n < payload->note_count; n++) {
                Rational start;
                Rational end;
                Rational from_bar;
                if (rational_add(note_span.start, payload->notes[n].offset, &start) != RATIONAL_OK || rational_add(start, payload->notes[n].duration, &end) != RATIONAL_OK ||
                    rational_cmp(end, bar_start) <= 0 || rational_cmp(start, bar_end) >= 0) {
                    continue;
                }
                if (rational_cmp(start, bar_start) < 0) {
                    start = bar_start;
                }
                if (rational_cmp(end, bar_end) > 0) {
                    end = bar_end;
                }
                if (count == capacity) {
                    size_t grown_capacity = capacity ? capacity * 2 : 64;
                    DimensionNote *grown = realloc(notes, grown_capacity * sizeof(*notes));
                    if (grown == NULL) {
                        free(notes);
                        free(ids);
                        return false;
                    }
                    notes = grown;
                    capacity = grown_capacity;
                }
                rational_sub(start, bar_start, &from_bar);
                notes[count].onset = from_bar;
                rational_sub(end, start, &notes[count].duration);
                notes[count].pitch = payload->notes[n].midi;
                count++;
            }
        }
        free(ids);
    }
    DimensionSpan measured = {notes, count, written->bar_length, rational_ratio(1, (long long)lab->meter.beat_unit), dimension == DIMENSION_BRIGHTNESS,
                              spelled_key_tonic_pitch_class(&lab->key), NULL};
    bool ok = count > 0 && dimension_measure(dimension, &measured, out) == DIMENSION_OK;
    free(notes);
    return ok;
}

/* "measures": [{"bar", "tension", "brightness"}], a value null for a bar that has none. */
static cJSON *measures_json(const Written *written, const LabRequest *lab) {
    cJSON *list = cJSON_CreateArray();
    for (long bar = 0; list != NULL && bar < lab->bars; bar++) {
        cJSON *row = cJSON_CreateObject();
        Rational value;
        char text[48];
        bool ok = row != NULL && cJSON_AddNumberToObject(row, "bar", (double)(bar + 1)) != NULL;
        for (int d = 0; ok && d < 2; d++) {
            bool tension = d == 0;
            bool measured = bar_measure(written, lab, bar, tension ? DIMENSION_TENSION : DIMENSION_BRIGHTNESS, &value);
            ok = measured ? cJSON_AddStringToObject(row, tension ? "tension" : "brightness", rational_text(value, text, sizeof(text))) != NULL
                          : cJSON_AddNullToObject(row, tension ? "tension" : "brightness") != NULL;
        }
        if (!ok || !cJSON_AddItemToArray(list, row)) {
            cJSON_Delete(row);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* "harmony": {"key", "material", "with", "meter", "bars", "chords": [{"bar", "text"}], "stages": [{"chord", "units"}]}. */
static cJSON *harmony_json(const Written *written, const LabRequest *lab) {
    char key[24];
    char meter[16];
    util_key_name(spelled_key_tonic_pitch_class(&lab->key), lab->key.material, key, sizeof(key));
    snprintf(meter, sizeof(meter), "%d/%d", lab->meter.beats_per_bar, lab->meter.beat_unit);
    cJSON *object = cJSON_CreateObject();
    cJSON *chords = object != NULL ? cJSON_CreateArray() : NULL;
    cJSON *stages = chords != NULL ? cJSON_CreateArray() : NULL;
    bool ok = stages != NULL && cJSON_AddStringToObject(object, "key", key) && cJSON_AddStringToObject(object, "material", lab->key.material->id) &&
              (lab->with == NULL || cJSON_AddStringToObject(object, "with", lab->with->id)) && cJSON_AddStringToObject(object, "meter", meter) &&
              cJSON_AddNumberToObject(object, "bars", (double)lab->bars) && cJSON_AddItemToObject(object, "chords", chords) &&
              cJSON_AddItemToObject(object, "stages", stages);
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(chords);
        cJSON_Delete(stages);
        return NULL;
    }
    for (long bar = 0; ok && bar < lab->bars; bar++) {
        cJSON *row = cJSON_CreateObject();
        ok = row != NULL && cJSON_AddNumberToObject(row, "bar", (double)(bar + 1)) && cJSON_AddStringToObject(row, "text", written->bar_text[bar]) &&
             cJSON_AddItemToArray(chords, row);
        if (!ok) {
            cJSON_Delete(row);
        }
    }
    size_t units = written->choice.count;
    for (size_t u = 0; ok && u < units;) {
        size_t run = 1;
        while (u + run < units && written->choice.chords[u + run].root_semitones == written->choice.chords[u].root_semitones &&
               written->choice.chords[u + run].quality == written->choice.chords[u].quality) {
            run++;
        }
        cJSON *stage = cJSON_CreateObject();
        ok = stage != NULL && cJSON_AddStringToObject(stage, "chord", written->choice.chords[u].numeral) && cJSON_AddNumberToObject(stage, "units", (double)run) &&
             cJSON_AddItemToArray(stages, stage);
        if (!ok) {
            cJSON_Delete(stage);
        }
        u += run;
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord rows of the response: one for each harmony span, "bar", "beat", "chord", "function" and the bass notes under it. */
static bool rows_json(const Written *written, const LabRequest *lab, cJSON *chords) {
    LaneSpanId ids[LAB_CHORDS_MAX * 4];
    size_t count = plan_lane_find(written->plan, PLAN_LANE_HARMONY, 0, written->span, ids, LAB_CHORDS_MAX * 4);
    for (size_t i = 0; i < count && i < LAB_CHORDS_MAX * 4; i++) {
        PlanSpan span = plan_lane_span_span(written->plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(written->plan, ids[i])->harmony;
        Rational in_bar;
        long long bar = 0;
        rational_div(span.start, written->bar_length, &in_bar);
        rational_floor(in_bar, &bar);
        Rational bar_start;
        Rational offset;
        rational_mul_int(written->bar_length, bar, &bar_start);
        rational_sub(span.start, bar_start, &offset);
        char bass[48] = "";
        LaneSpanId basses[2];
        if (plan_lane_find(written->plan, PLAN_LANE_BASS, 0, span, basses, 2) >= 1) {
            const PlanNotes *notes = &plan_lane_span_payload(written->plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof(bass); n++) {
                char name[8];
                util_pitch_name(notes->notes[n].midi, false, name, sizeof(name));
                used += (size_t)snprintf(bass + used, sizeof(bass) - used, n ? " %s" : "%s", name);
            }
        }
        cJSON *row = cJSON_CreateObject();
        if (row == NULL || !cJSON_AddNumberToObject(row, "bar", (double)(bar + 1)) ||
            !cJSON_AddNumberToObject(row, "beat", (double)(offset.num * lab->meter.beat_unit / (long long)offset.den) + 1) ||
            !cJSON_AddStringToObject(row, "chord", harmony->roman_numeral) ||
            !cJSON_AddStringToObject(row, "function", util_function_name(harmony->has_function, harmony->function)) || !cJSON_AddStringToObject(row, "bass", bass) ||
            !cJSON_AddItemToArray(chords, row)) {
            cJSON_Delete(row);
            return false;
        }
    }
    return true;
}

/* Adds "heard", "chords", "measures", "harmony" and, when asked, "plan" of one progression to object. */
static UrsatzStatus add_result(const KnowledgeSnapshot *knowledge, cJSON *object, const LabRequest *lab, const Written *written, UrsatzError *error) {
    cJSON *chords = cJSON_CreateArray();
    CadenceType heard;
    bool at_end;
    bool heard_cadence = lab->has_cadence && harmony_hear_cadence(written->plan, written->span, harmony_key_mode(&lab->key), lab->cadence, &heard, &at_end) && at_end;
    bool ok = chords != NULL && (heard_cadence ? cJSON_AddStringToObject(object, "heard", cadence_type_name(heard)) != NULL : cJSON_AddNullToObject(object, "heard") != NULL) &&
              cJSON_AddItemToObject(object, "chords", chords) && rows_json(written, lab, chords);
    cJSON *measures = ok ? measures_json(written, lab) : NULL;
    cJSON *harmony = measures != NULL ? harmony_json(written, lab) : NULL;
    if (!ok || measures == NULL || harmony == NULL || !cJSON_AddItemToObject(object, "measures", measures) || !cJSON_AddItemToObject(object, "harmony", harmony)) {
        if (measures != NULL && cJSON_GetObjectItemCaseSensitive(object, "measures") != measures) {
            cJSON_Delete(measures);
        }
        if (harmony != NULL && cJSON_GetObjectItemCaseSensitive(object, "harmony") != harmony) {
            cJSON_Delete(harmony);
        }
        return api_fail_alloc(error);
    }
    if (lab->plans) {
        cJSON *plan = api_piece_plan_object(knowledge, written->plan);
        if (plan == NULL || !cJSON_AddItemToObject(object, "plan", plan)) {
            cJSON_Delete(plan);
            return api_fail_alloc(error);
        }
    }
    return URSATZ_OK;
}

static UrsatzStatus build_lab_document(const KnowledgeSnapshot *knowledge, const LabRequest *lab, Written *written, size_t made, unsigned long seed, cJSON **result,
                                       UrsatzError *error) {
    char key[24];
    char meter[16];
    util_key_name(spelled_key_tonic_pitch_class(&lab->key), lab->key.material, key, sizeof(key));
    snprintf(meter, sizeof(meter), "%d/%d", lab->meter.beats_per_bar, lab->meter.beat_unit);
    cJSON *document = cJSON_CreateObject();
    bool ok = document && cJSON_AddStringToObject(document, "key", key) && cJSON_AddStringToObject(document, "meter", meter) &&
              cJSON_AddNumberToObject(document, "bars", (double)lab->bars) &&
              cJSON_AddStringToObject(document, "cadence", lab->has_cadence ? plan_cadence_name(lab->cadence) : "none") && cJSON_AddNumberToObject(document, "seed", (double)seed);
    UrsatzStatus status = ok ? URSATZ_OK : api_fail_alloc(error);
    if (status == URSATZ_OK && lab->suggest > 0) {
        cJSON *list = cJSON_AddArrayToObject(document, "alternatives");
        for (size_t k = 0; list && k < made && status == URSATZ_OK; k++) {
            cJSON *entry = cJSON_CreateObject();
            if (!entry || !cJSON_AddItemToArray(list, entry)) {
                cJSON_Delete(entry);
                status = api_fail_alloc(error);
                break;
            }
            status = add_result(knowledge, entry, lab, &written[k], error);
        }
        if (!list) {
            status = api_fail_alloc(error);
        }
        if (status == URSATZ_OK && made < lab->suggest) {
            char notice[96];
            cJSON *notices = cJSON_AddArrayToObject(document, "notices");
            snprintf(notice, sizeof(notice), "only %zu alternative%s found for this request", made, made == 1 ? "" : "s");
            if (!notices || !cJSON_AddItemToArray(notices, cJSON_CreateString(notice))) {
                status = api_fail_alloc(error);
            }
        }
    } else if (status == URSATZ_OK) {
        status = add_result(knowledge, document, lab, &written[0], error);
    }
    if (status != URSATZ_OK) {
        cJSON_Delete(document);
        return status;
    }
    *result = document;
    return URSATZ_OK;
}

/* --- the entry ----------------------------------------------------------------------------------------------- */

UrsatzStatus api_progression_lab(const ComposerContext *context, const cJSON *request, const UrsatzJob *job, ComposerResponse *out) {
    LabRequest *lab = calloc(1, sizeof(*lab));
    Written *written = calloc(LAB_SUGGEST_MAX, sizeof(*written));
    if (lab == NULL || written == NULL) {
        free(lab);
        free(written);
        return api_fail_alloc(&out->error);
    }
    UrsatzStatus status = read_lab(request, context, lab, &out->error);
    size_t made = 0;
    unsigned long seed = lab->seed;
    if (status == URSATZ_OK && lab->suggest > 0) {
        for (size_t k = 0; k < lab->suggest && status == URSATZ_OK; k++) {
            bool ok;
            if (job_step(job, "progression", (unsigned)k, (unsigned)lab->suggest)) {
                status = api_fail(&out->error, URSATZ_ERR_CANCELLED, "", "cancelled");
                break;
            }
            status = make_lab(lab, job, lab->seed, k, &written[made], &ok, &out->error);
            made += ok;
        }
    } else if (status == URSATZ_OK) {
        unsigned long first = lab->seed;
        for (int attempt = 0; attempt < API_SEED_TRIES && made == 0 && status == URSATZ_OK; attempt++) {
            bool ok;
            if (job_step(job, "progression", (unsigned)attempt, API_SEED_TRIES)) {
                status = api_fail(&out->error, URSATZ_ERR_CANCELLED, "", "cancelled");
                break;
            }
            seed = (first + (unsigned long)attempt) & 0xFFFFFFFFUL;
            status = make_lab(lab, job, seed, 0, &written[0], &ok, &out->error);
            made += ok;
        }
    }
    if (status == URSATZ_OK && made == 0) {
        status = api_fail(&out->error, URSATZ_ERR_UNSATISFIABLE, "", "no valid progression for the palette in %s, %s, %ld bars from seeds %lu to %lu", lab->key_text, lab->meter_text, lab->bars,
                          lab->seed, (lab->seed + API_SEED_TRIES - 1) & 0xFFFFFFFFUL);
    }
    if (status == URSATZ_OK) {
        cJSON *document = NULL;
        status = build_lab_document(context->knowledge, lab, written, made, seed, &document, &out->error);
        if (status == URSATZ_OK) {
            status = api_render_outputs(context->knowledge, written[0].plan, &lab->outputs, document, out);
            if (status != URSATZ_OK) {
                cJSON_Delete(document);
            } else {
                status = api_respond_json(out, document);
            }
        }
    }
    for (size_t i = 0; i < LAB_SUGGEST_MAX; i++) {
        plan_destroy(written[i].plan);
    }
    free(written);
    free(lab);
    return status;
}
