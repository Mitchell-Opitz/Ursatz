#include "pipeline/motive_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/motif_window_matching.h"
#include "analysis/outer_voice.h"
#include "common-practice-minimal/key_vocabulary.h"
#include "common-practice-minimal/outer_scale_degrees.h"
#include "core/rational.h"
#include "pipeline/motive_features.h"
#include "pipeline/motive_sketch_map.h"
#include "pitch/pitch_numeric_value.h"
#include "semantics/analysis_note.h"
#include "time/duration.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"
#include "util/misc_util.h"

/* A soprano note: an index into the ImportedNotes, with exact start and end in whole notes. */
typedef struct {
    size_t index;
    long long start_n;
    long long start_d;
    long long end_n;
    long long end_d;
    long semitone;
} SopranoNote;

/* A note of the library's note set and its place in the ImportedNotes. */
typedef struct {
    AnalysisNote note;
    size_t index;
} SetNote;

static int compare_set_notes(const void *left, const void *right) {
    return analysis_note_compare(&((const SetNote *)left)->note, &((const SetNote *)right)->note);
}

/* The soprano: at every onset the highest sounding note (the later attack among equals), kept when it is attacked
   there. The kernel's outer_voice_attack_soprano finds it; this builds its note set from the imported notes (a note
   of unknown pitch or no length is not in it) and reads the chosen notes back. *out is owned, NULL when there are no
   notes. */
static bool find_soprano(const ImportedNotes *notes, SopranoNote **out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    if (notes->count == 0) {
        return true;
    }
    SetNote *sorted = calloc(notes->count, sizeof(SetNote));
    AnalysisNote *set_notes = calloc(notes->count, sizeof(AnalysisNote));
    TimeSpan **spans = calloc(notes->count, sizeof(TimeSpan *));
    size_t set_count = 0;
    size_t *chosen = NULL;
    size_t chosen_count = 0;
    SopranoNote *line = NULL;
    bool ok = sorted && set_notes && spans;
    for (size_t i = 0; ok && i < notes->count; i++) {
        const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        long semitone = pitch_numeric_value_position_numerator(notes->pitch_values[i]);
        if (duration_numerator(note_event_duration(notes->events[i])) <= 0) {
            continue; /* a note with no length is never sounding at an onset */
        }
        MusicalTime *end = NULL;
        if (musical_time_add(start, note_event_duration(notes->events[i]), &end) != MUSICAL_TIME_OK) {
            ok = false;
            break;
        }
        TimeSpan *span = NULL;
        TimeSpanResult made = time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span);
        musical_time_destroy(end);
        if (made != TIME_SPAN_OK) {
            ok = false;
            break;
        }
        spans[set_count] = span;
        sorted[set_count].note = (AnalysisNote){.note_id = entity_id_value(note_event_id(notes->events[i])),
                                                .event = notes->events[i],
                                                .span = span,
                                                .midi = semitone >= 0 && semitone <= 127 ? (int)semitone
                                                                                          : ANALYSIS_PITCH_UNKNOWN,
                                                .track = ANALYSIS_INDEX_NONE,
                                                .voice = ANALYSIS_INDEX_NONE,
                                                .voice_confidence = 1.0};
        sorted[set_count].index = i;
        set_count++;
    }
    if (ok) {
        qsort(sorted, set_count, sizeof(SetNote), compare_set_notes);
        for (size_t i = 0; i < set_count; i++) {
            set_notes[i] = sorted[i].note;
        }
        AnalysisNoteSet set = {set_notes, set_count, notes->meter.meter_map};
        ok = set_count == 0 || outer_voice_attack_soprano(&set, &chosen, &chosen_count) == OUTER_VOICE_OK;
    }
    if (ok && chosen_count > 0) {
        line = calloc(chosen_count, sizeof(SopranoNote));
        ok = line != NULL;
    }
    for (size_t k = 0; ok && k < chosen_count; k++) {
        size_t i = sorted[chosen[k]].index;
        const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        const Duration *duration = note_event_duration(notes->events[i]);
        SopranoNote *note = &line[k];
        note->index = i;
        note->start_n = musical_time_numerator(start);
        note->start_d = (long long)musical_time_denominator(start);
        long long duration_n = duration_numerator(duration);
        long long duration_d = (long long)duration_denominator(duration);
        note->end_n = note->start_n * duration_d + duration_n * note->start_d;
        note->end_d = note->start_d * duration_d;
        note->semitone = pitch_numeric_value_position_numerator(notes->pitch_values[i]);
    }
    for (size_t i = 0; spans && i < set_count; i++) {
        time_span_destroy(spans[i]);
    }
    free(spans);
    free(set_notes);
    free(sorted);
    free(chosen);
    if (!ok) {
        free(line);
        return false;
    }
    *out = line;
    *out_count = chosen_count;
    return true;
}

