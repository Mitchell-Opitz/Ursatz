#include <ursatz_composer/form_hear.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <identity/entity_id.h>
#include <standard-form/analysis/analysis_theme.h>
#include <standard-form/phrase_level_form.h>
#include <time/duration.h>
#include <time/musical_time.h>
#include <time/temporal_anchor.h>
#include <time/time_span.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

#include "form/form_internal.h"
#include "harmony/harmony_internal.h"

/* The spans the adapter's input borrows, kept until it is done. */
typedef struct {
    TimeSpan **items;
    size_t count;
    size_t capacity;
} SpanPool;

typedef struct {
    RenderedPlan rendered;
    HarmonyCadences cadences;
    SpanPool spans;
    StandardFormInput input;
    bool input_ready;
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    StandardFormThemes themes;
} HearRun;

const char *form_hear_result_name(FormHearResult result) {
    switch (result) {
    case FORM_HEAR_OK:
        return "ok";
    case FORM_HEAR_ERR_NULL:
        return "null argument";
    case FORM_HEAR_ERR_ALLOC:
        return "out of memory";
    case FORM_HEAR_ERR_NOT_WRITTEN:
        return "lane not written";
    case FORM_HEAR_ERR_METER:
        return "meter";
    case FORM_HEAR_ERR_ANALYSIS:
        return "analysis failed";
    }
    return "unknown";
}

const char *form_hear_form_name(int form) {
    return phrase_level_form_name((PhraseLevelForm)form);
}

void form_heard_destroy(FormHeard *heard) {
    if (!heard) {
        return;
    }
    free(heard->themes);
    free(heard->cadences);
    memset(heard, 0, sizeof(*heard));
}

static void run_destroy(HearRun *run) {
    standard_form_themes_destroy(&run->themes);
    standard_form_segmentation_destroy(&run->segmentation);
    standard_form_phrases_destroy(&run->phrases);
    if (run->input_ready) {
        standard_form_input_destroy(&run->input);
    }
    for (size_t i = 0; i < run->spans.count; i++) {
        time_span_destroy(run->spans.items[i]);
    }
    free(run->spans.items);
    harmony_cadences_destroy(&run->cadences);
    rendered_plan_destroy(&run->rendered);
}

static FormHearResult result_of(StandardFormAnalysisStatus status) {
    switch (status) {
    case STANDARD_FORM_ANALYSIS_OK:
        return FORM_HEAR_OK;
    case STANDARD_FORM_ANALYSIS_ERR_ALLOC:
        return FORM_HEAR_ERR_ALLOC;
    case STANDARD_FORM_ANALYSIS_ERR_METER:
        return FORM_HEAR_ERR_METER;
    default:
        return FORM_HEAR_ERR_ANALYSIS;
    }
}

/* A span [start, end) kept by the pool. NULL when out of memory. */
static const TimeSpan *pool_span(SpanPool *pool, PlanTime start, PlanTime end) {
    if (pool->count == pool->capacity) {
        size_t capacity = pool->capacity ? pool->capacity * 2 : 64;
        TimeSpan **items = realloc(pool->items, capacity * sizeof(*items));
        if (!items) {
            return NULL;
        }
        pool->items = items;
        pool->capacity = capacity;
    }
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    if (musical_time_create(start.num, start.den, &from) == MUSICAL_TIME_OK &&
        musical_time_create(end.num, end.den, &to) == MUSICAL_TIME_OK &&
        time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK) {
        pool->items[pool->count++] = span;
    }
    musical_time_destroy(to);
    musical_time_destroy(from);
    return span;
}

/* True when a span of the lane over range is Constrained. */
static bool lane_open(const PiecePlan *plan, PlanLaneKind lane, PlanSpan range) {
    size_t count = plan_lane_find(plan, lane, 0, range, NULL, 0);
    if (count == 0) {
        return false;
    }
    LaneSpanId *ids = malloc(count * sizeof(*ids));
    if (!ids) {
        return true;
    }
    plan_lane_find(plan, lane, 0, range, ids, count);
    bool open = false;
    for (size_t i = 0; i < count && !open; i++) {
        open = plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED;
    }
    free(ids);
    return open;
}

