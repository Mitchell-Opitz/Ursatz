#include <ursatz_composer/analysis_load.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time/meter_map.h>
#include <time/musical_time.h>
#include <time/time_rational.h>
#include <time/time_signature.h>
#include <time/time_span.h>
#include <ursatz_analyzer/chord_sequence.h>
#include <ursatz_analyzer/version.h>
#include <ursatz_analyzer/result_chain.h>
#include <ursatz_chain/analysis_chain.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "util/plan_util.h"

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

static PlanResult plan_result_of_time(RationalResult result) {
    return result == RATIONAL_OK ? PLAN_OK : PLAN_ERR_BAD_TIME;
}

static bool rational_of_time(const MusicalTime *time, Rational *out) {
    return time && rational_of_musical_time(time, out) == RATIONAL_OK;
}

/* The span from start to end, false when end is not after start. */
static bool span_between(Rational start, Rational end, PlanSpan *out) {
    Rational negative_start = {-start.num, start.den};
    out->start = start;
    return plan_result_of_time(rational_add(end, negative_start, &out->length)) == PLAN_OK && out->length.num > 0;
}

static Rational later(Rational a, Rational b) {
    return rational_cmp(a, b) >= 0 ? a : b;
}

/* The time a measure (by MeterMap measure number) starts at, or ends at. */
static bool measure_time(const MeterMap *map, int number, bool end, Rational *out) {
    TimeSpan *span = NULL;
    if (meter_map_measure_span(map, number, &span) != METER_MAP_OK || !span) {
        return false;
    }
    bool ok = rational_of_time(end ? time_span_end(span) : time_span_start(span), out);
    time_span_destroy(span);
    return ok;
}

typedef struct {
    PiecePlan *plan;
    NodeId root;
    Notes *notes;
    const MeterMap *map;
    PlanSpan extent;
} Loader;

static PlanResult add_span(Loader *loader, PlanLaneKind lane, PlanSpan span, const PlanLanePayload *payload,
                           const char *path) {
    LaneSpanId id;
    PlanResult result = plan_add_lane_span(loader->plan, loader->root, lane, 0, span, payload, &id);
    if (result == PLAN_ERR_ALLOC) {
        return result;
    }
    if (result != PLAN_OK) {
        note(loader->notes, path, "cannot be placed on the timeline; skipped");
        return PLAN_OK;
    }
    plan_lane_span_set_state(loader->plan, id, PLAN_STATE_REALIZED);
    return PLAN_OK;
}

/* The extent: from 0 to the latest end of a chord, a key region, a phrase, a voice entry or the last note. */
static bool find_extent_end(const AnalyzerResult *result, const ImportedNotes *notes, const AnalyzerChord *chords,
                            size_t chord_count, Rational *end) {
    const AnalysisChain *chain = analyzer_result_chain(result);
    const MeterMap *map = notes->meter.meter_map;
    *end = (Rational){0, 1};
    if (notes->meter.end_time) {
        Rational last;
        if (rational_of_time(notes->meter.end_time, &last)) {
            *end = later(*end, last);
        }
    }
    for (size_t i = 0; i < chord_count; i++) {
        *end = later(*end, chords[i].end);
    }
    const CommonPracticeKeyRegions *regions = analysis_chain_key_regions(chain);
    for (size_t i = 0; regions && i < regions->region_count; i++) {
        Rational region_end;
        if (measure_time(map, regions->regions[i].last_measure, true, &region_end)) {
            *end = later(*end, region_end);
        }
    }
    return end->num > 0;
}

static PlanResult add_meter_span(Loader *loader, Rational start, Rational end, unsigned top, unsigned bottom) {
    PlanSpan span;
    if (!span_between(start, end, &span)) {
        return PLAN_OK;
    }
    PlanLanePayload payload = {0};
    payload.meter.beats_per_bar = (int)top;
    payload.meter.beat_unit = (int)bottom;
    return add_span(loader, PLAN_LANE_METER, span, &payload, "meter");
}

