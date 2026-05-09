#include "semantics/sonority_segmentation.h"

#include <stdlib.h>
#include <string.h>

#include "pitch/pitch_spelling.h"
#include "semantics/sonority_segmentation_internal.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

AdaptiveSegmentationConfig adaptive_segmentation_config_default(void) {
    AdaptiveSegmentationConfig config;
    config.min_split_numerator = 1;
    config.min_split_denominator = 2;
    config.merge_across_barlines = false;
    config.merge_unchanged_harmony = true;
    return config;
}

void segment_list_destroy(SegmentList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].members);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* The semitone of each letter above C, indexed by PitchLetter (A B C D E F G). */
static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};

static int semitone_of(const PitchSpelling *spelling) {
    return 12 * (pitch_spelling_octave(spelling) + 1) + LETTER_SEMITONE[pitch_spelling_letter(spelling)] +
           pitch_spelling_accidental(spelling);
}

AdaptiveSegmentationResult segmentation_prepare_notes(NoteEvent *const *notes, size_t note_count, NoteSpan **out) {
    NoteSpan *spans = (NoteSpan *)calloc(note_count, sizeof(NoteSpan));
    if (spans == NULL) {
        return AD_SEGMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < note_count; i++) {
        const NoteEvent *note = notes[i];
        if (note == NULL) {
            free(spans);
            return AD_SEGMENTATION_ERR_NULL_NOTE;
        }
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(note);
        if (spelling == NULL) {
            free(spans);
            return AD_SEGMENTATION_ERR_NO_PITCH;
        }
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(note));
        if (musical_time_numerator(position) < 0) {
            free(spans);
            return AD_SEGMENTATION_ERR_METER_MAP;
        }
        spans[i].onset = er_of_time(position);
        if (er_add(spans[i].onset, er_of_duration(note_event_duration(note)), &spans[i].release) != 0) {
            free(spans);
            return AD_SEGMENTATION_ERR_OVERFLOW;
        }
        spans[i].pitch = semitone_of(spelling);
        spans[i].pitch_class = ((spans[i].pitch % 12) + 12) % 12;
    }
    *out = spans;
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_members_of(const NoteSpan *spans, size_t span_count, ExactRational start,
                                                    ExactRational end, SegmentMember **out_members,
                                                    size_t *out_count) {
    SegmentMember *members = NULL;
    size_t count = 0;
    if (span_count > 0) {
        members = (SegmentMember *)malloc(span_count * sizeof(SegmentMember));
        if (members == NULL) {
            return AD_SEGMENTATION_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < span_count; i++) {
        if (er_cmp(spans[i].onset, end) < 0 && er_cmp(spans[i].release, start) > 0) {
            members[count].note = i;
            members[count].attacked = er_cmp(spans[i].onset, start) >= 0;
            count++;
        }
    }
    if (count == 0) {
        free(members);
        members = NULL;
    }
    *out_members = members;
    *out_count = count;
    return AD_SEGMENTATION_OK;
}

int segment_bass_pitch(const NoteSpan *spans, const Segment *segment) {
    int bass = -1;
    for (size_t i = 0; i < segment->member_count; i++) {
        int pitch = spans[segment->members[i].note].pitch;
        if (bass < 0 || pitch < bass) {
            bass = pitch;
        }
    }
    return bass;
}

unsigned segment_pitch_class_set(const NoteSpan *spans, const Segment *segment) {
    unsigned set = 0;
    for (size_t i = 0; i < segment->member_count; i++) {
        set |= 1u << spans[segment->members[i].note].pitch_class;
    }
    return set;
}

static void free_beat_starts(MusicalTime **starts, size_t count) {
    if (starts == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        musical_time_destroy(starts[i]);
    }
    free(starts);
}

static AdaptiveSegmentationResult append_segment(SegmentList *list, size_t *capacity, Segment segment) {
    if (list->count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 64 : *capacity * 2;
        Segment *grown = (Segment *)realloc(list->items, grown_capacity * sizeof(Segment));
        if (grown == NULL) {
            return AD_SEGMENTATION_ERR_ALLOC;
        }
        list->items = grown;
        *capacity = grown_capacity;
    }
    list->items[list->count++] = segment;
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_cut(const NoteSpan *spans, size_t span_count, const MeterMap *map,
                                             BeatGridProvider provider, void *provider_context, SegmentList *out) {
    if (spans == NULL || span_count == 0 || map == NULL || provider == NULL || out == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }

    ExactRational sounding_end = spans[0].release;
    for (size_t i = 1; i < span_count; i++) {
        if (er_cmp(spans[i].release, sounding_end) > 0) {
            sounding_end = spans[i].release;
        }
    }

    SegmentList list = {NULL, 0};
    size_t capacity = 0;
    AdaptiveSegmentationResult failure = AD_SEGMENTATION_OK;

    for (int measure = meter_map_first_measure_number(map); failure == AD_SEGMENTATION_OK; measure++) {
        TimeSpan *measure_span = NULL;
        if (meter_map_measure_span(map, measure, &measure_span) != METER_MAP_OK) {
            failure = AD_SEGMENTATION_ERR_METER_MAP;
            break;
        }
        ExactRational measure_start = er_of_time(time_span_start(measure_span));
        ExactRational measure_end = er_of_time(time_span_end(measure_span));
        time_span_destroy(measure_span);
        if (er_cmp(measure_start, sounding_end) >= 0) {
            break;
        }

        MusicalTime **starts = NULL;
        size_t start_count = 0;
        if (!provider(map, measure, provider_context, &starts, &start_count) || start_count == 0) {
            free_beat_starts(starts, start_count);
            failure = AD_SEGMENTATION_ERR_BEAT_GRID;
            break;
        }

        for (size_t beat = 0; beat < start_count && failure == AD_SEGMENTATION_OK; beat++) {
            ExactRational beat_start = er_of_time(starts[beat]);
            ExactRational beat_end = beat + 1 < start_count ? er_of_time(starts[beat + 1]) : measure_end;
            bool starts_ok = beat > 0 || er_cmp(beat_start, measure_start) == 0;
            if (!starts_ok || er_cmp(beat_start, beat_end) >= 0 || er_cmp(beat_end, measure_end) > 0) {
                failure = AD_SEGMENTATION_ERR_BEAT_GRID;
                break;
            }

            Segment segment;
            segment.start = beat_start;
            segment.end = beat_end;
            segment.starts_measure = beat == 0;
            failure = segmentation_members_of(spans, span_count, beat_start, beat_end, &segment.members,
                                              &segment.member_count);
            if (failure != AD_SEGMENTATION_OK) {
                break;
            }
            failure = append_segment(&list, &capacity, segment);
            if (failure != AD_SEGMENTATION_OK) {
                free(segment.members);
            }
        }
        free_beat_starts(starts, start_count);
    }

    if (failure != AD_SEGMENTATION_OK) {
        segment_list_destroy(&list);
        return failure;
    }
    *out = list;
    return AD_SEGMENTATION_OK;
}

/* --- step 2, the bass split --- */

/* The lowest sounding pitch among the members sounding at `t` (onset <= t < release) when `after`,
   or sounding just before it (onset < t <= release) otherwise; -1 for none. *out_attacked is set when
   a note with onset == t has that lowest pitch. */
static int lowest_at(const NoteSpan *spans, const Segment *segment, ExactRational t, bool after,
                     bool *out_attacked) {
    int lowest = -1;
    for (size_t i = 0; i < segment->member_count; i++) {
        const NoteSpan *note = &spans[segment->members[i].note];
        bool sounding = after ? (er_cmp(note->onset, t) <= 0 && er_cmp(note->release, t) > 0)
                              : (er_cmp(note->onset, t) < 0 && er_cmp(note->release, t) >= 0);
        if (sounding && (lowest < 0 || note->pitch < lowest)) {
            lowest = note->pitch;
        }
    }
    if (out_attacked != NULL) {
        *out_attacked = false;
        for (size_t i = 0; lowest >= 0 && i < segment->member_count; i++) {
            const NoteSpan *note = &spans[segment->members[i].note];
            if (er_cmp(note->onset, t) == 0 && note->pitch == lowest) {
                *out_attacked = true;
            }
        }
    }
    return lowest;
}

static int pitch_class_of(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

/* The accepted split times inside `segment`, ascending. *out is allocated (NULL when none). */
static AdaptiveSegmentationResult split_points_of(const NoteSpan *spans, const Segment *segment,
                                                   const AdaptiveSegmentationConfig *config,
                                                   ExactRational **out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    if (segment->member_count == 0) {
        return AD_SEGMENTATION_OK;
    }

    /* The attack times strictly inside the beat, ascending and without repeats. */
    ExactRational *attacks = (ExactRational *)malloc(segment->member_count * sizeof(ExactRational));
    ExactRational *changes = (ExactRational *)malloc(segment->member_count * sizeof(ExactRational));
    if (attacks == NULL || changes == NULL) {
        free(attacks);
        free(changes);
        return AD_SEGMENTATION_ERR_ALLOC;
    }
    size_t attack_count = 0;
    for (size_t i = 0; i < segment->member_count; i++) {
        ExactRational onset = spans[segment->members[i].note].onset;
        if (er_cmp(onset, segment->start) <= 0 || er_cmp(onset, segment->end) >= 0) {
            continue;
        }
        size_t at = 0;
        while (at < attack_count && er_cmp(attacks[at], onset) < 0) {
            at++;
        }
        if (at < attack_count && er_cmp(attacks[at], onset) == 0) {
            continue;
        }
        for (size_t j = attack_count; j > at; j--) {
            attacks[j] = attacks[j - 1];
        }
        attacks[at] = onset;
        attack_count++;
    }

    /* Of those, the times the bass changes at (a new lowest note, a different pitch class). */
    size_t change_count = 0;
    for (size_t i = 0; i < attack_count; i++) {
        bool attacked = false;
        int after = lowest_at(spans, segment, attacks[i], true, &attacked);
        int before = lowest_at(spans, segment, attacks[i], false, NULL);
        if (after >= 0 && attacked && (before < 0 || pitch_class_of(before) != pitch_class_of(after))) {
            changes[change_count++] = attacks[i];
        }
    }
    free(attacks);

    /* The shortest a new bass may last: the minimum fraction of the beat. */
    ExactRational beat = {0, 1};
    ExactRational minimum = {0, 1};
    if (er_sub(segment->end, segment->start, &beat) != 0 ||
        er_mul(beat, er_make(config->min_split_numerator, config->min_split_denominator), &minimum) != 0) {
        free(changes);
        return AD_SEGMENTATION_ERR_OVERFLOW;
    }

    size_t accepted = 0;
    for (size_t i = 0; i < change_count; i++) {
        ExactRational until = i + 1 < change_count ? changes[i + 1] : segment->end;
        ExactRational lasts = {0, 1};
        if (er_sub(until, changes[i], &lasts) != 0) {
            free(changes);
            return AD_SEGMENTATION_ERR_OVERFLOW;
        }
        if (er_cmp(lasts, minimum) >= 0) {
            changes[accepted++] = changes[i]; /* in place: accepted never passes i */
            /* the next candidate's end is read from changes[i + 1], untouched while accepted <= i */
        }
    }
    if (accepted == 0) {
        free(changes);
        return AD_SEGMENTATION_OK;
    }
    *out = changes;
    *out_count = accepted;
    return AD_SEGMENTATION_OK;
}

/* The members of `parent` that sound in [start, end). */
static AdaptiveSegmentationResult members_within(const NoteSpan *spans, const Segment *parent, ExactRational start,
                                                  ExactRational end, SegmentMember **out, size_t *out_count) {
    SegmentMember *members = NULL;
    size_t count = 0;
    if (parent->member_count > 0) {
        members = (SegmentMember *)malloc(parent->member_count * sizeof(SegmentMember));
        if (members == NULL) {
            return AD_SEGMENTATION_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < parent->member_count; i++) {
        const NoteSpan *note = &spans[parent->members[i].note];
        if (er_cmp(note->onset, end) < 0 && er_cmp(note->release, start) > 0) {
            members[count].note = parent->members[i].note;
            members[count].attacked = er_cmp(note->onset, start) >= 0;
            count++;
        }
    }
    if (count == 0) {
        free(members);
        members = NULL;
    }
    *out = members;
    *out_count = count;
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_split_bass(const NoteSpan *spans, const AdaptiveSegmentationConfig *config,
                                                    SegmentList *segments) {
    if (spans == NULL || config == NULL || segments == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }

    SegmentList result = {NULL, 0};
    size_t capacity = 0;
    AdaptiveSegmentationResult failure = AD_SEGMENTATION_OK;

    for (size_t i = 0; i < segments->count && failure == AD_SEGMENTATION_OK; i++) {
        Segment *segment = &segments->items[i];
        ExactRational *points = NULL;
        size_t point_count = 0;
        failure = split_points_of(spans, segment, config, &points, &point_count);
        if (failure != AD_SEGMENTATION_OK) {
            break;
        }

        if (point_count == 0) {
            failure = append_segment(&result, &capacity, *segment);
            if (failure == AD_SEGMENTATION_OK) {
                segment->members = NULL; /* moved to the result */
                segment->member_count = 0;
            }
            continue;
        }

        ExactRational piece_start = segment->start;
        for (size_t piece = 0; piece <= point_count && failure == AD_SEGMENTATION_OK; piece++) {
            Segment part;
            part.start = piece_start;
            part.end = piece < point_count ? points[piece] : segment->end;
            part.starts_measure = piece == 0 && segment->starts_measure;
            failure = members_within(spans, segment, part.start, part.end, &part.members, &part.member_count);
            if (failure != AD_SEGMENTATION_OK) {
                break;
            }
            failure = append_segment(&result, &capacity, part);
            if (failure != AD_SEGMENTATION_OK) {
                free(part.members);
                break;
            }
            piece_start = part.end;
        }
        free(points);
    }

    if (failure != AD_SEGMENTATION_OK) {
        segment_list_destroy(&result);
        return failure;
    }
    /* Segments that were split still own their member arrays; the moved ones were cleared. */
    segment_list_destroy(segments);
    *segments = result;
    return AD_SEGMENTATION_OK;
}

/* The pitch class of the lowest sounding pitch of a non-empty segment. */
static int bass_pitch_class(const NoteSpan *spans, const Segment *segment) {
    return ((segment_bass_pitch(spans, segment) % 12) + 12) % 12;
}

/* Appends the members of `next` that `into` does not already hold, and marks a shared note attacked if
   either has it attacked. */
static AdaptiveSegmentationResult union_members(Segment *into, const Segment *next) {
    size_t added = 0;
    for (size_t i = 0; i < next->member_count; i++) {
        bool found = false;
        for (size_t j = 0; j < into->member_count; j++) {
            if (into->members[j].note == next->members[i].note) {
                into->members[j].attacked = into->members[j].attacked || next->members[i].attacked;
                found = true;
                break;
            }
        }
        if (!found) {
            added++;
        }
    }
    if (added == 0) {
        return AD_SEGMENTATION_OK;
    }
    SegmentMember *grown = (SegmentMember *)realloc(into->members, (into->member_count + added) * sizeof(SegmentMember));
    if (grown == NULL) {
        return AD_SEGMENTATION_ERR_ALLOC;
    }
    into->members = grown;
    size_t base = into->member_count;
    for (size_t i = 0; i < next->member_count; i++) {
        bool found = false;
        for (size_t j = 0; j < base; j++) {
            if (into->members[j].note == next->members[i].note) {
                found = true;
                break;
            }
        }
        if (!found) {
            into->members[into->member_count++] = next->members[i];
        }
    }
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_merge(const NoteSpan *spans, const AdaptiveSegmentationConfig *config,
                                               SegmentList *segments) {
    if (spans == NULL || config == NULL || segments == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }

    size_t kept = 0; /* segments[0..kept) is the merged result so far */
    for (size_t i = 0; i < segments->count; i++) {
        Segment *segment = &segments->items[i];
        if (kept > 0) {
            Segment *last = &segments->items[kept - 1];
            bool crosses_barline = segment->starts_measure && !config->merge_across_barlines;
            bool same_content = last->member_count > 0 && segment->member_count > 0 &&
                                segment_pitch_class_set(spans, last) == segment_pitch_class_set(spans, segment) &&
                                bass_pitch_class(spans, last) == bass_pitch_class(spans, segment);
            if (same_content && !crosses_barline && er_cmp(last->end, segment->start) == 0) {
                AdaptiveSegmentationResult merged = union_members(last, segment);
                if (merged != AD_SEGMENTATION_OK) {
                    return merged;
                }
                last->end = segment->end;
                free(segment->members);
                segment->members = NULL;
                segment->member_count = 0;
                continue;
            }
        }
        if (kept != i) {
            segments->items[kept] = *segment;
        }
        kept++;
    }
    segments->count = kept;
    return AD_SEGMENTATION_OK;
}

/* --- step 4, the merge over unchanged harmony --- */

/* The pitch classes of a segment's chord: every member's, less the highest note's when no other member
   carries its pitch class (a melody note that may move on). */
static unsigned chord_pitch_class_set(const NoteSpan *spans, const Segment *segment) {
    if (segment->member_count == 0) {
        return 0;
    }
    size_t top = 0;
    for (size_t i = 1; i < segment->member_count; i++) {
        if (spans[segment->members[i].note].pitch > spans[segment->members[top].note].pitch) {
            top = i;
        }
    }
    unsigned set = 0;
    bool top_doubled = false;
    int top_pc = spans[segment->members[top].note].pitch_class;
    for (size_t i = 0; i < segment->member_count; i++) {
        if (i == top) {
            continue;
        }
        int pc = spans[segment->members[i].note].pitch_class;
        set |= 1u << pc;
        top_doubled = top_doubled || pc == top_pc;
    }
    if (top_doubled || segment->member_count == 1) {
        set |= 1u << top_pc;
    }
    return set;
}

static int pitch_class_count(unsigned set) {
    int count = 0;
    for (; set != 0; set &= set - 1) {
        count++;
    }
    return count;
}

/* True when `segment` only adds non-bass attacks over the chord `chord_set` still held from before it. A
   chord needs two pitch classes: a lone bass under a moving line is not unchanged harmony. */
static bool holds_unchanged_harmony(const NoteSpan *spans, const Segment *segment, unsigned chord_set) {
    if (segment->member_count == 0 || pitch_class_count(chord_set) < 2) {
        return false;
    }
    int bass = segment_bass_pitch(spans, segment);
    unsigned held = 0;
    bool any_attack = false;
    for (size_t i = 0; i < segment->member_count; i++) {
        const NoteSpan *note = &spans[segment->members[i].note];
        if (segment->members[i].attacked) {
            if (note->pitch <= bass) {
                return false; /* a new bass */
            }
            any_attack = true;
        } else {
            held |= 1u << note->pitch_class;
        }
    }
    return any_attack && (chord_set & ~held) == 0;
}

AdaptiveSegmentationResult segmentation_merge_unchanged_harmony(const NoteSpan *spans,
                                                                const AdaptiveSegmentationConfig *config,
                                                                SegmentList *segments) {
    if (spans == NULL || config == NULL || segments == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }
    if (!config->merge_unchanged_harmony) {
        return AD_SEGMENTATION_OK;
    }

    size_t kept = 0;      /* segments[0..kept) is the merged result so far */
    unsigned chord_set = 0; /* the chord of segments[kept - 1], as its first segment had it */
    for (size_t i = 0; i < segments->count; i++) {
        Segment *segment = &segments->items[i];
        if (kept > 0) {
            Segment *last = &segments->items[kept - 1];
            bool crosses_barline = segment->starts_measure && !config->merge_across_barlines;
            if (!crosses_barline && last->member_count > 0 && er_cmp(last->end, segment->start) == 0 &&
                holds_unchanged_harmony(spans, segment, chord_set)) {
                AdaptiveSegmentationResult merged = union_members(last, segment);
                if (merged != AD_SEGMENTATION_OK) {
                    return merged;
                }
                last->end = segment->end;
                free(segment->members);
                segment->members = NULL;
                segment->member_count = 0;
                continue;
            }
        }
        if (kept != i) {
            segments->items[kept] = *segment;
        }
        chord_set = chord_pitch_class_set(spans, &segments->items[kept]);
        kept++;
    }
    segments->count = kept;
    return AD_SEGMENTATION_OK;
}

void adaptive_sonorities_destroy(AdaptiveSonorities *sonorities) {
    if (sonorities == NULL) {
        return;
    }
    for (size_t i = 0; i < sonorities->count; i++) {
        sonority_destroy(sonorities->items[i].sonority);
        free(sonorities->items[i].held);
    }
    free(sonorities->items);
    sonorities->items = NULL;
    sonorities->count = 0;
}

/* One AdaptiveSonority for a non-empty segment. */
static AdaptiveSegmentationResult assemble_one(NoteEvent *const *notes, const Segment *segment,
                                                AdaptiveSonority *out) {
    MusicalTime *start = er_to_time(segment->start);
    MusicalTime *end = er_to_time(segment->end);
    if (start == NULL || end == NULL) {
        musical_time_destroy(start);
        musical_time_destroy(end);
        return AD_SEGMENTATION_ERR_OVERFLOW;
    }
    TimeSpan *window = NULL;
    TimeSpanResult span_result = time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &window);
    musical_time_destroy(start);
    musical_time_destroy(end);
    if (span_result != TIME_SPAN_OK) {
        return AD_SEGMENTATION_ERR_ALLOC;
    }

    EntityID **ids = (EntityID **)calloc(segment->member_count, sizeof(EntityID *));
    bool *held = (bool *)calloc(segment->member_count, sizeof(bool));
    AdaptiveSegmentationResult failure = AD_SEGMENTATION_OK;
    if (ids == NULL || held == NULL) {
        failure = AD_SEGMENTATION_ERR_ALLOC;
    } else {
        for (size_t i = 0; i < segment->member_count; i++) {
            const EntityID *note_id = note_event_id(notes[segment->members[i].note]);
            if (entity_id_create(entity_id_value(note_id), &ids[i]) != ENTITY_ID_OK) {
                failure = AD_SEGMENTATION_ERR_ALLOC;
                break;
            }
            held[i] = !segment->members[i].attacked;
        }
    }

    Sonority *sonority = NULL;
    if (failure == AD_SEGMENTATION_OK &&
        sonority_create(window, ids, segment->member_count, NULL, 0, SONORITY_EVENT_DOMAIN_SOUNDING, &sonority) !=
            SONORITY_OK) {
        failure = AD_SEGMENTATION_ERR_ALLOC;
    }
    if (failure != AD_SEGMENTATION_OK) {
        if (ids != NULL) {
            for (size_t i = 0; i < segment->member_count; i++) {
                entity_id_destroy(ids[i]);
            }
        }
        free(ids);
        free(held);
        time_span_destroy(window);
        return failure;
    }
    /* sonority_create took the window and the ids; the array itself is ours. */
    free(ids);
    out->sonority = sonority;
    out->held = held;
    return AD_SEGMENTATION_OK;
}

static AdaptiveSegmentationResult validate_config(const AdaptiveSegmentationConfig *config) {
    if (config->min_split_denominator == 0 || config->min_split_numerator > config->min_split_denominator) {
        return AD_SEGMENTATION_ERR_BAD_CONFIG;
    }
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult sonority_builder_build_adaptive(NoteEvent *const *notes, size_t note_count,
                                                             const MeterMap *map, BeatGridProvider provider,
                                                             void *provider_context,
                                                             const AdaptiveSegmentationConfig *config,
                                                             AdaptiveSonorities *out) {
    if (notes == NULL || note_count == 0 || map == NULL || provider == NULL || out == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }
    AdaptiveSegmentationConfig effective = config != NULL ? *config : adaptive_segmentation_config_default();
    AdaptiveSegmentationResult checked = validate_config(&effective);
    if (checked != AD_SEGMENTATION_OK) {
        return checked;
    }

    NoteSpan *spans = NULL;
    AdaptiveSegmentationResult prepared = segmentation_prepare_notes(notes, note_count, &spans);
    if (prepared != AD_SEGMENTATION_OK) {
        return prepared;
    }

    SegmentList segments = {NULL, 0};
    AdaptiveSegmentationResult result =
        segmentation_cut(spans, note_count, map, provider, provider_context, &segments);
    if (result == AD_SEGMENTATION_OK) {
        result = segmentation_split_bass(spans, &effective, &segments);
    }
    if (result == AD_SEGMENTATION_OK) {
        result = segmentation_merge(spans, &effective, &segments);
    }
    if (result == AD_SEGMENTATION_OK) {
        result = segmentation_merge_unchanged_harmony(spans, &effective, &segments);
    }

    AdaptiveSonorities assembled = {NULL, 0};
    if (result == AD_SEGMENTATION_OK) {
        size_t non_empty = 0;
        for (size_t i = 0; i < segments.count; i++) {
            if (segments.items[i].member_count > 0) {
                non_empty++;
            }
        }
        if (non_empty > 0) {
            assembled.items = (AdaptiveSonority *)calloc(non_empty, sizeof(AdaptiveSonority));
            if (assembled.items == NULL) {
                result = AD_SEGMENTATION_ERR_ALLOC;
            }
        }
        for (size_t i = 0; i < segments.count && result == AD_SEGMENTATION_OK; i++) {
            if (segments.items[i].member_count == 0) {
                continue;
            }
            result = assemble_one(notes, &segments.items[i], &assembled.items[assembled.count]);
            if (result == AD_SEGMENTATION_OK) {
                assembled.count++;
            }
        }
    }

    segment_list_destroy(&segments);
    free(spans);
    if (result != AD_SEGMENTATION_OK) {
        adaptive_sonorities_destroy(&assembled);
        return result;
    }
    *out = assembled;
    return AD_SEGMENTATION_OK;
}