/* The key region the time lies in, or the last one for a time past the end. Regions are ordered and gap-free. */
static const AnalysisKeyRegion *region_at(const AnalysisReport *report, long long n, long long d) {
    for (size_t r = 0; r < report->key_region_count; r++) {
        const AnalysisKeyRegion *region = &report->key_regions[r];
        if (rational_cmp(rational_ratio(n, d),
                         rational_ratio(region->end_numerator, (long long)region->end_denominator)) < 0) {
            return region;
        }
    }
    return &report->key_regions[report->key_region_count - 1];
}

/* Adds the soprano to the stage's input, with each note's scale degree in its key region. The input's notes and
   form.note_indices are parallel. */
static bool add_soprano(FormStage *form, const ImportedNotes *notes, const SopranoNote *line, size_t count,
                        const AnalysisReport *report) {
    form->note_indices = malloc(sizeof(size_t) * (count > 0 ? count : 1));
    if (!form->note_indices) {
        return false;
    }
    for (size_t k = 0; k < count; k++) {
        const SopranoNote *note = &line[k];
        MusicalTime *start = NULL;
        MusicalTime *end = NULL;
        if (musical_time_create((long)note->start_n, (unsigned long)note->start_d, &start) != MUSICAL_TIME_OK ||
            musical_time_create((long)note->end_n, (unsigned long)note->end_d, &end) != MUSICAL_TIME_OK) {
            musical_time_destroy(start);
            musical_time_destroy(end);
            return false;
        }
        const TimeSpan *span = form_stage_keep_span(form, start, end);
        musical_time_destroy(start);
        musical_time_destroy(end);
        const NoteEvent *event = notes->events[note->index];
        size_t before = form->input.note_count;
        if (!span || standard_form_input_add_note(&form->input, entity_id_value(note_event_id(event)), event, span, NULL,
                                                  note->semitone) == STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
            return false;
        }
        if (form->input.note_count == before) {
            continue; /* the library declined the note */
        }
        form->note_indices[form->note_index_count++] = note->index;
        const AnalysisKeyRegion *region = region_at(report, note->start_n, note->start_d);
        int pitch_class = (int)(((note->semitone % 12) + 12) % 12);
        int degree = 0;
        int alteration = 0;
        if (common_practice_scale_degree_of(pitch_class, region->tonic_pitch_class,
                                            region->is_major ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR,
                                            &degree, &alteration) &&
            standard_form_input_set_degree(&form->input, false, form->input.note_count - 1, degree, alteration) ==
                STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
            return false;
        }
    }
    return true;
}

/* The injected window matcher: the kernel's Motif Window Matching finds the windows whose contour direction sequence
   and rhythm both equal another window's, and returns one occurrence per matched window, borrowing its event ids.
   Ported from form_kernel_match_windows. */