/* Regions with one signature are one span; a pickup measure belongs to the span of the measure after it. */
static PlanResult add_meter(Loader *loader, const ImportedNotes *notes) {
    const MeterMap *map = notes->meter.meter_map;
    size_t count = meter_map_region_count(map);
    Rational piece_end;
    plan_span_end(loader->extent, &piece_end);
    Rational start = {0, 1};
    bool open = false;
    unsigned top = 0;
    unsigned bottom = 0;
    for (size_t i = 0; i < count; i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        const TimeSignature *signature = region ? meter_map_region_time_signature(region) : NULL;
        if (!signature) {
            continue;
        }
        if (meter_map_region_is_pickup(region)) {
            note(loader->notes, "meter", "pickup measure joined to the meter of the measure after it");
            continue;
        }
        unsigned region_top = time_signature_top_number(signature);
        unsigned region_bottom = time_signature_bottom_number(signature);
        if (open && region_top == top && region_bottom == bottom) {
            continue;
        }
        Rational region_start;
        if (!rational_of_time(meter_map_region_start(region), &region_start)) {
            return PLAN_ERR_BAD_TIME;
        }
        if (open) {
            PlanResult added = add_meter_span(loader, start, region_start, top, bottom);
            if (added != PLAN_OK) {
                return added;
            }
            start = region_start;
        }
        open = true;
        top = region_top;
        bottom = region_bottom;
    }
    return open ? add_meter_span(loader, start, piece_end, top, bottom) : PLAN_OK;
}

static PlanResult add_keys(Loader *loader, const AnalysisChain *chain) {
    const CommonPracticeKeyRegions *regions = analysis_chain_key_regions(chain);
    for (size_t i = 0; regions && i < regions->region_count; i++) {
        const CommonPracticeKeyRegion *region = &regions->regions[i];
        char path[PLAN_JSON_PATH_MAX];
        snprintf(path, sizeof(path), "keys[%zu]", i);
        PlanLanePayload payload = {0};
        Rational start;
        Rational end;
        PlanSpan span;
        if (!common_practice_key_region_analysis_key(region, &payload.key) ||
            !measure_time(loader->map, region->first_measure, false, &start) ||
            !measure_time(loader->map, region->last_measure, true, &end) || !span_between(start, end, &span)) {
            note(loader->notes, path, "key region cannot be read; skipped");
            continue;
        }
        payload.key.id = NULL;
        PlanResult added = add_span(loader, PLAN_LANE_KEY, span, &payload, path);
        if (added != PLAN_OK) {
            return added;
        }
    }
    return PLAN_OK;
}

static PlanResult add_chords(Loader *loader, const AnalyzerChord *chords, size_t count) {
    for (size_t i = 0; i < count; i++) {
        char path[PLAN_JSON_PATH_MAX];
        snprintf(path, sizeof(path), "chords[%zu]", i);
        size_t length = chords[i].numeral ? strlen(chords[i].numeral) : 0;
        PlanSpan span;
        if (length == 0 || length >= PLAN_ROMAN_NUMERAL_MAX || !span_between(chords[i].start, chords[i].end, &span)) {
            note(loader->notes, path, "chord '%.40s' skipped", chords[i].numeral ? chords[i].numeral : "");
            continue;
        }
        PlanLanePayload payload = {0};
        memcpy(payload.harmony.roman_numeral, chords[i].numeral, length + 1);
        payload.harmony.has_function = false;
        PlanResult added = add_span(loader, PLAN_LANE_HARMONY, span, &payload, path);
        if (added != PLAN_OK) {
            return added;
        }
    }
    return PLAN_OK;
}

