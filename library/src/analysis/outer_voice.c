#include "analysis/outer_voice.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice_internal.h"
#include "time/temporal_anchor.h"

OuterVoiceConfig outer_voice_config_default(void) {
    OuterVoiceConfig config;
    config.gap_bridge_max_numerator = 1;
    config.gap_bridge_max_denominator = 1;
    config.register_guard_semitones = 12;
    config.bridge_gaps = true;
    config.guard_soprano_register = true;
    config.guard_bass_register = true;
    return config;
}

static void entry_release(OuterVoiceEntry *entry) {
    entity_id_destroy(entry->note_id);
    pitch_numeric_value_destroy(entry->pitch);
    time_span_destroy(entry->span);
}

void outer_voice_lines_destroy(OuterVoiceLines *lines) {
    if (lines == NULL) {
        return;
    }
    for (size_t i = 0; i < lines->count; i++) {
        if (lines->soprano != NULL) {
            entry_release(&lines->soprano[i]);
        }
        if (lines->bass != NULL) {
            entry_release(&lines->bass[i]);
        }
    }
    free(lines->soprano);
    free(lines->bass);
    lines->soprano = NULL;
    lines->bass = NULL;
    lines->count = 0;
}

/* True when note `a` is the better pick than `b` for the soprano (highest) or the bass (lowest). */
static bool better_pick(const OuterNote *a, const OuterNote *b, bool soprano) {
    int by_pitch = rational_cmp(a->pitch, b->pitch);
    if (by_pitch != 0) {
        return soprano ? by_pitch > 0 : by_pitch < 0;
    }
    int by_onset = rational_cmp(a->onset, b->onset);
    if (by_onset != 0) {
        return by_onset > 0; /* the note attacked most recently */
    }
    return strcmp(a->id, b->id) < 0; /* then the lowest note id */
}

void outer_voice_pick_line(const OuterNote *notes, const OuterSegment *segments, size_t segment_count,
                           bool soprano, OuterNode *nodes) {
    for (size_t s = 0; s < segment_count; s++) {
        OuterNode node = {false, false, false, 0};
        for (size_t m = 0; m < segments[s].member_count; m++) {
            size_t candidate = segments[s].members[m];
            if (!node.present || better_pick(&notes[candidate], &notes[node.note], soprano)) {
                node.present = true;
                node.note = candidate;
            }
        }
        nodes[s] = node;
    }
}

/* --- smoothing --- */

/* How much higher `a` is than `b`, if it is: false when a <= b. */
static bool excess_above(Rational a, Rational b, Rational *out) {
    if (rational_cmp(a, b) <= 0) {
        return false;
    }
    return rational_sub(a, b, out) == 0;
}

OuterVoiceResult outer_voice_guard_register(const OuterNote *notes, const OuterSegment *segments,
                                             size_t segment_count, bool soprano, int semitones,
                                             Rational window_beats, OuterBeatLength beat_length,
                                             void *beat_context, OuterNode *nodes) {
    Rational limit = rational_ratio((unsigned long long)semitones, 1);
    bool have_previous = false;
    size_t previous = 0;
    for (size_t i = 0; i < segment_count; i++) {
        if (!nodes[i].present) {
            continue;
        }
        if (have_previous) {
            const OuterNote *before = &notes[nodes[previous].note];
            const OuterNote *now = &notes[nodes[i].note];
            /* Only a note that has ended can be the one "during a gap". */
            if (rational_cmp(before->release, segments[i].start) <= 0) {
                /* The soprano falling below, or the bass rising above, the previous note. */
                Rational jump = {0, 1};
                bool leaps = soprano ? excess_above(before->pitch, now->pitch, &jump)
                                     : excess_above(now->pitch, before->pitch, &jump);
                if (leaps && rational_cmp(jump, limit) > 0) {
                    Rational gap = {0, 1};
                    Rational beat = {0, 1};
                    Rational window = {0, 1};
                    if (rational_sub(segments[i].start, before->release, &gap) != RATIONAL_OK) {
                        return OUTER_VOICE_ERR_OVERFLOW;
                    }
                    if (!beat_length(beat_context, before->release, &beat)) {
                        return OUTER_VOICE_ERR_BEAT_GRID;
                    }
                    if (rational_mul(beat, window_beats, &window) != RATIONAL_OK) {
                        return OUTER_VOICE_ERR_OVERFLOW;
                    }
                    if (rational_cmp(gap, window) < 0) {
                        nodes[i].present = false;
                        nodes[i].guarded = true;
                        continue;
                    }
                }
            }
        }
        have_previous = true;
        previous = i;
    }
    return OUTER_VOICE_OK;
}

