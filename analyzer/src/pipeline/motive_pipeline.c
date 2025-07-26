#include "pipeline/motive_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/motif_window_matching.h"
#include "key_vocabulary.h"
#include "outer_scale_degrees.h"
#include "pitch/pitch_numeric_value.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/* A soprano note: an index into the ImportedNotes, with exact start and end in whole notes. */
typedef struct {
    size_t index;
    long long start_n;
    long long start_d;
    long long end_n;
    long long end_d;
    long semitone;
} SopranoNote;

/* a/b compared with c/d for positive denominators: negative, zero or positive. */
static int compare_rational(long long a, long long b, long long c, long long d) {
    long long left = a * d;
    long long right = c * b;
    return left < right ? -1 : (left > right ? 1 : 0);
}

/* Sort by start, then by the order of the notes. */
static int compare_start(const void *left, const void *right) {
    const SopranoNote *a = left;
    const SopranoNote *b = right;
    int order = compare_rational(a->start_n, a->start_d, b->start_n, b->start_d);
    return order != 0 ? order : (a->index < b->index ? -1 : (a->index > b->index ? 1 : 0));
}

/* The soprano: at every onset the highest sounding note (the later attack among equals), kept when it is attacked
   there. Ported from motive_soprano; the active list holds the notes sounding at the onset instead of rescanning every
   earlier note. *out is owned, NULL when there are no notes. */
static bool find_soprano(const ImportedNotes *notes, SopranoNote **out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    if (notes->count == 0) {
        return true;
    }
    SopranoNote *all = calloc(notes->count, sizeof(SopranoNote));
    SopranoNote **active = calloc(notes->count, sizeof(SopranoNote *));
    SopranoNote *line = calloc(notes->count, sizeof(SopranoNote));
    if (!all || !active || !line) {
        free(all);
        free(active);
        free(line);
        return false;
    }
    for (size_t i = 0; i < notes->count; i++) {
        const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        const Duration *duration = note_event_duration(notes->events[i]);
        SopranoNote *note = &all[i];
        note->index = i;
        note->start_n = musical_time_numerator(start);
        note->start_d = (long long)musical_time_denominator(start);
        long long duration_n = duration_numerator(duration);
        long long duration_d = (long long)duration_denominator(duration);
        note->end_n = note->start_n * duration_d + duration_n * note->start_d;
        note->end_d = note->start_d * duration_d;
        note->semitone = pitch_numeric_value_position_numerator(notes->pitch_values[i]);
    }
    qsort(all, notes->count, sizeof(SopranoNote), compare_start);
    size_t active_count = 0;
    size_t kept = 0;
    for (size_t i = 0; i < notes->count;) {
        long long onset_n = all[i].start_n;
        long long onset_d = all[i].start_d;
        for (; i < notes->count && compare_rational(all[i].start_n, all[i].start_d, onset_n, onset_d) == 0; i++) {
            active[active_count++] = &all[i];
        }
        size_t live = 0;
        const SopranoNote *top = NULL;
        for (size_t a = 0; a < active_count; a++) {
            if (compare_rational(active[a]->end_n, active[a]->end_d, onset_n, onset_d) <= 0) {
                continue; /* ended before this onset */
            }
            active[live++] = active[a];
            if (!top || active[a]->semitone > top->semitone ||
                (active[a]->semitone == top->semitone &&
                 compare_rational(active[a]->start_n, active[a]->start_d, top->start_n, top->start_d) > 0)) {
                top = active[a];
            }
        }
        active_count = live;
        if (top && compare_rational(top->start_n, top->start_d, onset_n, onset_d) == 0) {
            line[kept++] = *top;
        }
    }
    free(all);
    free(active);
    *out = line;
    *out_count = kept;
    if (kept == 0) {
        free(line);
        *out = NULL;
    }
    return true;
}

/* The key region the time lies in, or the last one for a time past the end. Regions are ordered and gap-free. */
static const AnalysisKeyRegion *region_at(const AnalysisReport *report, long long n, long long d) {
    for (size_t r = 0; r < report->key_region_count; r++) {
        const AnalysisKeyRegion *region = &report->key_regions[r];
        if (compare_rational(n, d, region->end_numerator, (long long)region->end_denominator) < 0) {
            return region;
        }
    }
    return &report->key_regions[report->key_region_count - 1];
}

/* A degree label as the library names it: "1" to "7", with a "b" or "#" before it for an altered degree. */
static bool parse_degree(const char *label, int *degree, int *alteration) {
    *alteration = 0;
    if (!label) {
        return false;
    }
    if (*label == 'b') {
        *alteration = -1;
        label++;
    } else if (*label == '#') {
        *alteration = 1;
        label++;
    }
    if (*label < '1' || *label > '7') {
        return false;
    }
    *degree = *label - '0';
    return true;
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
        if (parse_degree(common_practice_scale_degree_label(
                             pitch_class, region->tonic_pitch_class,
                             region->is_major ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR),
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
        if (compare_rational(first_n, first_d, phrase->start_numerator, start_d) < 0 ||
            compare_rational(last_n, last_d, phrase->end_numerator, end_d) >= 0) {
            continue;
        }
        occurrence->has_phrase = true;
        occurrence->phrase = p;
        for (size_t k = 0; k < phrase->idea_count; k++) {
            const AnalysisIdea *idea = &report->ideas[phrase->first_idea + k];
            if (compare_rational(first_n, first_d, idea->start_numerator, (long long)idea->start_denominator) >= 0 &&
                compare_rational(last_n, last_d, idea->end_numerator, (long long)idea->end_denominator) < 0) {
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

bool motive_stage_collect(const MotiveStage *stage, AnalysisReport *report) {
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
