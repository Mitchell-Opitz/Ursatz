#include "analysis_melody_nct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_melody_emit.h"
#include "core/rational.h"
#include "core/value.h"
#include "pitch/pitch_numeric_value.h"
#include "time/musical_time.h"
#include "time/time_span.h"

static bool read_claim(const Interpretation *interpretation, const char *type, void *out, size_t size) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t content_size = 0;
    if (claim == NULL || strcmp(value_type(claim), type) != 0) {
        return false;
    }
    const void *content = value_content(claim, &content_size);
    if (content == NULL || content_size != size) {
        return false;
    }
    memcpy(out, content, size);
    return true;
}

static void destroy_label(StandardCounterpointNctLabel *label) {
    interpretation_destroy(label->interpretation);
    structure_destroy(label->shell);
    memset(label, 0, sizeof(*label));
}

void standard_counterpoint_nct_labels_destroy(StandardCounterpointNctLabels *labels) {
    if (labels == NULL) {
        return;
    }
    for (size_t i = 0; i < labels->count; i++) {
        destroy_label(&labels->items[i]);
    }
    free(labels->items);
    labels->items = NULL;
    labels->count = 0;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_nct_labels(const StandardCounterpointMelodyLine *line,
                                                                         StandardCounterpointNctLabels *out) {
    if (line == NULL || out == NULL || (line->count > 0 && line->notes == NULL)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointNctLabels found = {NULL, 0};
    if (line->count > 0) {
        found.items = (StandardCounterpointNctLabel *)calloc(line->count, sizeof(StandardCounterpointNctLabel));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < line->count; i++) {
        const StandardCounterpointMelodyNote *note = &line->notes[i];
        if (!note->present) {
            continue;
        }
        StandardCounterpointNctLabel *label = &found.items[found.count];
        StandardCounterpointNctClaim *claim = &label->claim;
        claim->index = (int)i;
        claim->status = (int)note->nct;
        claim->type = note->nct == STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE ? (int)note->nct_type : -1;
        if (note->nct_ref != NULL) {
            claim->has_ref = 1;
            snprintf(claim->ref, sizeof(claim->ref), "%s", note->nct_ref);
        }
        label->uncertainty = 1.0 - note->confidence;
        const char *ids[1] = {note->note_id};
        if (!standard_counterpoint_melody_emit("melodic-nct", note->note_id, ids, 1,
                                               STANDARD_COUNTERPOINT_NCT_CLAIM_TYPE, claim, sizeof(*claim),
                                               label->uncertainty, &label->shell, &label->interpretation)) {
            standard_counterpoint_nct_labels_destroy(&found);
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
        found.count++;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

static Rational time_of(const MusicalTime *time) {
    return rational_ratio(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

/* The classifier's labels for notes[first, first + count) written into the copies in out. */
static StandardCounterpointMelodyStatus relabel_run(StandardCounterpointMelodyNote *notes, size_t first, size_t count,
                                                    NonChordToneClassifier classifier, void *context) {
    ToneLineNote *line = (ToneLineNote *)malloc(count * sizeof(ToneLineNote));
    ToneLineLabel *labels = (ToneLineLabel *)malloc(count * sizeof(ToneLineLabel));
    if (line == NULL || labels == NULL) {
        free(line);
        free(labels);
        return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
    }
    for (size_t k = 0; k < count; k++) {
        const StandardCounterpointMelodyNote *note = &notes[first + k];
        line[k].midi = (int)pitch_numeric_value_position_numerator(note->pitch);
        if (note->span != NULL) {
            line[k].onset = time_of(time_span_start(note->span));
            if (rational_sub(time_of(time_span_end(note->span)), line[k].onset, &line[k].duration) != RATIONAL_OK) {
                line[k].duration = rational_ratio(1, 1);
            }
        } else {
            line[k].onset = rational_ratio((long long)(first + k), 1);
            line[k].duration = rational_ratio(1, 1);
        }
        line[k].chord_pitch_classes = note->chord_pitch_classes;
    }
    if (classifier(line, count, labels, context)) {
        for (size_t k = 0; k < count; k++) {
            StandardCounterpointMelodyNote *note = &notes[first + k];
            if (labels[k].chord_tone) {
                note->nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
            } else if (labels[k].classified && non_chord_tone_valid(labels[k].type)) {
                note->nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
                note->nct_type = labels[k].type;
            } else {
                note->nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
            }
        }
    }
    free(line);
    free(labels);
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_nct_labels_with(const StandardCounterpointMelodyLine *line,
                                                                              NonChordToneClassifier classifier,
                                                                              void *context,
                                                                              StandardCounterpointNctLabels *out) {
    if (classifier == NULL || line == NULL || out == NULL || line->count == 0 || line->notes == NULL) {
        return standard_counterpoint_melody_nct_labels(line, out);
    }
    StandardCounterpointMelodyNote *copy =
        (StandardCounterpointMelodyNote *)malloc(line->count * sizeof(StandardCounterpointMelodyNote));
    if (copy == NULL) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
    }
    memcpy(copy, line->notes, line->count * sizeof(StandardCounterpointMelodyNote));
    size_t i = 0;
    while (i < line->count) {
        if (!copy[i].present || copy[i].chord_pitch_classes == 0) {
            i++;
            continue;
        }
        size_t first = i;
        while (i < line->count && copy[i].present && copy[i].chord_pitch_classes != 0) {
            i++;
        }
        StandardCounterpointMelodyStatus status = relabel_run(copy, first, i - first, classifier, context);
        if (status != STANDARD_COUNTERPOINT_MELODY_OK) {
            free(copy);
            return status;
        }
    }
    StandardCounterpointMelodyLine view = {copy, line->count, line->count};
    StandardCounterpointMelodyStatus status = standard_counterpoint_melody_nct_labels(&view, out);
    free(copy);
    return status;
}

bool standard_counterpoint_nct_claim_read(const Interpretation *interpretation, StandardCounterpointNctClaim *out) {
    return read_claim(interpretation, STANDARD_COUNTERPOINT_NCT_CLAIM_TYPE, out, sizeof(*out));
}

static void destroy_counts(StandardCounterpointNctCounts *counts) {
    interpretation_destroy(counts->interpretation);
    structure_destroy(counts->shell);
    memset(counts, 0, sizeof(*counts));
}

void standard_counterpoint_nct_count_list_destroy(StandardCounterpointNctCountList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        destroy_counts(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_nct_counts(const StandardCounterpointMelodyLine *line,
                                                                         const StandardCounterpointMelodySpan *spans,
                                                                         size_t span_count,
                                                                         StandardCounterpointNctCountList *out) {
    if (line == NULL || out == NULL || (span_count > 0 && spans == NULL) || (line->count > 0 && line->notes == NULL)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointMelodySpan whole = {"whole-line", 0, line->count > 0 ? line->count - 1 : 0};
    if (spans == NULL) {
        spans = &whole;
        span_count = line->count > 0 ? 1 : 0;
    }
    for (size_t s = 0; s < span_count; s++) {
        if (spans[s].id == NULL || spans[s].id[0] == '\0' || spans[s].first > spans[s].last ||
            spans[s].last >= line->count) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_BAD_SPAN;
        }
    }
    StandardCounterpointNctCountList found = {NULL, 0};
    if (span_count > 0) {
        found.items = (StandardCounterpointNctCounts *)calloc(span_count, sizeof(StandardCounterpointNctCounts));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t s = 0; s < span_count; s++) {
        const StandardCounterpointMelodySpan *span = &spans[s];
        StandardCounterpointNctCountsClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.span_first = (int)span->first;
        claim.span_last = (int)span->last;
        const char **ids = (const char **)calloc(span->last - span->first + 1, sizeof(char *));
        if (ids == NULL) {
            standard_counterpoint_nct_count_list_destroy(&found);
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
        double least = 1.0;
        for (size_t i = span->first; i <= span->last; i++) {
            const StandardCounterpointMelodyNote *note = &line->notes[i];
            if (!note->present) {
                continue;
            }
            ids[claim.note_count++] = note->note_id;
            switch (note->nct) {
                case STANDARD_COUNTERPOINT_NCT_CHORD_TONE:
                    claim.chord_tones++;
                    break;
                case STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE:
                    claim.non_chord_tones++;
                    claim.by_type[note->nct_type]++;
                    break;
                default:
                    claim.unclassified++;
                    break;
            }
            if (note->confidence < least) {
                least = note->confidence;
            }
        }
        if (claim.note_count == 0) {
            free(ids);
            continue;
        }
        StandardCounterpointNctCounts *counts = &found.items[found.count];
        snprintf(counts->span_id, sizeof(counts->span_id), "%s", span->id);
        counts->claim = claim;
        counts->uncertainty = 1.0 - least;
        bool ok = standard_counterpoint_melody_emit("nct-counts", span->id, ids, (size_t)claim.note_count,
                                                    STANDARD_COUNTERPOINT_NCT_COUNTS_CLAIM_TYPE, &claim, sizeof(claim),
                                                    counts->uncertainty, &counts->shell, &counts->interpretation);
        free(ids);
        if (!ok) {
            standard_counterpoint_nct_count_list_destroy(&found);
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
        found.count++;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

bool standard_counterpoint_nct_counts_claim_read(const Interpretation *interpretation,
                                                 StandardCounterpointNctCountsClaim *out) {
    return read_claim(interpretation, STANDARD_COUNTERPOINT_NCT_COUNTS_CLAIM_TYPE, out, sizeof(*out));
}