OuterVoiceResult outer_voice_mark_bridged_gaps(const OuterNote *notes, const OuterSegment *segments,
                                                size_t segment_count, Rational window_beats,
                                                OuterBeatLength beat_length, void *beat_context,
                                                OuterNode *nodes) {
    size_t i = 0;
    while (i < segment_count) {
        if (nodes[i].present) {
            i++;
            continue;
        }
        size_t run_start = i;
        while (i < segment_count && !nodes[i].present) {
            i++;
        }
        size_t run_end = i; /* one past the run; the next present entry, if any, is at run_end */
        if (run_start == 0 || run_end >= segment_count) {
            continue; /* an end of the line, not a gap between two notes */
        }

        const OuterNote *before = &notes[nodes[run_start - 1].note];
        Rational gap_start = before->release;
        if (rational_cmp(gap_start, segments[run_start].start) > 0) {
            gap_start = segments[run_start].start;
        }
        Rational gap = {0, 1};
        Rational beat = {0, 1};
        Rational window = {0, 1};
        if (rational_cmp(segments[run_end].start, gap_start) < 0) {
            continue;
        }
        if (rational_sub(segments[run_end].start, gap_start, &gap) != RATIONAL_OK) {
            return OUTER_VOICE_ERR_OVERFLOW;
        }
        if (!beat_length(beat_context, gap_start, &beat)) {
            return OUTER_VOICE_ERR_BEAT_GRID;
        }
        if (rational_mul(beat, window_beats, &window) != RATIONAL_OK) {
            return OUTER_VOICE_ERR_OVERFLOW;
        }
        if (rational_cmp(gap, window) < 0) {
            for (size_t k = run_start; k < run_end; k++) {
                nodes[k].bridged = true;
            }
        }
    }
    return OUTER_VOICE_OK;
}

/* The beat length from the injected beat grid: the length of the beat that holds `at`. */
typedef struct {
    const MeterMap *map;
    BeatGridProvider provider;
    void *provider_context;
} BeatGridContext;

static bool grid_beat_length(void *context, Rational at, Rational *out) {
    const BeatGridContext *grid = (const BeatGridContext *)context;
    MusicalTime *time = musical_time_from_rational_or_null(at);
    if (time == NULL) {
        return false;
    }
    int measure = 0;
    MeterMapResult indexed = meter_map_measure_index_at(grid->map, time, &measure);
    musical_time_destroy(time);
    TimeSpan *span = NULL;
    if (indexed != METER_MAP_OK || meter_map_measure_span(grid->map, measure, &span) != METER_MAP_OK) {
        return false;
    }
    Rational measure_end;
    bool measure_ok = rational_of_musical_time(time_span_end(span), &measure_end) == RATIONAL_OK;
    time_span_destroy(span);
    if (!measure_ok) {
        return false;
    }

    MusicalTime **starts = NULL;
    size_t count = 0;
    if (!grid->provider(grid->map, measure, grid->provider_context, &starts, &count) || count == 0) {
        return false;
    }
    bool found = false;
    for (size_t k = 0; k < count && !found; k++) {
        Rational beat_start;
        Rational beat_end = measure_end;
        if (rational_of_musical_time(starts[k], &beat_start) != RATIONAL_OK ||
            (k + 1 < count && rational_of_musical_time(starts[k + 1], &beat_end) != RATIONAL_OK)) {
            break;
        }
        if (rational_cmp(beat_start, at) <= 0 && rational_cmp(at, beat_end) < 0) {
            found = rational_sub(beat_end, beat_start, out) == 0;
            break;
        }
    }
    for (size_t k = 0; k < count; k++) {
        musical_time_destroy(starts[k]);
    }
    free(starts);
    return found;
}

typedef struct {
    const char *id;
    size_t index;
} NoteKey;

static int compare_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

