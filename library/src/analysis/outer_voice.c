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
    int by_pitch = er_cmp(a->pitch, b->pitch);
    if (by_pitch != 0) {
        return soprano ? by_pitch > 0 : by_pitch < 0;
    }
    int by_onset = er_cmp(a->onset, b->onset);
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

    MusicalTime *start = er_to_time(segment->start);
    MusicalTime *end = er_to_time(segment->end);
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
    (void)map;
    (void)provider;
    (void)provider_context;
    if (segments == NULL || out == NULL || (note_count > 0 && (notes == NULL || pitch_values == NULL))) {
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
        facts[i].onset = er_of_time(position);
        if (er_add(facts[i].onset, er_of_duration(note_event_duration(notes[i])), &facts[i].release) != 0) {
            failure = OUTER_VOICE_ERR_OVERFLOW;
            break;
        }
        facts[i].pitch = er_make((unsigned long long)numerator, pitch_numeric_value_position_denominator(pitch_values[i]));
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
        views[s].start = er_of_time(time_span_start(window));
        views[s].end = er_of_time(time_span_end(window));
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
