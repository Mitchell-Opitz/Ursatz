#include "analysis_melody_climax.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_melody_emit.h"
#include "core/value.h"

static void destroy_climax(StandardCounterpointClimax *climax) {
    interpretation_destroy(climax->interpretation);
    structure_destroy(climax->shell);
    memset(climax, 0, sizeof(*climax));
}

void standard_counterpoint_climaxes_destroy(StandardCounterpointClimaxes *climaxes) {
    if (climaxes == NULL) {
        return;
    }
    for (size_t i = 0; i < climaxes->count; i++) {
        destroy_climax(&climaxes->items[i]);
    }
    free(climaxes->items);
    climaxes->items = NULL;
    climaxes->count = 0;
}

static long semitone_of(const StandardCounterpointMelodyNote *note) {
    return pitch_numeric_value_position_numerator(note->pitch);
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_climaxes(const StandardCounterpointMelodyLine *line,
                                                                       const StandardCounterpointMelodicIntervals *intervals,
                                                                       const StandardCounterpointMelodySpan *spans,
                                                                       size_t span_count,
                                                                       StandardCounterpointClimaxes *out) {
    if (line == NULL || intervals == NULL || out == NULL || (span_count > 0 && spans == NULL) ||
        (line->count > 0 && line->notes == NULL)) {
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
    StandardCounterpointClimaxes found = {NULL, 0};
    if (span_count > 0) {
        found.items = (StandardCounterpointClimax *)calloc(span_count, sizeof(StandardCounterpointClimax));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t s = 0; s < span_count; s++) {
        const StandardCounterpointMelodySpan *span = &spans[s];
        size_t notes = 0;
        size_t first_top = line->count;
        size_t last_note = 0;
        double least = 1.0;
        long top = 0;
        for (size_t i = span->first; i <= span->last; i++) {
            const StandardCounterpointMelodyNote *note = &line->notes[i];
            if (!note->present) {
                continue;
            }
            if (notes == 0 || semitone_of(note) > top) {
                top = semitone_of(note);
                first_top = i;
            }
            notes++;
            last_note = i;
            if (note->confidence < least) {
                least = note->confidence;
            }
        }
        if (notes == 0) {
            continue;
        }
        StandardCounterpointClimaxClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.span_first = (int)span->first;
        claim.span_last = (int)span->last;
        claim.pitch = top;
        claim.first_index = (int)first_top;
        const char *ids[STANDARD_COUNTERPOINT_CLIMAX_LISTED];
        size_t listed = 0;
        size_t rank = 0;
        size_t first_rank = 0;
        for (size_t i = span->first; i <= span->last; i++) {
            const StandardCounterpointMelodyNote *note = &line->notes[i];
            if (!note->present) {
                continue;
            }
            if (i == first_top) {
                first_rank = rank;
            }
            rank++;
            if (semitone_of(note) == top) {
                if (listed < STANDARD_COUNTERPOINT_CLIMAX_LISTED) {
                    claim.occurrences[listed] = (int)i;
                    ids[listed++] = note->note_id;
                }
                claim.occurrence_count++;
            }
        }
        claim.unique = claim.occurrence_count == 1;
        claim.relative_position = notes > 1 ? (double)first_rank / (double)(notes - 1) : 0.0;
        const StandardCounterpointMelodyNote *first = &line->notes[first_top];
        claim.has_strength = first->has_strength;
        claim.strength = first->has_strength ? (int)first->strength : 0;
        for (size_t k = 0; k < intervals->count; k++) {
            const StandardCounterpointMelodicIntervalClaim *step = &intervals->items[k].claim;
            if ((size_t)step->to == first_top && (size_t)step->from >= span->first && step->leap) {
                claim.by_leap = 1;
            }
        }
        claim.final_note = first_top == last_note;

        StandardCounterpointClimax *climax = &found.items[found.count];
        snprintf(climax->span_id, sizeof(climax->span_id), "%s", span->id);
        climax->claim = claim;
        climax->uncertainty = 1.0 - least;
        if (!standard_counterpoint_melody_emit("melodic-climax", span->id, ids, listed,
                                               STANDARD_COUNTERPOINT_CLIMAX_CLAIM_TYPE, &claim, sizeof(claim),
                                               climax->uncertainty, &climax->shell, &climax->interpretation)) {
            standard_counterpoint_climaxes_destroy(&found);
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

bool standard_counterpoint_climax_claim_read(const Interpretation *interpretation,
                                             StandardCounterpointClimaxClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_COUNTERPOINT_CLIMAX_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