static OuterVoiceResult build_entry(const OuterNode *node, size_t segment_index, const OuterSegment *segment,
                                    NoteEvent *const *notes, PitchNumericValue *const *pitch_values,
                                    OuterVoiceEntry *out) {
    memset(out, 0, sizeof(*out));
    out->segment_index = segment_index;
    out->present = node->present;
    out->bridged = node->bridged;
    out->guarded = node->guarded;

    MusicalTime *start = musical_time_from_rational_or_null(segment->start);
    MusicalTime *end = musical_time_from_rational_or_null(segment->end);
    if (start == NULL || end == NULL) {
        musical_time_destroy(start);
        musical_time_destroy(end);
        return OUTER_VOICE_ERR_OVERFLOW;
    }
    TimeSpanResult made = time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &out->span);
    musical_time_destroy(start);
    musical_time_destroy(end);
    if (made != TIME_SPAN_OK) {
        return OUTER_VOICE_ERR_ALLOC;
    }

    if (node->present) {
        if (entity_id_create(entity_id_value(note_event_id(notes[node->note])), &out->note_id) != ENTITY_ID_OK) {
            time_span_destroy(out->span);
            out->span = NULL;
            return OUTER_VOICE_ERR_ALLOC;
        }
        const PitchNumericValue *pitch = pitch_values[node->note];
        if (pitch_numeric_value_create(pitch_numeric_value_tuning_system(pitch),
                                       pitch_numeric_value_position_numerator(pitch),
                                       pitch_numeric_value_position_denominator(pitch),
                                       &out->pitch) != PITCH_NUMERIC_VALUE_OK) {
            entity_id_destroy(out->note_id);
            time_span_destroy(out->span);
            out->note_id = NULL;
            out->span = NULL;
            return OUTER_VOICE_ERR_ALLOC;
        }
    }
    return OUTER_VOICE_OK;
}

static OuterVoiceResult validate_config(const OuterVoiceConfig *config) {
    if (config->gap_bridge_max_denominator == 0 || config->register_guard_semitones < 0) {
        return OUTER_VOICE_ERR_BAD_CONFIG;
    }
    return OUTER_VOICE_OK;
}

