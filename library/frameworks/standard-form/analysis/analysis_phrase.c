#include "analysis_phrase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_time.h"
#include "core/occurrence.h"
#include "core/value.h"
#include "external_expansion_rules.h"
#include "identity/entity_id.h"
#include "phrase_rules.h"
#include "semantics/external_expansion_kind.h"

#define DISCONTINUITY_UNCERTAINTY 0.25

bool standard_form_cadence_ends_phrase(CadenceType type) {
    return standard_form_cadence_is_phrase_goal(type) || type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED;
}

typedef struct {
    StandardFormQ start;
    StandardFormQ end;
    int first_measure;
    int last_measure;
} Extent;

typedef struct {
    int measure;
    size_t note; /* the first note sounding in the measure */
    int cadence; /* the goal cadence on this point, -1 with none */
    bool discontinuity;
} Point;

static void destroy_phrase(StandardFormPhrase *phrase) {
    interpretation_destroy(phrase->interpretation);
    phrase_occurrence_destroy(phrase->occurrence);
    time_span_destroy(phrase->span);
    memset(phrase, 0, sizeof(*phrase));
}

void standard_form_phrases_destroy(StandardFormPhrases *phrases) {
    if (phrases == NULL) {
        return;
    }
    for (size_t i = 0; i < phrases->count; i++) {
        destroy_phrase(&phrases->items[i]);
    }
    free(phrases->items);
    phrases->items = NULL;
    phrases->count = 0;
}

bool standard_form_phrase_claim_read(const Interpretation *interpretation, StandardFormPhraseClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_FORM_PHRASE_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}

/* The end of the arrival chord snapped to a beat, and the measure it falls in (the last measure the arrival touches). */
static bool arrival_end(const StandardFormInput *input, const StandardFormCadence *cadence, StandardFormQ *snapped,
                        int *measure) {
    StandardFormQ end;
    if (!standard_form_span_q(cadence->arrival, NULL, &end) ||
        !standard_form_last_measure_touched(input->map, end, measure) ||
        !standard_form_snap_to_beat(input->map, *measure, end, snapped)) {
        return false;
    }
    return true;
}

static bool measure_start_of(const StandardFormInput *input, int measure, StandardFormQ *start) {
    return standard_form_measure_bounds(input->map, measure, start, NULL);
}

static bool measure_end_of(const StandardFormInput *input, int measure, StandardFormQ *end) {
    return standard_form_measure_bounds(input->map, measure, NULL, end);
}