static StandardFormAnalysisStatus match_windows(const StandardFormMotifWindow *windows, size_t count, bool *matched,
                                                void *context) {
    (void)context;
    if (count > 0 && (windows == NULL || matched == NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    memset(matched, 0, count * sizeof(bool));
    MotifWindow *kernel = calloc(count, sizeof(MotifWindow));
    if (!kernel) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        kernel[i].contour = windows[i].contour;
        kernel[i].rhythm = windows[i].rhythm;
        kernel[i].event_ids = windows[i].event_ids;
        kernel[i].event_id_count = windows[i].event_id_count;
    }
    MotifDetectionOccurrence *found = NULL;
    size_t found_count = 0;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
    if (motif_window_matching_find(kernel, count, &found, &found_count) == MOTIF_WINDOW_MATCHING_OK) {
        status = STANDARD_FORM_ANALYSIS_OK;
        for (size_t j = 0; j < found_count; j++) {
            for (size_t i = 0; i < count; i++) {
                if (found[j].matched_event_ids == windows[i].event_ids) {
                    matched[i] = true;
                }
            }
        }
    }
    motif_window_matching_destroy_occurrences(found);
    free(kernel);
    return status;
}

bool motive_stage_run(MotiveStage *stage, const ImportedNotes *notes, const Segmentation *segmentation,
                      const HarmonyStage *harmony, const AnalysisReport *report) {
    memset(stage, 0, sizeof(*stage));
    if (!notes->meter.meter_map || segmentation->segment_count == 0 || report->key_region_count == 0) {
        return true; /* nothing to place the notes in */
    }
    SopranoNote *line = NULL;
    size_t count = 0;
    if (!find_soprano(notes, &line, &count)) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    FormStage *form = &stage->form;
    standard_form_input_init(&form->input, notes->meter.meter_map);
    form->built = true;
    bool ok = add_soprano(form, notes, line, count, report) && form_stage_add_segments(form, segmentation) &&
              form_stage_add_cadences(form, segmentation, harmony);
    free(line);
    if (!ok || !form_stage_analyze(form)) {
        return false;
    }
    if (form->phrases.count == 0) {
        return true; /* the library read no phrases, and the motive analysis needs them */
    }
    StandardFormAnalysisStatus status = standard_form_find_motives(&form->input, &form->phrases, &form->segmentation,
                                                                   &form->themes, NULL, match_windows, NULL,
                                                                   &stage->motives);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        memset(&stage->motives, 0, sizeof(stage->motives));
    }
    return status != STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

/* The phrase and idea of the report an occurrence lies in, by span: the phrase whose span holds the start of its first
   and of its last note, and the idea of that phrase which does. The motive stage reads its own phrases and ideas, from
   the same cadences, so the occurrence is placed against the report's by time. Both stay unset for an occurrence
   that crosses a boundary. */
static void link_occurrence(const AnalysisReport *report, const MusicalTime *first_start, const MusicalTime *last_start,
                            AnalysisMotiveOccurrence *occurrence) {
    long long first_n = musical_time_numerator(first_start);
    long long first_d = (long long)musical_time_denominator(first_start);
    long long last_n = musical_time_numerator(last_start);
    long long last_d = (long long)musical_time_denominator(last_start);
    for (size_t p = 0; p < report->phrase_count; p++) {
        const AnalysisPhrase *phrase = &report->phrases[p];
        long long end_d = (long long)phrase->end_denominator;
        long long start_d = (long long)phrase->start_denominator;
        if (rational_cmp(rational_ratio(first_n, first_d), rational_ratio(phrase->start_numerator, start_d)) < 0 ||
            rational_cmp(rational_ratio(last_n, last_d), rational_ratio(phrase->end_numerator, end_d)) >= 0) {
            continue;
        }
        occurrence->has_phrase = true;
        occurrence->phrase = p;
        for (size_t k = 0; k < phrase->idea_count; k++) {
            const AnalysisIdea *idea = &report->ideas[phrase->first_idea + k];
            if (rational_cmp(rational_ratio(first_n, first_d),
                             rational_ratio(idea->start_numerator, (long long)idea->start_denominator)) >= 0 &&
                rational_cmp(rational_ratio(last_n, last_d),
                             rational_ratio(idea->end_numerator, (long long)idea->end_denominator)) < 0) {
                occurrence->has_idea = true;
                occurrence->idea = phrase->first_idea + k;
                break;
            }
        }
        return;
    }
}

static bool collect_occurrence(const MotiveStage *stage, const AnalysisReport *report,
                               const StandardFormMotiveOccurrence *found, size_t index,
                               AnalysisMotiveOccurrence *occurrence) {
    const FormStage *form = &stage->form;
    const StandardFormMotiveOccurrenceClaim *claim = &found->claim;
    if (claim->note_first < 0 || claim->note_count < 1 ||
        (size_t)(claim->note_first + claim->note_count) > form->note_index_count) {
        return false;
    }
    size_t first = (size_t)claim->note_first;
    size_t last = first + (size_t)claim->note_count - 1;
    const MusicalTime *start = time_span_start(form->input.notes[first].span);
    const MusicalTime *last_start = time_span_start(form->input.notes[last].span);
    const MusicalTime *end = time_span_end(form->input.notes[last].span);
    occurrence->index = index;
    occurrence->start_numerator = musical_time_numerator(start);
    occurrence->start_denominator = musical_time_denominator(start);
    occurrence->end_numerator = musical_time_numerator(end);
    occurrence->end_denominator = musical_time_denominator(end);
    occurrence->first_measure = claim->first_measure;
    occurrence->first_beat = claim->first_beat;
    link_occurrence(report, start, last_start, occurrence);
    if (meter_map_measure_index_at(form->input.map, last_start, &occurrence->last_measure) != METER_MAP_OK) {
        return false;
    }
    occurrence->kind = standard_form_motive_kind_name((StandardFormMotiveKind)claim->kind);
    if (!occurrence->kind) {
        return false;
    }
    occurrence->score = claim->score;
    occurrence->mismatches = claim->mismatches;
    occurrence->intervals = claim->intervals;
    /* The claim gives the part of the prototype a fragment is as 0 (prefix) or 1 (suffix), and has no names for them. */
    occurrence->fragment = claim->fragment_part == 0 ? "prefix" : (claim->fragment_part == 1 ? "suffix" : NULL);
    occurrence->in_continuation = claim->in_continuation != 0;
    occurrence->uncertainty = found->uncertainty;
    occurrence->note_ids = malloc(sizeof(size_t) * (size_t)claim->note_count);
    if (!occurrence->note_ids) {
        return false;
    }
    memcpy(occurrence->note_ids, form->note_indices + first, sizeof(size_t) * (size_t)claim->note_count);
    occurrence->note_id_count = (size_t)claim->note_count;
    return true;
}

/* Fills motive->development: the occurrence indices in time order, earliest first, ties in the library's order (an
   insertion sort: the library gives them in time order already, so it is a single pass). */
static bool order_development(AnalysisMotive *motive) {
    size_t count = motive->occurrence_count;
    motive->development = malloc(sizeof(size_t) * (count > 0 ? count : 1));
    if (!motive->development) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        size_t at = i;
        while (at > 0) {
            const AnalysisMotiveOccurrence *before = &motive->occurrences[motive->development[at - 1]];
            const AnalysisMotiveOccurrence *now = &motive->occurrences[i];
            if (rational_cmp(rational_ratio(before->start_numerator, (long long)before->start_denominator),
                             rational_ratio(now->start_numerator, (long long)now->start_denominator)) <= 0) {
                break;
            }
            motive->development[at] = motive->development[at - 1];
            at--;
        }
        motive->development[at] = i;
    }
    return true;
}

