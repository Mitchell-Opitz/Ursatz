#include "chain/chain_inputs_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/harmony/harmony_cadence_rules.h"
#include "analysis/harmony/outer_scale_degrees.h"
#include "model/pitch/midi_pitch.h"

/* --- the form input --- */

/* Keeps a copy of a soprano note, which the input borrows. */
static bool form_keep_note(ChainFormInput *build, const AnalysisNote *note) {
    if (build->note_count == build->note_capacity) {
        size_t capacity = build->note_capacity == 0 ? 256 : build->note_capacity * 2;
        AnalysisNote *grown = realloc(build->notes, capacity * sizeof(AnalysisNote));
        if (grown == NULL) {
            return false;
        }
        build->notes = grown;
        build->note_capacity = capacity;
    }
    build->notes[build->note_count++] = *note;
    return true;
}

void chain_form_input_destroy(ChainFormInput *input) {
    if (input == NULL) {
        return;
    }
    standard_form_input_destroy(&input->input);
    free(input->notes);
    free(input->degrees);
    chain_release_spans(input->spans, input->span_count);
    memset(input, 0, sizeof(*input));
}

/* The note with this id, or NULL. `keys` is the set's ids sorted. */
static const AnalysisNote *find_note(const AnalysisNoteSet *notes, const NoteKey *keys, const char *id) {
    NoteKey probe = {id, 0};
    const NoteKey *found = bsearch(&probe, keys, notes->count, sizeof(NoteKey), chain_compare_keys);
    return found != NULL ? &notes->notes[found->index] : NULL;
}

static NoteKey *sorted_keys(const AnalysisNoteSet *notes) {
    NoteKey *keys = malloc((notes->count > 0 ? notes->count : 1) * sizeof(NoteKey));
    if (keys == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < notes->count; i++) {
        keys[i].id = notes->notes[i].note_id;
        keys[i].index = i;
    }
    qsort(keys, notes->count, sizeof(NoteKey), chain_compare_keys);
    return keys;
}

