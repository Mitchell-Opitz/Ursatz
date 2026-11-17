#include "chain/chain_inputs_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/meter/metric_position_rules.h"
#include "theory/meter/segmentation_beat_grid.h"

ChainInputStatus chain_build_segments(const AnalysisNoteSet *notes, const AdaptiveSegmentationConfig *config, ChainSegments *out) {
    if (notes == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    NoteEvent **events = calloc(notes->count, sizeof(NoteEvent *));
    if (events == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        if (notes->notes[i].event == NULL) {
            free(events);
            return CHAIN_INPUT_ERR_NO_EVENT;
        }
        events[i] = (NoteEvent *)notes->notes[i].event; /* read only by segmentation */
    }
    ChainSegments built;
    memset(&built, 0, sizeof(built));
    AdaptiveSegmentationResult segmented = sonority_builder_build_adaptive(events, notes->count, notes->map,
                                                                            standard_meter_segmentation_beat_grid, NULL, config,
                                                                            &built.sonorities);
    free(events);
    if (segmented != AD_SEGMENTATION_OK) {
        return CHAIN_INPUT_ERR_SEGMENTATION;
    }
    built.count = built.sonorities.count;
    built.ids = calloc(built.count > 0 ? built.count : 1, sizeof(char[32]));
    built.starts = calloc(built.count > 0 ? built.count : 1, sizeof(Rational));
    built.ends = calloc(built.count > 0 ? built.count : 1, sizeof(Rational));
    if (built.ids == NULL || built.starts == NULL || built.ends == NULL) {
        chain_segments_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t s = 0; s < built.count; s++) {
        const TimeSpan *window = sonority_time_window(built.sonorities.items[s].sonority);
        Rational start = window->start.value;
        Rational end = window->end.value;
        built.starts[s] = start;
        built.ends[s] = end;
        snprintf(built.ids[s], sizeof(built.ids[s]), "seg-%zu", s);
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

BeatStrength chain_attack_strength(const MeterMap *map, const MusicalTime *onset) {
    MetricalPosition *position = NULL;
    BeatStrength strength = BEAT_STRENGTH_WEAK;
    if (map == NULL || onset == NULL || metrical_position_from_time(map, onset, &position) != METRICAL_POSITION_OK) {
        return strength;
    }
    const TimeSignature *reading = NULL;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) == METRICAL_POSITION_OK) {
        BeatStrength found;
        if (standard_meter_beat_strength(reading, position, &found)) {
            strength = found;
        }
    }
    metrical_position_destroy(position);
    return strength;
}

/* The beat a segment [start, end) lies inside, as a non-zero index (measure and beat), or 0 when it spans beats or the grid cannot be
   read. It is CommonPracticeSegment.beat, which the context chord reading shares. */
static long beat_index(const MeterMap *map, Rational start, Rational end) {
    MusicalTime at = musical_time_of(start);
    int measure = 0;
    bool found = meter_map_measure_index_at(map, &at, &measure) == METER_MAP_OK;
    MusicalTime *starts = NULL;
    size_t count = 0;
    if (!found || !standard_meter_segmentation_beat_grid(map, measure, NULL, &starts, &count)) {
        return 0;
    }
    TimeSpan span;
    long index = 0;
    if (meter_map_measure_span(map, measure, &span) == METER_MAP_OK) {
        Rational measure_end = span.end.value;
        for (size_t b = 0; b < count; b++) {
            Rational beat_start = starts[b].value;
            Rational beat_end = b + 1 < count ? starts[b + 1].value : measure_end;
            if (rational_cmp(beat_start, start) <= 0 && rational_cmp(start, beat_end) < 0) {
                if (rational_cmp(end, beat_end) <= 0) {
                    index = ((long)measure + 1) * 1000 + (long)b + 1;
                }
                break;
            }
        }
    }
    free(starts);
    return index;
}

ChainInputStatus chain_build_members(const AnalysisNoteSet *notes, ChainSegments *segments) {
    if (notes == NULL || segments == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    NoteKey *keys = malloc(notes->count * sizeof(NoteKey));
    if (keys == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        keys[i].id = notes->notes[i].note_id;
        keys[i].index = i;
    }
    qsort(keys, notes->count, sizeof(NoteKey), chain_compare_keys);

    size_t total = 0;
    for (size_t s = 0; s < segments->count; s++) {
        total += sonority_event_id_count(segments->sonorities.items[s].sonority);
    }
    CommonPracticeMember *members = calloc(total > 0 ? total : 1, sizeof(CommonPracticeMember));
    CommonPracticeSegment *views = calloc(segments->count > 0 ? segments->count : 1, sizeof(CommonPracticeSegment));
    if (members == NULL || views == NULL) {
        free(keys);
        free(members);
        free(views);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainInputStatus status = CHAIN_INPUT_OK;
    size_t next = 0;
    for (size_t s = 0; s < segments->count && status == CHAIN_INPUT_OK; s++) {
        const AdaptiveSonority *segment = &segments->sonorities.items[s];
        const TimeSpan *window = sonority_time_window(segment->sonority);
        Rational start = window->start.value;
        Rational end = window->end.value;
        size_t count = sonority_event_id_count(segment->sonority);
        views[s].members = &members[next];
        views[s].member_count = count;
        views[s].id = segments->ids[s];
        views[s].beat = beat_index(notes->map, start, end);
        for (size_t j = 0; j < count; j++) {
            NoteKey probe = {entity_id_value(sonority_event_id_at(segment->sonority, j)), 0};
            const NoteKey *found = bsearch(&probe, keys, notes->count, sizeof(NoteKey), chain_compare_keys);
            if (found == NULL) {
                status = CHAIN_INPUT_ERR_UNKNOWN_NOTE;
                break;
            }
            const AnalysisNote *note = &notes->notes[found->index];
            CommonPracticeMember *member = &members[next];
            if (!common_practice_member_from_analysis_note(note, *window, BEAT_STRENGTH_STRONG, member)) {
                status = CHAIN_INPUT_ERR_MEMBER;
                break;
            }
            member->held = segment->held[j];
            member->strength = member->held ? BEAT_STRENGTH_STRONG : chain_attack_strength(notes->map, &note->span.start);
            next++;
        }
    }
    free(keys);
    if (status != CHAIN_INPUT_OK) {
        free(members);
        free(views);
        return status;
    }
    free(segments->members);
    free(segments->segments);
    segments->members = members;
    segments->segments = views;
    return CHAIN_INPUT_OK;
}

void chain_segments_destroy(ChainSegments *segments) {
    if (segments == NULL) {
        return;
    }
    adaptive_sonorities_destroy(&segments->sonorities);
    free(segments->ids);
    free(segments->starts);
    free(segments->ends);
    free(segments->members);
    free(segments->segments);
    memset(segments, 0, sizeof(*segments));
}

static bool same_note_id(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static HarmonicSpanLevel span_level_at(const MeterMap *map, const MusicalTime *start) {
    HarmonicSpanLevel level = HARMONIC_SPAN_LEVEL_INNER;
    MetricalPosition *position = NULL;
    if (metrical_position_from_time(map, start, &position) != METRICAL_POSITION_OK) {
        return level;
    }
    const TimeSignature *reading = NULL;
    StandardMeterLevel found;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) == METRICAL_POSITION_OK &&
        standard_meter_metric_level(reading, position, &found)) {
        if (found == STANDARD_METER_LEVEL_DOWNBEAT) {
            level = HARMONIC_SPAN_LEVEL_BAR;
        } else if (found == STANDARD_METER_LEVEL_SECONDARY_BEAT) {
            level = HARMONIC_SPAN_LEVEL_HALF_BAR;
        } else if (found == STANDARD_METER_LEVEL_BEAT) {
            level = HARMONIC_SPAN_LEVEL_BEAT;
        }
    }
    metrical_position_destroy(position);
    return level;
}

ChainInputStatus chain_build_span_cells(const AnalysisNoteSet *notes, const ChainSegments *segments, HarmonicSpanCell **out) {
    if (notes == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (segments->segments == NULL || segments->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    HarmonicSpanCell *cells = calloc(segments->count, sizeof(HarmonicSpanCell));
    if (cells == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < segments->count; i++) {
        const MusicalTime *start = &sonority_time_window(segments->sonorities.items[i].sonority)->start;
        int measure = 0;
        if (meter_map_measure_index_at(notes->map, start, &measure) != METER_MAP_OK) {
            free(cells);
            return CHAIN_INPUT_ERR_SEGMENTATION;
        }
        cells[i].segment = &segments->segments[i];
        cells[i].start = segments->starts[i];
        cells[i].end = segments->ends[i];
        cells[i].measure = measure;
        cells[i].level = span_level_at(notes->map, start);
    }
    *out = cells;
    return CHAIN_INPUT_OK;
}

void chain_span_cells_destroy(HarmonicSpanCell *cells) {
    free(cells);
}

static bool spans_cover(const HarmonicSpans *spans, size_t count) {
    if (spans->count == 0 || spans->first == NULL || spans->last == NULL || spans->bass == NULL || spans->first[0] != 0) {
        return false;
    }
    for (size_t k = 0; k < spans->count; k++) {
        if (spans->last[k] < spans->first[k] || (k + 1 < spans->count && spans->first[k + 1] != spans->last[k] + 1)) {
            return false;
        }
    }
    return spans->last[spans->count - 1] + 1 == count;
}

/* One merged sonority: the notes of the cells first..last once each (in the order they first sound), held when the first cell holds them. */
static bool merged_sonority(const ChainSegments *segs, size_t first, size_t last, AdaptiveSonority *out) {
    size_t capacity = 0;
    for (size_t c = first; c <= last; c++) {
        capacity += sonority_event_id_count(segs->sonorities.items[c].sonority);
    }
    EntityID **ids = calloc(capacity > 0 ? capacity : 1, sizeof(EntityID *));
    bool *held = calloc(capacity > 0 ? capacity : 1, sizeof(bool));
    const char **values = calloc(capacity > 0 ? capacity : 1, sizeof(char *));
    size_t n = 0;
    bool ok = ids != NULL && held != NULL && values != NULL;
    for (size_t c = first; ok && c <= last; c++) {
        const AdaptiveSonority *cell = &segs->sonorities.items[c];
        for (size_t j = 0; ok && j < sonority_event_id_count(cell->sonority); j++) {
            const char *value = entity_id_value(sonority_event_id_at(cell->sonority, j));
            bool seen = false;
            for (size_t k = 0; k < n && !seen; k++) {
                seen = same_note_id(values[k], value);
            }
            if (seen) {
                continue;
            }
            ok = entity_id_create(value, &ids[n]) == ENTITY_ID_OK;
            if (ok) {
                values[n] = entity_id_value(ids[n]);
                held[n] = c == first && cell->held[j];
                n++;
            }
        }
    }
    TimeSpan window;
    const TimeSpan *from = sonority_time_window(segs->sonorities.items[first].sonority);
    const TimeSpan *to = sonority_time_window(segs->sonorities.items[last].sonority);
    Sonority *sonority = NULL;
    ok = ok && n > 0 && time_span_make(from->start, to->end, &window) == TIME_SPAN_OK &&
         sonority_create(window, ids, n, NULL, 0, SONORITY_EVENT_DOMAIN_SOUNDING, &sonority) == SONORITY_OK;
    if (!ok) {
        if (ids != NULL) {
            for (size_t k = 0; k < n; k++) {
                entity_id_destroy(ids[k]);
            }
        }
        free(ids);
        free(held);
        free(values);
        return false;
    }
    free(ids); /* the sonority took the ids and copied the window */
    free(values);
    out->sonority = sonority;
    out->held = held;
    return true;
}

ChainInputStatus chain_merge_segments(const AnalysisNoteSet *notes, ChainSegments *segs, const HarmonicSpans *spans) {
    if (notes == NULL || segs == NULL || spans == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (segs->segments == NULL || segs->count == 0 || !spans_cover(spans, segs->count)) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainSegments built;
    memset(&built, 0, sizeof(built));
    built.sonorities.items = calloc(spans->count, sizeof(AdaptiveSonority));
    built.ids = calloc(spans->count, sizeof(char[32]));
    built.starts = calloc(spans->count, sizeof(Rational));
    built.ends = calloc(spans->count, sizeof(Rational));
    if (built.sonorities.items == NULL || built.ids == NULL || built.starts == NULL || built.ends == NULL) {
        chain_segments_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t k = 0; k < spans->count; k++) {
        if (!merged_sonority(segs, spans->first[k], spans->last[k], &built.sonorities.items[k])) {
            chain_segments_destroy(&built);
            return CHAIN_INPUT_ERR_ALLOC;
        }
        built.sonorities.count = k + 1;
        built.count = k + 1;
        built.starts[k] = segs->starts[spans->first[k]];
        built.ends[k] = segs->ends[spans->last[k]];
        snprintf(built.ids[k], sizeof(built.ids[k]), "seg-%zu", k);
    }
    ChainInputStatus status = chain_build_members(notes, &built);
    if (status != CHAIN_INPUT_OK) {
        chain_segments_destroy(&built);
        return status;
    }
    for (size_t k = 0; k < spans->count; k++) {
        built.segments[k].has_bass = spans->bass[k] >= 0;
        built.segments[k].bass_pitch = spans->bass[k] >= 0 ? spans->bass[k] : 0;
    }
    chain_segments_destroy(segs);
    *segs = built;
    return CHAIN_INPUT_OK;
}

/* --- the notes' pitches --- */

ChainInputStatus chain_build_pitches(const AnalysisNoteSet *notes, ChainPitches *out) {
    if (notes == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainPitches built;
    memset(&built, 0, sizeof(built));
    built.events = calloc(notes->count, sizeof(NoteEvent *));
    built.values = calloc(notes->count, sizeof(PitchNumericValue *));
    EntityID *id = NULL;
    if (built.events == NULL || built.values == NULL || entity_id_create("chain-midi", &id) != ENTITY_ID_OK) {
        chain_pitches_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    if (tuning_system_create(id, "MIDI note number", &built.tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(id);
        chain_pitches_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        const AnalysisNote *note = &notes->notes[i];
        if (note->event == NULL) {
            chain_pitches_destroy(&built);
            return CHAIN_INPUT_ERR_NO_EVENT;
        }
        if (note->midi == ANALYSIS_PITCH_UNKNOWN) {
            chain_pitches_destroy(&built);
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
        built.events[i] = (NoteEvent *)note->event; /* read only by the kernel's analyses */
        if (pitch_numeric_value_create(built.tuning, note->midi, 1, &built.values[i]) != PITCH_NUMERIC_VALUE_OK) {
            chain_pitches_destroy(&built);
            return CHAIN_INPUT_ERR_ALLOC;
        }
        built.count = i + 1;
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

void chain_pitches_destroy(ChainPitches *pitches) {
    if (pitches == NULL) {
        return;
    }
    for (size_t i = 0; i < pitches->count && pitches->values != NULL; i++) {
        pitch_numeric_value_destroy(pitches->values[i]);
    }
    free(pitches->events);
    free(pitches->values);
    tuning_system_destroy(pitches->tuning);
    memset(pitches, 0, sizeof(*pitches));
}