OuterVoiceResult outer_voice_extract(const AdaptiveSonorities *segments, NoteEvent *const *notes,
                                      PitchNumericValue *const *pitch_values, size_t note_count, const MeterMap *map,
                                      BeatGridProvider provider, void *provider_context,
                                      const OuterVoiceConfig *config, OuterVoiceLines *out) {
    if (segments == NULL || out == NULL || map == NULL || provider == NULL || (note_count > 0 && (notes == NULL || pitch_values == NULL))) {
        return OUTER_VOICE_ERR_NULL_ARGUMENT;
    }
    OuterVoiceConfig effective = config != NULL ? *config : outer_voice_config_default();
    OuterVoiceResult checked = validate_config(&effective);
    if (checked != OUTER_VOICE_OK) {
        return checked;
    }

    /* The notes, reduced to their facts. */
    OuterNote *facts = (OuterNote *)calloc(note_count > 0 ? note_count : 1, sizeof(OuterNote));
    NoteKey *keys = (NoteKey *)calloc(note_count > 0 ? note_count : 1, sizeof(NoteKey));
    if (facts == NULL || keys == NULL) {
        free(facts);
        free(keys);
        return OUTER_VOICE_ERR_ALLOC;
    }
    OuterVoiceResult failure = OUTER_VOICE_OK;
    for (size_t i = 0; i < note_count && failure == OUTER_VOICE_OK; i++) {
        if (notes[i] == NULL || pitch_values[i] == NULL) {
            failure = OUTER_VOICE_ERR_NULL_NOTE;
            break;
        }
        if (i > 0 && !tuning_system_identity_equals(pitch_numeric_value_tuning_system(pitch_values[0]),
                                                    pitch_numeric_value_tuning_system(pitch_values[i]))) {
            failure = OUTER_VOICE_ERR_MIXED_TUNING;
            break;
        }
        long numerator = pitch_numeric_value_position_numerator(pitch_values[i]);
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(notes[i]));
        if (numerator < 0 || musical_time_numerator(position) < 0) {
            failure = OUTER_VOICE_ERR_UNSUPPORTED_PITCH;
            break;
        }
        facts[i].id = entity_id_value(note_event_id(notes[i]));
        Rational length;
        if (rational_of_musical_time(position, &facts[i].onset) != RATIONAL_OK ||
            rational_of_duration(note_event_duration(notes[i]), &length) != RATIONAL_OK ||
            rational_add(facts[i].onset, length, &facts[i].release) != RATIONAL_OK) {
            failure = OUTER_VOICE_ERR_OVERFLOW;
            break;
        }
        facts[i].pitch = rational_ratio((unsigned long long)numerator, pitch_numeric_value_position_denominator(pitch_values[i]));
        keys[i].id = facts[i].id;
        keys[i].index = i;
    }
    if (failure == OUTER_VOICE_OK && note_count > 0) {
        qsort(keys, note_count, sizeof(NoteKey), compare_keys);
    }

    /* The segments, with their members as note indices. */
    size_t segment_count = segments->count;
    OuterSegment *views = (OuterSegment *)calloc(segment_count > 0 ? segment_count : 1, sizeof(OuterSegment));
    size_t **member_arrays = (size_t **)calloc(segment_count > 0 ? segment_count : 1, sizeof(size_t *));
    if (views == NULL || member_arrays == NULL) {
        failure = OUTER_VOICE_ERR_ALLOC;
    }
    for (size_t s = 0; s < segment_count && failure == OUTER_VOICE_OK; s++) {
        const Sonority *sonority = segments->items[s].sonority;
        const TimeSpan *window = sonority_time_window(sonority);
        if (rational_of_musical_time(time_span_start(window), &views[s].start) != RATIONAL_OK ||
            rational_of_musical_time(time_span_end(window), &views[s].end) != RATIONAL_OK) {
            failure = OUTER_VOICE_ERR_OVERFLOW;
            break;
        }
        size_t count = sonority_event_id_count(sonority);
        member_arrays[s] = (size_t *)calloc(count > 0 ? count : 1, sizeof(size_t));
        if (member_arrays[s] == NULL) {
            failure = OUTER_VOICE_ERR_ALLOC;
            break;
        }
        for (size_t m = 0; m < count; m++) {
            NoteKey probe = {entity_id_value(sonority_event_id_at(sonority, m)), 0};
            const NoteKey *found =
                note_count > 0 ? (const NoteKey *)bsearch(&probe, keys, note_count, sizeof(NoteKey), compare_keys)
                               : NULL;
            if (found == NULL) {
                failure = OUTER_VOICE_ERR_UNKNOWN_NOTE;
                break;
            }
            member_arrays[s][m] = found->index;
        }
        views[s].members = member_arrays[s];
        views[s].member_count = count;
    }

    OuterVoiceLines lines = {NULL, NULL, segment_count};
    if (failure == OUTER_VOICE_OK) {
        OuterNode *soprano_nodes = (OuterNode *)calloc(segment_count > 0 ? segment_count : 1, sizeof(OuterNode));
        OuterNode *bass_nodes = (OuterNode *)calloc(segment_count > 0 ? segment_count : 1, sizeof(OuterNode));
        lines.soprano = (OuterVoiceEntry *)calloc(segment_count > 0 ? segment_count : 1, sizeof(OuterVoiceEntry));
        lines.bass = (OuterVoiceEntry *)calloc(segment_count > 0 ? segment_count : 1, sizeof(OuterVoiceEntry));
        if (soprano_nodes == NULL || bass_nodes == NULL || lines.soprano == NULL || lines.bass == NULL) {
            failure = OUTER_VOICE_ERR_ALLOC;
        } else {
            outer_voice_pick_line(facts, views, segment_count, true, soprano_nodes);
            outer_voice_pick_line(facts, views, segment_count, false, bass_nodes);

            BeatGridContext grid = {map, provider, provider_context};
            Rational window = rational_ratio(effective.gap_bridge_max_numerator, effective.gap_bridge_max_denominator);
            if (effective.guard_soprano_register) {
                failure = outer_voice_guard_register(facts, views, segment_count, true,
                                                     effective.register_guard_semitones, window, grid_beat_length,
                                                     &grid, soprano_nodes);
            }
            if (failure == OUTER_VOICE_OK && effective.guard_bass_register) {
                failure = outer_voice_guard_register(facts, views, segment_count, false,
                                                     effective.register_guard_semitones, window, grid_beat_length,
                                                     &grid, bass_nodes);
            }
            if (failure == OUTER_VOICE_OK && effective.bridge_gaps) {
                failure = outer_voice_mark_bridged_gaps(facts, views, segment_count, window, grid_beat_length, &grid,
                                                        soprano_nodes);
                if (failure == OUTER_VOICE_OK) {
                    failure = outer_voice_mark_bridged_gaps(facts, views, segment_count, window, grid_beat_length,
                                                            &grid, bass_nodes);
                }
            }
            size_t built = 0;
            for (; built < segment_count && failure == OUTER_VOICE_OK; built++) {
                failure = build_entry(&soprano_nodes[built], built, &views[built], notes, pitch_values,
                                      &lines.soprano[built]);
                if (failure == OUTER_VOICE_OK) {
                    failure = build_entry(&bass_nodes[built], built, &views[built], notes, pitch_values,
                                          &lines.bass[built]);
                    if (failure != OUTER_VOICE_OK) {
                        entry_release(&lines.soprano[built]);
                    }
                }
            }
            lines.count = built;
        }
        free(soprano_nodes);
        free(bass_nodes);
    }

    if (member_arrays != NULL) {
        for (size_t s = 0; s < segment_count; s++) {
            free(member_arrays[s]);
        }
    }
    free(member_arrays);
    free(views);
    free(keys);
    free(facts);

    if (failure != OUTER_VOICE_OK) {
        outer_voice_lines_destroy(&lines);
        return failure;
    }
    *out = lines;
    return OUTER_VOICE_OK;
}