static ChainInputStatus add_cadences(ChainFormInput *build, const ChainSegments *segments, const HarmonyCadences *cadences) {
    for (size_t i = 0; cadences != NULL && i < cadences->count; i++) {
        const HarmonyCadenceResult *c = &cadences->items[i];
        if (c->arrival_first >= segments->count || c->arrival_last >= segments->count) {
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
        const TimeSpan *first = sonority_time_window(segments->sonorities.items[c->arrival_first].sonority);
        const TimeSpan *last = sonority_time_window(segments->sonorities.items[c->arrival_last].sonority);
        const TimeSpan *span = chain_keep_span(&build->spans, &build->span_count, &build->span_capacity, time_span_start(first), time_span_end(last));
        char id[32];
        snprintf(id, sizeof(id), "chain-%zu", i);
        if (span == NULL) {
            return CHAIN_INPUT_ERR_ALLOC;
        }
        if (standard_form_input_add_cadence(&build->input, id, span, c->type, harmony_cadence_strength(c->type), c->post_cadential,
                                            c->uncertainty) != STANDARD_FORM_ANALYSIS_OK) {
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    return CHAIN_INPUT_OK;
}

ChainInputStatus chain_build_form_input(const AnalysisNoteSet *notes, const ChainSegments *segments,
                                        const ChainSegments *line_segments, const OuterVoiceLines *lines, int first_measure, int last_measure, const HarmonyCadences *cadences, ChainFormInput *out) {
    if (notes == NULL || segments == NULL || line_segments == NULL || lines == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || segments->count == 0 || line_segments->count == 0 ||
        lines->count < line_segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    NoteKey *keys = sorted_keys(notes);
    if (keys == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainFormInput build;
    memset(&build, 0, sizeof(build));
    ChainInputStatus status = CHAIN_INPUT_OK;
    const char *current_id = NULL;
    const MusicalTime *current_start = NULL;
    const MusicalTime *current_end = NULL;
    int current_semitone = 0;
    for (size_t s = 0; s <= line_segments->count && status == CHAIN_INPUT_OK; s++) {
        const TimeSpan *window = s < line_segments->count ? sonority_time_window(line_segments->sonorities.items[s].sonority) : NULL;
        int measure = window != NULL ? chain_measure_of(notes->map, time_span_start(window)) : -1;
        bool in_range = window != NULL && measure >= first_measure && measure <= last_measure;
        const char *id = in_range && lines->soprano[s].present ? entity_id_value(lines->soprano[s].note_id) : NULL;
        if (current_id != NULL && (id == NULL || strcmp(id, current_id) != 0)) {
            const AnalysisNote *source = find_note(notes, keys, current_id);
            const TimeSpan *span = source != NULL ? chain_keep_span(&build.spans, &build.span_count, &build.span_capacity, current_start, current_end) : NULL;
            if (source == NULL || span == NULL) {
                status = source == NULL ? CHAIN_INPUT_ERR_UNKNOWN_NOTE : CHAIN_INPUT_ERR_ALLOC;
                break;
            }
            AnalysisNote line_note = {current_id, source->event, span, current_semitone, NULL, NULL, ANALYSIS_INDEX_NONE,
                                      ANALYSIS_INDEX_NONE, 1.0};
            if (!form_keep_note(&build, &line_note)) {
                status = CHAIN_INPUT_ERR_ALLOC;
                break;
            }
            current_id = NULL;
        }
        if (id == NULL) {
            continue;
        }
        if (current_id == NULL) {
            current_id = id;
            current_start = time_span_start(window);
            current_semitone = (int)pitch_numeric_value_position_numerator(lines->soprano[s].pitch);
        }
        current_end = time_span_end(window);
    }
    free(keys);
    /* The input borrows the notes, which are all kept now. */
    standard_form_input_init(&build.input, notes->map, build.notes, build.note_count);
    if (status == CHAIN_INPUT_OK) {
        status = add_cadences(&build, segments, cadences);
    }
    if (status == CHAIN_INPUT_OK && standard_form_input_finish(&build.input) != STANDARD_FORM_ANALYSIS_OK) {
        status = CHAIN_INPUT_ERR_BAD_NOTES;
    }
    if (status != CHAIN_INPUT_OK) {
        chain_form_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}

typedef struct {
    size_t index; /* into the note set */
    Rational start;
    Rational end;
    long semitone;
} MotiveNote;

static Rational q_of_time(const MusicalTime *time) {
    return rational_ratio(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

/* The soprano notes of the measures [first, last], in time order. *out is allocated; its count is returned.
   Only notes starting in the measures compete, so a note held in from an earlier measure does not hide an attack. */
static bool motive_soprano(const AnalysisNoteSet *notes, int first_measure, int last_measure, MotiveNote **out, size_t *kept_out) {
    AnalysisNote *window = calloc(notes->count > 0 ? notes->count : 1, sizeof(AnalysisNote));
    size_t *origin = calloc(notes->count > 0 ? notes->count : 1, sizeof(size_t));
    if (window == NULL || origin == NULL) {
        free(window);
        free(origin);
        return false;
    }
    size_t count = 0;
    for (size_t i = 0; i < notes->count; i++) {
        int measure = chain_measure_of(notes->map, time_span_start(notes->notes[i].span));
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        window[count] = notes->notes[i];
        origin[count] = i;
        count++;
    }
    AnalysisNoteSet in_window = {window, count, notes->map};
    size_t *picked = NULL;
    size_t kept = 0;
    MotiveNote *line = NULL;
    bool ok = count == 0 || outer_voice_attack_soprano(&in_window, &picked, &kept) == OUTER_VOICE_OK;
    if (ok) {
        line = calloc(kept > 0 ? kept : 1, sizeof(MotiveNote));
        ok = line != NULL;
    }
    for (size_t k = 0; ok && k < kept; k++) {
        const AnalysisNote *source = &window[picked[k]];
        line[k].index = origin[picked[k]];
        line[k].start = q_of_time(time_span_start(source->span));
        line[k].end = q_of_time(time_span_end(source->span));
        line[k].semitone = source->midi;
    }
    free(window);
    free(origin);
    free(picked);
    if (!ok) {
        free(line);
        return false;
    }
    *out = line;
    *kept_out = kept;
    return true;
}

ChainInputStatus chain_build_motive_input(const AnalysisNoteSet *notes, const ChainSegments *segments, int first_measure,
                                          int last_measure, int tonic_pitch_class, CommonPracticeKeyMode mode,
                                          const HarmonyCadences *cadences, ChainFormInput *out) {
    if (notes == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    MotiveNote *line = NULL;
    size_t count = 0;
    if (!motive_soprano(notes, first_measure, last_measure, &line, &count)) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainFormInput build;
    memset(&build, 0, sizeof(build));
    ChainInputStatus status = CHAIN_INPUT_OK;
    build.degrees = (int *)calloc(count > 0 ? count : 1, sizeof(int));
    if (build.degrees == NULL) {
        status = CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t k = 0; k < count && status == CHAIN_INPUT_OK; k++) {
        const AnalysisNote *source = &notes->notes[line[k].index];
        /* the span is the note's own, borrowed from the note set */
        if (!form_keep_note(&build, source)) {
            status = CHAIN_INPUT_ERR_ALLOC;
            break;
        }
        int pitch_class = pitch_class_of_midi((int)line[k].semitone);
        int degree = 0;
        int alteration = 0;
        if (!chain_parse_degree(common_practice_scale_degree_label(pitch_class, tonic_pitch_class, mode), &degree, &alteration)) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
            break;
        }
        build.degrees[k] = degree;
    }
    free(line);
    /* The input borrows the notes and degrees, which are all filled in now. */
    standard_form_input_init(&build.input, notes->map, build.notes, build.note_count);
    build.input.degrees = build.degrees;
    if (status == CHAIN_INPUT_OK) {
        status = add_cadences(&build, segments, cadences);
    }
    if (status == CHAIN_INPUT_OK && standard_form_input_finish(&build.input) != STANDARD_FORM_ANALYSIS_OK) {
        status = CHAIN_INPUT_ERR_BAD_NOTES;
    }
    if (status != CHAIN_INPUT_OK) {
        chain_form_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}

/* One cadence of the grid input: arriving over the window of its first arrival segment only, not the run. */
static ChainInputStatus add_grid_cadence(ChainFormInput *build, const ChainSegments *segments, size_t *serial, size_t arrival_first,
                                         CadenceType type, bool post_cadential, double uncertainty) {
    if (arrival_first >= segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    const TimeSpan *window = sonority_time_window(segments->sonorities.items[arrival_first].sonority);
    const TimeSpan *span = chain_keep_span(&build->spans, &build->span_count, &build->span_capacity, time_span_start(window), time_span_end(window));
    if (span == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    char id[32];
    snprintf(id, sizeof(id), "grid-cadence-%zu", (*serial)++);
    if (standard_form_input_add_cadence(&build->input, id, span, type, harmony_cadence_strength(type), post_cadential, uncertainty) !=
        STANDARD_FORM_ANALYSIS_OK) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    return CHAIN_INPUT_OK;
}

static ChainInputStatus add_grid_cadences(ChainFormInput *build, const ChainSegments *segments, const HarmonyCadences *results,
                                          size_t *serial) {
    for (size_t i = 0; results != NULL && i < results->count; i++) {
        const HarmonyCadenceResult *c = &results->items[i];
        ChainInputStatus status =
            add_grid_cadence(build, segments, serial, c->arrival_first, (CadenceType)c->type, c->post_cadential, c->uncertainty);
        if (status == CHAIN_INPUT_OK && c->alternative.present && c->alternative.reading == HARMONY_CADENCE_READING_CADENCE) {
            status = add_grid_cadence(build, segments, serial, c->alternative.arrival_first, c->alternative.type, false, c->uncertainty);
        }
        if (status != CHAIN_INPUT_OK) {
            return status;
        }
    }
    return CHAIN_INPUT_OK;
}

ChainInputStatus chain_build_grid_input(const AnalysisNoteSet *notes, const ChainSegments *segments, int first_measure, int last_measure,
                                        const HarmonyCadences *found, const HarmonyCadences *readings, ChainFormInput *out) {
    if (notes == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    MotiveNote *line = NULL;
    size_t count = 0;
    if (!motive_soprano(notes, first_measure, last_measure, &line, &count)) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainFormInput build;
    memset(&build, 0, sizeof(build));
    ChainInputStatus status = CHAIN_INPUT_OK;
    for (size_t k = 0; k < count && status == CHAIN_INPUT_OK; k++) {
        /* the span is the note's own, borrowed from the note set */
        if (!form_keep_note(&build, &notes->notes[line[k].index])) {
            status = CHAIN_INPUT_ERR_ALLOC;
        }
    }
    free(line);
    /* The input borrows the notes, which are all kept now. */
    standard_form_input_init(&build.input, notes->map, build.notes, build.note_count);
    size_t serial = 0;
    if (status == CHAIN_INPUT_OK) {
        status = add_grid_cadences(&build, segments, found, &serial);
    }
    if (status == CHAIN_INPUT_OK) {
        status = add_grid_cadences(&build, segments, readings, &serial);
    }
    if (status == CHAIN_INPUT_OK && standard_form_input_finish(&build.input) != STANDARD_FORM_ANALYSIS_OK) {
        status = CHAIN_INPUT_ERR_BAD_NOTES;
    }
    if (status != CHAIN_INPUT_OK) {
        chain_form_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}