static PlanResult add_phrases(Loader *loader, const AnalysisChain *chain) {
    const StandardFormPhrases *phrases = analysis_chain_phrases(chain);
    if (!phrases || phrases->count == 0) {
        return PLAN_OK;
    }
    /* A phrase sits under a section: one section holds the whole piece's phrases (the result has no sections). */
    NodeId section;
    PlanResult made = plan_add_node(loader->plan, loader->root, PLAN_NODE_SECTION, loader->extent, "piece", &section);
    if (made != PLAN_OK) {
        return made == PLAN_ERR_ALLOC ? made : PLAN_OK;
    }
    plan_node_set_state(loader->plan, section, PLAN_STATE_REALIZED);
    Rational previous_end = loader->extent.start;
    Rational piece_end;
    plan_span_end(loader->extent, &piece_end);
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhrase *phrase = &phrases->items[i];
        char path[PLAN_JSON_PATH_MAX];
        snprintf(path, sizeof(path), "phrases[%zu]", i);
        Rational start;
        Rational end;
        PlanSpan span;
        if (!phrase->span || !rational_of_time(time_span_start(phrase->span), &start) ||
            !rational_of_time(time_span_end(phrase->span), &end) || !span_between(start, end, &span) ||
            rational_cmp(start, previous_end) < 0 || rational_cmp(end, piece_end) > 0) {
            note(loader->notes, path, "phrase overlaps the one before it or does not fit the piece; skipped");
            continue;
        }
        char name[32];
        snprintf(name, sizeof(name), "phrase %zu", i + 1);
        NodeId node;
        PlanResult added = plan_add_node(loader->plan, section, PLAN_NODE_PHRASE, span, name, &node);
        if (added == PLAN_ERR_ALLOC) {
            return added;
        }
        if (added != PLAN_OK) {
            note(loader->notes, path, "phrase cannot be placed on the timeline; skipped");
            continue;
        }
        plan_node_set_state(loader->plan, node, PLAN_STATE_REALIZED);
        previous_end = end;
    }
    return PLAN_OK;
}

/* One voice of the outer voices as a lane span over the whole piece: a note for each run of segments one note sounds in. */
static PlanResult add_voice(Loader *loader, const OuterVoiceEntry *entries, size_t count, PlanLaneKind lane) {
    PlanNote *voice = count ? calloc(count, sizeof(PlanNote)) : NULL;
    if (count && !voice) {
        return PLAN_ERR_ALLOC;
    }
    size_t used = 0;
    const EntityID *run_id = NULL;
    Rational run_end = {0, 1};
    for (size_t i = 0; i < count; i++) {
        const OuterVoiceEntry *entry = &entries[i];
        Rational start;
        Rational end;
        if (!entry->present || !entry->span || !entry->note_id || !entry->pitch ||
            !rational_of_time(time_span_start(entry->span), &start) ||
            !rational_of_time(time_span_end(entry->span), &end)) {
            run_id = NULL;
            continue;
        }
        if (run_id && entity_id_equals(run_id, entry->note_id) && rational_cmp(start, run_end) == 0 && used > 0) {
            Rational begin;
            rational_add(voice[used - 1].offset, loader->extent.start, &begin);
            PlanSpan whole;
            if (span_between(begin, end, &whole)) {
                voice[used - 1].duration = whole.length;
            }
            run_end = end;
            continue;
        }
        PlanSpan window;
        Rational negative = {-loader->extent.start.num, loader->extent.start.den};
        Rational offset;
        if (!span_between(start, end, &window) || rational_add(start, negative, &offset) != RATIONAL_OK) {
            run_id = NULL;
            continue;
        }
        int midi = (int)pitch_numeric_value_position_numerator(entry->pitch);
        if (midi < 0 || midi > 127) {
            run_id = NULL;
            continue;
        }
        voice[used++] = plan_note(midi, offset, window.length);
        run_id = entry->note_id;
        run_end = end;
    }
    PlanResult added = PLAN_OK;
    if (used > 0) {
        PlanLanePayload payload = {0};
        payload.notes.notes = voice;
        payload.notes.note_count = used;
        added = add_span(loader, lane, loader->extent, &payload, plan_lane_name(lane));
    } else {
        note(loader->notes, plan_lane_name(lane), "the result has no notes in this line; no lane written");
    }
    free(voice);
    return added;
}