/*
 * The scale degree (1 to 7) and alteration of a MIDI pitch in the key. In major a chromatic pitch is the flat of the
 * degree above, but the raised fourth; in minor the degrees are the natural minor's, with the raised sixth and seventh
 * and the raised fourth, the lowered second.
 */
static void degree_of(PlanKey key, int midi, int *degree, int *alteration) {
    static const int MAJOR[12][2] = {{1, 0}, {2, -1}, {2, 0}, {3, -1}, {3, 0}, {4, 0},
                                     {4, 1}, {5, 0},  {6, -1}, {6, 0}, {7, -1}, {7, 0}};
    static const int MINOR[12][2] = {{1, 0}, {2, -1}, {2, 0}, {3, 0}, {3, 1}, {4, 0},
                                     {4, 1}, {5, 0},  {6, 0},  {6, 1}, {7, 0}, {7, 1}};
    int relative = ((midi - key.tonic_pitch_class) % 12 + 12) % 12;
    const int(*table)[2] = key.mode == PLAN_MODE_MINOR ? MINOR : MAJOR;
    *degree = table[relative][0];
    *alteration = table[relative][1];
}

static PlanTime time_of(const MusicalTime *time) {
    PlanTime out = {0, 1};
    plan_time_make(musical_time_numerator(time), musical_time_denominator(time), &out);
    return out;
}

/* The key lane over the range, in order: each span's extent and key. */
#define MAX_KEY_SPANS 32

typedef struct {
    PlanSpan span[MAX_KEY_SPANS];
    PlanKey key[MAX_KEY_SPANS];
    size_t count;
} KeyMap;

/* Reads the key spans over the range; they must follow each other and hold all of it. False otherwise. */
static bool key_map_read(const PiecePlan *plan, PlanSpan range, KeyMap *map) {
    LaneSpanId ids[MAX_KEY_SPANS];
    PlanTime range_end;
    PlanTime at = range.start;
    map->count = plan_lane_find(plan, PLAN_LANE_KEY, 0, range, ids, MAX_KEY_SPANS);
    if (map->count == 0 || map->count > MAX_KEY_SPANS || plan_span_end(range, &range_end) != PLAN_OK) {
        return false;
    }
    for (size_t i = 0; i < map->count; i++) {
        PlanTime end;
        map->span[i] = plan_lane_span_span(plan, ids[i]);
        map->key[i] = plan_lane_span_payload(plan, ids[i])->key;
        /* The first span may start before the range; each next one starts where the one before ends. */
        if (plan_span_end(map->span[i], &end) != PLAN_OK ||
            (i == 0 ? plan_time_compare(map->span[i].start, range.start) > 0
                    : plan_time_compare(map->span[i].start, at) != 0)) {
            return false;
        }
        at = end;
    }
    return plan_time_compare(at, range_end) >= 0;
}

/* The key at a time of the range. */
static PlanKey key_at(const KeyMap *map, PlanTime time) {
    for (size_t i = 0; i < map->count; i++) {
        PlanTime end;
        if (plan_span_end(map->span[i], &end) == PLAN_OK && plan_time_compare(time, end) < 0) {
            return map->key[i];
        }
    }
    return map->key[map->count - 1];
}

/* One rendered note of a part over the span. */
typedef struct {
    size_t event;
    PlanTime start;
    PlanTime end;
    int midi;
} HeardNote;