/* A note's start and duration in whole notes, and its MIDI pitch. */
static bool note_timing(const ImportedNotes *notes, size_t index, Rational *start, Rational *duration,
                        int *pitch) {
    if (index >= notes->count) {
        return false;
    }
    const MusicalTime *time = temporal_anchor_position(note_event_temporal_anchor(notes->events[index]));
    const Duration *length = note_event_duration(notes->events[index]);
    if (musical_time_numerator(time) < 0 || duration_numerator(length) <= 0) {
        return false;
    }
    *start = msk_make((unsigned long long)musical_time_numerator(time), musical_time_denominator(time));
    *duration = msk_make((unsigned long long)duration_numerator(length), duration_denominator(length));
    *pitch = (int)pitch_numeric_value_position_numerator(notes->pitch_values[index]);
    return true;
}

/* Each note of an occurrence as a degree of the tonic of the key region it starts in. */
static bool collect_occurrence_degrees(const ImportedNotes *notes, const AnalysisReport *report,
                                       AnalysisMotiveOccurrence *occurrence) {
    occurrence->degrees = calloc(occurrence->note_id_count > 0 ? occurrence->note_id_count : 1, sizeof(char *));
    if (!occurrence->degrees) {
        return false;
    }
    for (size_t k = 0; k < occurrence->note_id_count; k++) {
        Rational start;
        Rational duration;
        int pitch = 0;
        if (!note_timing(notes, occurrence->note_ids[k], &start, &duration, &pitch)) {
            return false;
        }
        const AnalysisKeyRegion *region = region_at(report, (long long)start.num, (long long)start.den);
        occurrence->degrees[k] = motive_features_degree_label(pitch % 12, region->tonic_pitch_class);
    }
    return true;
}