static void note_unmapped_areas(Notes *notes, const AnalyzerResult *result) {
    static const struct {
        AnalyzerArea area;
        const char *name;
    } AREAS[] = {{ANALYZER_AREA_CADENCES, "cadences"},       {ANALYZER_AREA_THEMES, "themes"},
                 {ANALYZER_AREA_MOTIVES, "motives"},         {ANALYZER_AREA_TEXTURE, "texture"},
                 {ANALYZER_AREA_VOICE_LEADING, "voice_leading"}, {ANALYZER_AREA_SCHEMAS, "schemas"}};
    for (size_t i = 0; i < sizeof(AREAS) / sizeof(AREAS[0]); i++) {
        if (analyzer_result_claim_count(result, AREAS[i].area) > 0) {
            note(notes, AREAS[i].name, "area has claims the plan has no place for yet; not loaded");
        }
    }
}

PlanResult plan_from_analyzer_result(const AnalyzerResult *result, const ImportedNotes *notes, PiecePlan **out,
                                     AnalysisLoadReport *report) {
    if (out) {
        *out = NULL;
    }
    if (report) {
        report->notes = NULL;
        report->count = 0;
    }
    if (!result || !notes || !out) {
        return PLAN_ERR_NULL;
    }
    const AnalysisChain *chain = analyzer_result_chain(result);
    if (!chain || !notes->meter.meter_map) {
        return PLAN_ERR_FORMAT;
    }
    AnalyzerChord *chords = NULL;
    size_t chord_count = 0;
    AnalyzerStatus status = analyzer_chord_sequence(result, &chords, &chord_count);
    if (status != ANALYZER_OK) {
        return status == ANALYZER_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT;
    }
    const CommonPracticeKeyRegions *regions = analysis_chain_key_regions(chain);
    Rational end;
    if ((chord_count == 0 && (!regions || regions->region_count == 0)) ||
        !find_extent_end(result, notes, chords, chord_count, &end)) {
        analyzer_chord_sequence_free(chords, chord_count);
        return PLAN_ERR_FORMAT;
    }

    PiecePlan *plan = plan_create();
    Notes bookkeeping = {report, 0, false};
    PlanResult built = plan ? PLAN_OK : PLAN_ERR_ALLOC;
    Loader loader = {plan, PLAN_NODE_NONE, &bookkeeping, notes->meter.meter_map, {{0, 1}, end}};
    if (built == PLAN_OK) {
        built = plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, loader.extent, NULL, &loader.root);
    }
    if (built == PLAN_OK) {
        plan_node_set_state(plan, loader.root, PLAN_STATE_REALIZED);
        built = plan_set_analyzer_version(plan, ursatz_analyzer_version());
    }
    if (built == PLAN_OK) {
        built = add_meter(&loader, notes);
    }
    if (built == PLAN_OK) {
        built = add_keys(&loader, chain);
    }
    if (built == PLAN_OK) {
        built = add_chords(&loader, chords, chord_count);
    }
    if (built == PLAN_OK) {
        built = add_phrases(&loader, chain);
    }
    const OuterVoiceLines *lines = analysis_chain_outer_voices(chain);
    if (built == PLAN_OK && lines) {
        built = add_voice(&loader, lines->soprano, lines->count, PLAN_LANE_MELODY);
    }
    if (built == PLAN_OK && lines) {
        built = add_voice(&loader, lines->bass, lines->count, PLAN_LANE_BASS);
    }
    analyzer_chord_sequence_free(chords, chord_count);
    if (built == PLAN_OK) {
        note_unmapped_areas(&bookkeeping, result);
        if (bookkeeping.failed) {
            built = PLAN_ERR_ALLOC;
        }
    }
    if (built != PLAN_OK) {
        plan_destroy(plan);
        analysis_load_report_destroy(report);
        return built == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT;
    }
    *out = plan;
    return PLAN_OK;
}