/* The notes of the lane's part that start in the span, in time order (the render's). */
static bool collect_notes(const RenderedPlan *rendered, PlanLaneKind lane, PlanSpan span, HeardNote **out,
                          size_t *count) {
    PlanTime span_end;
    plan_span_end(span, &span_end);
    *out = NULL;
    *count = 0;
    if (rendered->count == 0) {
        return true;
    }
    HeardNote *notes = malloc(rendered->count * sizeof(*notes));
    if (!notes) {
        return false;
    }
    for (size_t i = 0; i < rendered->count; i++) {
        const RenderedPart *part = &rendered->parts[rendered->part_index[i]];
        if (part->lane != lane || part->voice != 0) {
            continue;
        }
        const NoteEvent *event = rendered->events[i];
        PlanTime start = time_of(temporal_anchor_position(note_event_temporal_anchor(event)));
        if (plan_time_compare(start, span.start) < 0 || plan_time_compare(start, span_end) >= 0) {
            continue;
        }
        const Duration *duration = note_event_duration(event);
        PlanTime length = {0, 1};
        plan_time_make(duration_numerator(duration), duration_denominator(duration), &length);
        HeardNote *note = &notes[(*count)++];
        note->event = i;
        note->start = start;
        plan_time_add(start, length, &note->end);
        note->midi = (int)pitch_numeric_value_position_numerator(rendered->pitch_values[i]);
    }
    *out = notes;
    return true;
}

/* Adds a part's notes to the input with their degrees: the soprano, or the bass. */
static FormHearResult add_line(HearRun *run, const KeyMap *keys, const HeardNote *notes, size_t count, bool bass) {
    for (size_t i = 0; i < count; i++) {
        const NoteEvent *event = run->rendered.events[notes[i].event];
        const TimeSpan *span = pool_span(&run->spans, notes[i].start, notes[i].end);
        if (!span) {
            return FORM_HEAR_ERR_ALLOC;
        }
        const char *id = entity_id_value(note_event_id(event));
        StandardFormAnalysisStatus status =
            bass ? standard_form_input_add_bass_note(&run->input, id, event, span, NULL, notes[i].midi)
                 : standard_form_input_add_note(&run->input, id, event, span, NULL, notes[i].midi);
        int degree;
        int alteration;
        /* The degree is in the key under the note's start. */
        degree_of(key_at(keys, notes[i].start), notes[i].midi, &degree, &alteration);
        if (status == STANDARD_FORM_ANALYSIS_OK) {
            status = standard_form_input_set_degree(&run->input, bass, i, degree, alteration);
        }
        if (status != STANDARD_FORM_ANALYSIS_OK) {
            return result_of(status);
        }
    }
    return FORM_HEAR_OK;
}

/* The soprano degree over each chord: the melody note sounding at its start, or 0 for none. */
static void soprano_over_chords(const PiecePlan *plan, const KeyMap *keys, const LaneSpanId *chords, size_t chord_count,
                                const HeardNote *melody, size_t melody_count, int *soprano) {
    for (size_t c = 0; c < chord_count; c++) {
        PlanTime start = plan_lane_span_span(plan, chords[c]).start;
        soprano[c] = 0;
        for (size_t n = 0; n < melody_count; n++) {
            if (plan_time_compare(melody[n].start, start) <= 0 && plan_time_compare(start, melody[n].end) < 0) {
                int alteration;
                degree_of(key_at(keys, melody[n].start), melody[n].midi, &soprano[c], &alteration);
            }
        }
    }
}