/* The bar and beat at the first note and its position in the bar. A pickup measure is short, so its bar and beat are
   the following region's. */
static bool meter_at(const MeterMap *map, Rational start, Rational *bar, Rational *beat,
                     Rational *position) {
    MusicalTime *time = NULL;
    if (musical_time_create((long)start.num, (unsigned long)start.den, &time) != MUSICAL_TIME_OK) {
        return false;
    }
    int measure = 0;
    const MeterMapRegion *region = NULL;
    bool ok = meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK &&
              meter_map_region_of_measure(map, measure, &region) == METER_MAP_OK;
    musical_time_destroy(time);
    Rational measure_start;
    Rational measure_end;
    if (!ok || meter_map_measure_bounds(map, measure, &measure_start, &measure_end) != METER_MAP_OK) {
        return false;
    }
    bool pickup = meter_map_region_is_pickup(region);
    int reference = measure;
    const MeterMapRegion *reference_region = region;
    if (pickup && meter_map_region_of_measure(map, measure + 1, &reference_region) == METER_MAP_OK) {
        reference = measure + 1;
    } else {
        reference_region = region;
    }
    Duration *bar_duration = NULL;
    Rational beat_length;
    if (time_signature_measure_duration(meter_map_region_time_signature(reference_region), &bar_duration) !=
            DURATION_OK ||
        meter_map_beat_length(map, reference, &beat_length) != METER_MAP_OK) {
        return false;
    }
    *bar = msk_make((unsigned long long)duration_numerator(bar_duration), duration_denominator(bar_duration));
    duration_destroy(bar_duration);
    *beat = beat_length;
    if (pickup) {
        Rational remaining = msk_sub(measure_end, start);
        *position = msk_ok(remaining) && msk_cmp(remaining, *bar) < 0 ? msk_sub(*bar, remaining) : msk_int(0);
    } else {
        *position = msk_sub(start, measure_start);
    }
    return msk_ok(*bar) && msk_ok(*beat) && msk_ok(*position);
}

/* The non-chord-tone type of the segment the note starts in: the library's type, "chord-tone" when the note is one of
   the chord there, NULL when the segment has no chord or the library left the tone unclassified. *failed is set on
   allocation failure. */
static char *note_nct(const AnalysisReport *report, size_t note_id, Rational start, bool *failed) {
    for (size_t e = 0; e < report->entry_count; e++) {
        const AnalysisReportEntry *entry = &report->entries[e];
        Rational from = msk_make((unsigned long long)entry->time_numerator, entry->time_denominator);
        Rational to = msk_make((unsigned long long)entry->end_numerator, entry->end_denominator);
        if (msk_cmp(start, from) < 0 || msk_cmp(start, to) >= 0) {
            continue;
        }
        if (!entry->chord.label) {
            return NULL;
        }
        const char *type = "chord-tone";
        for (size_t n = 0; n < report->non_chord_tone_count; n++) {
            const AnalysisNonChordTone *tone = &report->non_chord_tones[n];
            if (tone->note_id == note_id && tone->chord_index == e) {
                type = tone->type;
                break;
            }
        }
        if (!type) {
            return NULL;
        }
        char *copy = strdup(type);
        *failed = copy == NULL;
        return copy;
    }
    return NULL;
}

/* Describes the family by its prototype. Leaves has_features false for a family whose first note has no key region
   or meter, or whose sketch the library would refuse. false only on allocation failure. */