static StandardFormAnalysisStatus build_phrase(const StandardFormInput *input, const Extent *extents,
                                               StandardFormPhrase *phrase, StandardFormQ end_time, double uncertainty) {
    StandardFormQ start_time;
    StandardFormPhraseClaim *claim = &phrase->claim;
    if (!measure_start_of(input, claim->first_measure, &start_time)) {
        return STANDARD_FORM_ANALYSIS_ERR_METER;
    }
    claim->counted_first = claim->first_measure < standard_form_first_counted_measure(input->map)
                               ? standard_form_first_counted_measure(input->map)
                               : claim->first_measure;
    claim->length_measures = claim->end_measure >= claim->counted_first ? claim->end_measure - claim->counted_first + 1 : 0;
    claim->extension_measures = claim->last_measure - claim->end_measure;
    claim->note_first = -1;
    claim->note_end = 0;
    for (size_t i = 0; i < input->note_count; i++) {
        if (extents[i].first_measure >= claim->first_measure && extents[i].first_measure <= claim->last_measure) {
            if (claim->note_first < 0) {
                claim->note_first = (int)i;
            }
            claim->note_end = (int)i + 1;
        }
    }
    if (claim->note_first < 0) {
        claim->note_first = claim->note_end;
    }
    phrase->uncertainty = uncertainty;
    phrase->span = standard_form_make_span(start_time, end_time);
    if (phrase->span == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_METER;
    }
    size_t count = (size_t)(claim->note_end - claim->note_first);
    EntityID **ids = (EntityID **)calloc(count > 0 ? count : 1, sizeof(EntityID *));
    if (ids == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    bool ok = count > 0;
    for (size_t k = 0; ok && k < count; k++) {
        ok = entity_id_create(input->notes[(size_t)claim->note_first + k].note_id, &ids[k]) == ENTITY_ID_OK;
    }
    char text[160];
    EntityID *occurrence_id = NULL;
    EntityID *phrase_id = NULL;
    Occurrence *occurrence = NULL;
    if (ok) {
        snprintf(text, sizeof(text), "standard-form-phrase-occurrence-%d", claim->index);
        ok = entity_id_create(text, &occurrence_id) == ENTITY_ID_OK;
    }
    if (ok) {
        snprintf(text, sizeof(text), "standard-form-phrase-%d", claim->index);
        ok = entity_id_create(text, &phrase_id) == ENTITY_ID_OK;
    }
    bool ids_owned = false;
    if (ok && occurrence_create(occurrence_id, phrase_id, &occurrence) == OCCURRENCE_OK) {
        occurrence_id = NULL;
        phrase_id = NULL;
        if (phrase_occurrence_create(occurrence, ids, count, &phrase->occurrence) != PHRASE_OCCURRENCE_OK) {
            occurrence_destroy(occurrence);
            phrase->occurrence = NULL;
            ok = false;
        } else {
            ids_owned = true; /* the entries are the occurrence's now; the array is ours */
        }
    } else {
        ok = false;
    }
    if (!ok) {
        entity_id_destroy(occurrence_id);
        entity_id_destroy(phrase_id);
        for (size_t k = 0; k < count; k++) {
            entity_id_destroy(ids[k]);
        }
        free(ids);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    (void)ids_owned;
    free(ids);
    snprintf(text, sizeof(text), "%d", claim->index);
    char interpretation_id[200];
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-form-phrase-%s", text);
    phrase->interpretation = standard_form_analysis_interpretation(
        interpretation_id, entity_id_value(phrase_occurrence_id(phrase->occurrence)), STANDARD_FORM_PHRASE_CLAIM_TYPE, claim,
        sizeof(*claim), uncertainty, "phrase-segmentation");
    return phrase->interpretation != NULL ? STANDARD_FORM_ANALYSIS_OK : STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

StandardFormAnalysisStatus standard_form_segment_phrases(const StandardFormInput *input, const StandardFormConfig *config,
                                                         StandardFormBoundaryDetector detector, void *detector_context,
                                                         StandardFormPhrases *out) {
    if (input == NULL || out == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardFormConfig defaults = standard_form_config_default();
    const StandardFormConfig *effective = config != NULL ? config : &defaults;
    if (!standard_form_config_valid(effective) || !input->finished) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG;
    }
    if (detector == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR;
    }
    StandardFormPhrases found = {NULL, 0};
    if (input->note_count == 0) {
        *out = found;
        return STANDARD_FORM_ANALYSIS_OK;
    }
    size_t max_length =
        effective->max_length_without_cadence == 0 ? STANDARD_FORM_DEFAULT_MAX_LENGTH : effective->max_length_without_cadence;

    Extent *extents = (Extent *)calloc(input->note_count, sizeof(Extent));
    if (extents == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    int first_measure = 0;
    int last_measure = 0;
    for (size_t i = 0; i < input->note_count; i++) {
        if (!standard_form_span_q(input->notes[i].span, &extents[i].start, &extents[i].end) ||
            !standard_form_measure_at(input->map, extents[i].start, &extents[i].first_measure) ||
            !standard_form_last_measure_touched(input->map, extents[i].end, &extents[i].last_measure)) {
            free(extents);
            return STANDARD_FORM_ANALYSIS_ERR_METER;
        }
        if (extents[i].last_measure < extents[i].first_measure) {
            extents[i].last_measure = extents[i].first_measure;
        }
        if (i == 0 || extents[i].first_measure < first_measure) {
            first_measure = extents[i].first_measure;
        }
        if (i == 0 || extents[i].last_measure > last_measure) {
            last_measure = extents[i].last_measure;
        }
    }

    /* the points: one per measure with a sounding note */
    size_t measure_count = (size_t)(last_measure - first_measure + 1);
    Point *points = (Point *)calloc(measure_count, sizeof(Point));
    StandardFormBoundaryPoint *detector_points = (StandardFormBoundaryPoint *)calloc(measure_count, sizeof(StandardFormBoundaryPoint));
    StandardFormBoundary *boundaries = (StandardFormBoundary *)calloc(measure_count, sizeof(StandardFormBoundary));
    if (points == NULL || detector_points == NULL || boundaries == NULL) {
        free(extents);
        free(points);
        free(detector_points);
        free(boundaries);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    size_t point_count = 0;
    for (int m = first_measure; m <= last_measure; m++) {
        for (size_t i = 0; i < input->note_count; i++) {
            if (extents[i].first_measure <= m && m <= extents[i].last_measure) {
                points[point_count].measure = m;
                points[point_count].note = i;
                points[point_count].cadence = -1;
                point_count++;
                break;
            }
        }
    }
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    for (size_t k = 0; k + 1 < point_count && status == STANDARD_FORM_ANALYSIS_OK; k++) {
        const Point *next = &points[k + 1];
        bool broken = next->measure > points[k].measure + 1;
        StandardFormQ barline;
        if (!broken) {
            if (!measure_start_of(input, next->measure, &barline)) {
                status = STANDARD_FORM_ANALYSIS_ERR_METER;
                break;
            }
            const StandardFormNote *first = &input->notes[next->note];
            if (standard_form_q_compare(extents[next->note].start, barline) > 0) {
                broken = true; /* a rest at the barline */
            } else if (next->note > 0 && input->notes[next->note - 1].has_next &&
                       labs(input->notes[next->note - 1].semitones_to_next) >= effective->discontinuity_leap_semitones &&
                       standard_form_q_compare(extents[next->note].start, barline) == 0) {
                broken = true; /* a leap into the first note */
            }
            (void)first;
        }
        points[k].discontinuity = broken;
    }
    /* the cadence signal */
    for (size_t c = 0; c < input->cadence_count && status == STANDARD_FORM_ANALYSIS_OK; c++) {
        const StandardFormCadence *cadence = &input->cadences[c];
        if (cadence->post_cadential || !standard_form_cadence_ends_phrase(cadence->type)) {
            continue;
        }
        StandardFormQ start;
        int measure = 0;
        if (!standard_form_span_q(cadence->arrival, &start, NULL) || !standard_form_measure_at(input->map, start, &measure)) {
            status = STANDARD_FORM_ANALYSIS_ERR_METER;
            break;
        }
        if (measure < first_measure) {
            continue;
        }
        size_t at = 0;
        for (size_t k = 0; k < point_count; k++) {
            if (points[k].measure <= measure) {
                at = k;
            }
        }
        if (points[at].cadence < 0 || cadence->strength_rank > input->cadences[points[at].cadence].strength_rank) {
            points[at].cadence = (int)c;
        }
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        free(extents);
        free(points);
        free(detector_points);
        free(boundaries);
        return status;
    }
    for (size_t k = 0; k < point_count; k++) {
        detector_points[k].event = input->notes[points[k].note].event;
        detector_points[k].has_cadence = points[k].cadence >= 0;
        detector_points[k].cadence_type = points[k].cadence >= 0 ? input->cadences[points[k].cadence].type : CADENCE_TYPE_UNKNOWN;
        detector_points[k].has_discontinuity = points[k].discontinuity;
    }
    status = detector(detector_points, point_count, max_length, boundaries, detector_context);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        free(extents);
        free(points);
        free(detector_points);
        free(boundaries);
        return status;
    }

    /* the phrases */
    found.items = (StandardFormPhrase *)calloc(point_count + 1, sizeof(StandardFormPhrase));
    if (found.items == NULL) {
        status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    int current_first = first_measure;
    bool last_by_cadence = false;
    for (size_t k = 0; k < point_count && status == STANDARD_FORM_ANALYSIS_OK; k++) {
        if (!boundaries[k].is_boundary || points[k].measure < current_first) {
            continue;
        }
        StandardFormPhrase *phrase = &found.items[found.count];
        StandardFormPhraseClaim *claim = &phrase->claim;
        claim->index = (int)found.count;
        claim->first_measure = current_first;
        claim->cadence_measure = -1;
        claim->cadence_index = -1;
        claim->cadence_type = -1;
        claim->extension_kind = -1;
        StandardFormQ end_time;
        double uncertainty;
        bool by_cadence = boundaries[k].by_cadence && points[k].cadence >= 0;
        if (by_cadence) {
            const StandardFormCadence *cadence = &input->cadences[points[k].cadence];
            int ended = 0;
            if (!arrival_end(input, cadence, &end_time, &ended)) {
                status = STANDARD_FORM_ANALYSIS_ERR_METER;
                break;
            }
            claim->signal = (int)STANDARD_FORM_SIGNAL_CADENCE;
            claim->cadence_index = points[k].cadence;
            claim->cadence_type = (int)cadence->type;
            claim->strength_rank = cadence->strength_rank;
            claim->cadence_measure = points[k].measure;
            claim->end_measure = ended > points[k].measure ? ended : points[k].measure;
            if (claim->end_measure < current_first) {
                claim->end_measure = current_first;
            }
            uncertainty = cadence->uncertainty;
        } else {
            claim->signal = (int)STANDARD_FORM_SIGNAL_DISCONTINUITY;
            claim->end_measure = points[k].measure;
            if (!measure_end_of(input, claim->end_measure, &end_time)) {
                status = STANDARD_FORM_ANALYSIS_ERR_METER;
                break;
            }
            uncertainty = DISCONTINUITY_UNCERTAINTY;
        }
        claim->last_measure = claim->end_measure;
        /* post-cadential absorption: a post-cadential cadence between this boundary and the next one */
        if (by_cadence) {
            int next_boundary = last_measure + 1;
            for (size_t j = k + 1; j < point_count; j++) {
                if (boundaries[j].is_boundary) {
                    next_boundary = points[j].measure;
                    break;
                }
            }
            for (size_t c = 0; c < input->cadence_count; c++) {
                const StandardFormCadence *post = &input->cadences[c];
                StandardFormQ start;
                int measure = 0;
                if (!post->post_cadential || !standard_form_span_q(post->arrival, &start, NULL) ||
                    !standard_form_measure_at(input->map, start, &measure) || measure < points[k].measure ||
                    measure >= next_boundary) {
                    continue;
                }
                StandardFormQ post_end;
                int ended = 0;
                if (!arrival_end(input, post, &post_end, &ended) || ended < claim->last_measure ||
                    ended - claim->end_measure > (int)effective->max_absorbed_tail) {
                    continue;
                }
                claim->last_measure = ended;
                end_time = post_end;
                claim->extension_kind = (int)EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION;
            }
        }
        status = build_phrase(input, extents, phrase, end_time, uncertainty);
        if (status != STANDARD_FORM_ANALYSIS_OK) {
            found.count++;
            break;
        }
        found.count++;
        current_first = claim->last_measure + 1;
        last_by_cadence = by_cadence;
    }

    /* the tail after the last boundary */
    if (status == STANDARD_FORM_ANALYSIS_OK && current_first <= last_measure) {
        int tail = last_measure - current_first + 1;
        if (found.count > 0 && last_by_cadence && tail <= (int)effective->max_absorbed_tail) {
            StandardFormPhrase *last = &found.items[found.count - 1];
            StandardFormQ end_time;
            last->claim.last_measure = last_measure;
            last->claim.extension_kind = (int)(last->claim.last_measure - last->claim.end_measure <= 2
                                                   ? EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION
                                                   : EXTERNAL_EXPANSION_KIND_CODETTA);
            if (!measure_end_of(input, last_measure, &end_time)) {
                status = STANDARD_FORM_ANALYSIS_ERR_METER;
            } else {
                double uncertainty = last->uncertainty;
                StandardFormPhraseClaim claim = last->claim;
                destroy_phrase(last);
                last->claim = claim;
                status = build_phrase(input, extents, last, end_time, uncertainty);
            }
        } else {
            StandardFormPhrase *phrase = &found.items[found.count];
            StandardFormPhraseClaim *claim = &phrase->claim;
            StandardFormQ end_time;
            claim->index = (int)found.count;
            claim->first_measure = current_first;
            claim->end_measure = last_measure;
            claim->last_measure = last_measure;
            claim->cadence_measure = -1;
            claim->cadence_index = -1;
            claim->cadence_type = -1;
            claim->extension_kind = -1;
            claim->signal = (int)STANDARD_FORM_SIGNAL_NONE;
            if (!measure_end_of(input, last_measure, &end_time)) {
                status = STANDARD_FORM_ANALYSIS_ERR_METER;
            } else {
                status = build_phrase(input, extents, phrase, end_time, effective->no_signal_uncertainty);
            }
            found.count++;
        }
    }
    free(extents);
    free(points);
    free(detector_points);
    free(boundaries);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        standard_form_phrases_destroy(&found);
        return status;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_FORM_ANALYSIS_OK;
}