/* The cadences heard and the chord segments, into the input. */
static FormHearResult add_harmony(HearRun *run, const PiecePlan *plan, const LaneSpanId *chords, size_t count) {
    for (size_t i = 0; i < run->cadences.count; i++) {
        const HarmonyCadenceResult *cadence = &run->cadences.items[i];
        if (cadence->arrival_first >= count || cadence->arrival_last >= count) {
            return FORM_HEAR_ERR_ANALYSIS;
        }
        PlanTime end;
        plan_span_end(plan_lane_span_span(plan, chords[cadence->arrival_last]), &end);
        const TimeSpan *arrival =
            pool_span(&run->spans, plan_lane_span_span(plan, chords[cadence->arrival_first]).start, end);
        if (!arrival) {
            return FORM_HEAR_ERR_ALLOC;
        }
        char id[32];
        snprintf(id, sizeof(id), "cadence-%zu", i);
        StandardFormAnalysisStatus status =
            standard_form_input_add_cadence(&run->input, id, arrival, cadence->type,
                                            harmony_cadence_strength(cadence->type), cadence->post_cadential, 0.0);
        if (status != STANDARD_FORM_ANALYSIS_OK) {
            return result_of(status);
        }
    }
    for (size_t i = 0; i < count; i++) {
        PlanSpan chord = plan_lane_span_span(plan, chords[i]);
        PlanTime end;
        plan_span_end(chord, &end);
        const TimeSpan *span = pool_span(&run->spans, chord.start, end);
        if (!span) {
            return FORM_HEAR_ERR_ALLOC;
        }
        char id[32];
        snprintf(id, sizeof(id), "chord-%zu", i);
        StandardFormAnalysisStatus status = standard_form_input_add_segment(
            &run->input, id, span, (int)plan_lane_span_payload(plan, chords[i])->harmony.function, 0);
        if (status != STANDARD_FORM_ANALYSIS_OK) {
            return result_of(status);
        }
    }
    return FORM_HEAR_OK;
}

/* The cadences the recognizer heard, with where each arrives. */
static FormHearResult copy_cadences(const HearRun *run, const PiecePlan *plan, const LaneSpanId *chords,
                                    FormHeard *out) {
    if (run->cadences.count == 0) {
        return FORM_HEAR_OK;
    }
    out->cadences = calloc(run->cadences.count, sizeof(*out->cadences));
    if (!out->cadences) {
        return FORM_HEAR_ERR_ALLOC;
    }
    for (size_t i = 0; i < run->cadences.count; i++) {
        out->cadences[i].arrival = plan_lane_span_span(plan, chords[run->cadences.items[i].arrival_first]).start;
        out->cadences[i].type = run->cadences.items[i].type;
    }
    out->cadence_count = run->cadences.count;
    return FORM_HEAR_OK;
}

/* The adapter's themes as heard themes. */
static FormHearResult copy_themes(const HearRun *run, FormHeard *out) {
    out->phrase_count = run->phrases.count;
    if (run->themes.count == 0) {
        return FORM_HEAR_OK;
    }
    out->themes = calloc(run->themes.count, sizeof(*out->themes));
    if (!out->themes) {
        return FORM_HEAR_ERR_ALLOC;
    }
    int first_measure = meter_map_first_measure_number(run->rendered.meter_map);
    for (size_t i = 0; i < run->themes.count; i++) {
        const StandardFormThemeClaim *claim = &run->themes.items[i].claim;
        FormHeardTheme *theme = &out->themes[i];
        theme->form = claim->form;
        theme->beginning = claim->beginning;
        theme->ending = claim->ending;
        theme->commonality = claim->commonality;
        theme->first_bar = (size_t)(claim->first_measure - first_measure);
        theme->bars = (size_t)(claim->last_measure - claim->first_measure + 1);
        theme->phrase_count = (size_t)claim->phrase_count;
        theme->first_cadence = claim->first_cadence_type;
        theme->second_cadence = claim->second_cadence_type;
        theme->archetypal = claim->archetypal != 0;
        theme->has_fragmentation = claim->has_fragmentation != 0;
    }
    out->count = run->themes.count;
    return FORM_HEAR_OK;
}