static bool collect_features(const MotiveStage *stage, const ImportedNotes *notes, const AnalysisReport *report,
                             AnalysisMotive *motive) {
    const AnalysisMotiveOccurrence *prototype = &motive->occurrences[0];
    size_t count = prototype->note_id_count;
    if (count == 0 || count > MOTIVE_SKETCH_MAX_NOTES || report->key_region_count == 0) {
        return true;
    }
    AnalysisMotiveFeatures *features = &motive->features;
    features->notes = calloc(count, sizeof(AnalysisMotiveNote));
    features->degrees = calloc(count, sizeof(char *));
    features->ncts = calloc(count, sizeof(char *));
    features->intervals = calloc(count, sizeof(int));
    features->steps = calloc(count, sizeof(long));
    if (!features->notes || !features->degrees || !features->ncts || !features->intervals || !features->steps) {
        return false;
    }
    features->note_count = count;
    Rational first_start = {0, 0};
    for (size_t k = 0; k < count; k++) {
        Rational start;
        Rational duration;
        int pitch = 0;
        if (!note_timing(notes, prototype->note_ids[k], &start, &duration, &pitch)) {
            goto skip;
        }
        if (k == 0) {
            first_start = start;
            const AnalysisKeyRegion *region = region_at(report, (long long)start.num, (long long)start.den);
            features->tonic_pc = region->tonic_pitch_class;
            features->minor = !region->is_major;
        }
        Rational onset = msk_sub(start, first_start);
        if (!msk_ok(onset)) {
            goto skip;
        }
        AnalysisMotiveNote *note = &features->notes[k];
        note->note_id = prototype->note_ids[k];
        note->pitch = pitch;
        note->onset_numerator = onset.num;
        note->onset_denominator = onset.den;
        note->duration_numerator = duration.num;
        note->duration_denominator = duration.den;
        features->degrees[k] = motive_features_degree_label(pitch % 12, features->tonic_pc);
        bool failed = false;
        features->ncts[k] = note_nct(report, prototype->note_ids[k], start, &failed);
        if (failed) {
            return false;
        }
        if (k > 0) {
            features->intervals[k - 1] = pitch - features->notes[k - 1].pitch;
        }
    }
    Rational bar;
    Rational beat;
    Rational position;
    if (!meter_at(stage->form.input.map, first_start, &bar, &beat, &position)) {
        goto skip;
    }
    features->bar_numerator = bar.num;
    features->bar_denominator = bar.den;
    features->beat_numerator = beat.num;
    features->beat_denominator = beat.den;
    features->pickup_numerator = position.num;
    features->pickup_denominator = position.den;
    MotiveSketch sketch;
    if (!motive_features_to_sketch(features, &sketch)) {
        goto skip;
    }
    for (size_t k = 0; k + 1 < count; k++) {
        features->steps[k] = motive_sketch_step(&sketch, features->notes[k + 1].pitch, NULL) -
                             motive_sketch_step(&sketch, features->notes[k].pitch, NULL);
    }
    snprintf(features->fingerprint, sizeof features->fingerprint, "%016llx", motive_sketch_fingerprint(&sketch));
    motive->has_features = true;
    return true;
skip:
    motive_features_destroy(features);
    return true;
}

bool motive_stage_collect(const MotiveStage *stage, const ImportedNotes *notes, AnalysisReport *report) {
    size_t count = stage->motives.family_count;
    if (count == 0) {
        return true;
    }
    report->motives = calloc(count, sizeof(AnalysisMotive));
    if (!report->motives) {
        return false;
    }
    report->motive_count = count;
    for (size_t i = 0; i < count; i++) {
        const StandardFormMotiveFamily *found = &stage->motives.families[i];
        AnalysisMotive *motive = &report->motives[i];
        motive->index = i;
        motive->note_count = found->claim.note_count;
        motive->generic = found->claim.generic != 0;
        motive->occurrences = calloc(found->occurrence_count > 0 ? found->occurrence_count : 1,
                                     sizeof(AnalysisMotiveOccurrence));
        if (!motive->occurrences) {
            return false;
        }
        motive->occurrence_count = found->occurrence_count;
        for (size_t o = 0; o < found->occurrence_count; o++) {
            if (!collect_occurrence(stage, report, &found->occurrences[o], o, &motive->occurrences[o])) {
                return false;
            }
        }
        if (!order_development(motive)) {
            return false;
        }
        for (size_t o = 0; o < motive->occurrence_count; o++) {
            if (!collect_occurrence_degrees(notes, report, &motive->occurrences[o])) {
                return false;
            }
        }
        if (!collect_features(stage, notes, report, motive)) {
            return false;
        }
    }
    return true;
}

void motive_stage_destroy(MotiveStage *stage) {
    if (!stage) {
        return;
    }
    standard_form_motives_destroy(&stage->motives);
    form_stage_destroy(&stage->form);
    memset(stage, 0, sizeof(*stage));
}