/* Everything after the checks: the lanes as the adapter's input, then its three steps. */
static FormHearResult hear(HearRun *run, const PiecePlan *plan, PlanSpan span, const KeyMap *keys,
                           const PlanTime *section_starts, size_t section_count, FormHeard *out) {
    PlanRenderResult rendered = plan_render(plan, &run->rendered);
    if (rendered != PLAN_RENDER_OK) {
        return rendered == PLAN_RENDER_ERR_ALLOC ? FORM_HEAR_ERR_ALLOC : FORM_HEAR_ERR_ANALYSIS;
    }
    if (!run->rendered.meter_map) {
        return FORM_HEAR_ERR_METER;
    }
    size_t chord_count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, NULL, 0);
    if (chord_count == 0 || chord_count > HARMONY_MAX_CHORDS) {
        return FORM_HEAR_ERR_ANALYSIS;
    }
    LaneSpanId chords[HARMONY_MAX_CHORDS];
    int soprano[HARMONY_MAX_CHORDS];
    plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, chords, chord_count);

    HeardNote *melody = NULL;
    HeardNote *bass = NULL;
    size_t melody_count = 0;
    size_t bass_count = 0;
    FormHearResult result = FORM_HEAR_ERR_ALLOC;
    if (collect_notes(&run->rendered, PLAN_LANE_MELODY, span, &melody, &melody_count) &&
        collect_notes(&run->rendered, PLAN_LANE_BASS, span, &bass, &bass_count)) {
        soprano_over_chords(plan, keys, chords, chord_count, melody, melody_count, soprano);
        result = harmony_hear_lane_cadences(plan, span, keys->key[0].mode, soprano, chord_count, section_starts, section_count,
                                       &run->cadences)
                     ? FORM_HEAR_OK
                     : FORM_HEAR_ERR_ANALYSIS;
    }
    if (result == FORM_HEAR_OK) {
        standard_form_input_init(&run->input, run->rendered.meter_map);
        run->input_ready = true;
        result = add_harmony(run, plan, chords, chord_count);
    }
    if (result == FORM_HEAR_OK) {
        result = add_line(run, keys, melody, melody_count, false);
    }
    if (result == FORM_HEAR_OK) {
        result = add_line(run, keys, bass, bass_count, true);
    }
    free(melody);
    free(bass);
    if (result != FORM_HEAR_OK) {
        return result;
    }

    StandardFormConfig config = standard_form_config_default();
    config.include_bass = true;
    StandardFormAnalysisStatus status = standard_form_input_finish(&run->input);
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = standard_form_segment_phrases(&run->input, &config, form_boundary_detect, NULL, &run->phrases);
    }
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = standard_form_segment_ideas(&run->input, &run->phrases, &config, &run->segmentation);
    }
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = standard_form_identify_themes(&run->input, &run->phrases, &run->segmentation, &config,
                                               &run->themes);
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        return result_of(status);
    }
    FormHearResult copied = copy_cadences(run, plan, chords, out);
    return copied == FORM_HEAR_OK ? copy_themes(run, out) : copied;
}

FormHearResult form_hear(const PiecePlan *plan, PlanSpan span, FormHeard *out) {
    return form_hear_with_sections(plan, span, NULL, 0, out);
}

FormHearResult form_hear_with_sections(const PiecePlan *plan, PlanSpan span, const PlanTime *section_starts,
                                       size_t section_count, FormHeard *out) {
    if (!out) {
        return FORM_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return FORM_HEAR_ERR_NULL;
    }
    if (lane_open(plan, PLAN_LANE_MELODY, span) || lane_open(plan, PLAN_LANE_BASS, span) ||
        lane_open(plan, PLAN_LANE_HARMONY, span)) {
        return FORM_HEAR_ERR_NOT_WRITTEN;
    }
    PhraseMetre metre;
    if (!harmony_read_metre(plan, span, &metre)) {
        return FORM_HEAR_ERR_METER;
    }
    /* The key lane holds the whole range, in one key or several: a degree is read in the key at its start. */
    KeyMap keys;
    if (!key_map_read(plan, span, &keys)) {
        return FORM_HEAR_ERR_ANALYSIS;
    }
    HearRun run;
    memset(&run, 0, sizeof(run));
    FormHearResult result = hear(&run, plan, span, &keys, section_starts, section_count, out);
    run_destroy(&run);
    if (result != FORM_HEAR_OK) {
        form_heard_destroy(out);
    }
    return result;
}
