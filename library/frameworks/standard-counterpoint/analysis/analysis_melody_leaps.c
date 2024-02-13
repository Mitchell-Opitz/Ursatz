#include "analysis_melody_leaps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_melody_emit.h"
#include "core/value.h"

/* The size from which the existing rule asks a leap to be recovered (a fourth). */
#define RECOVERY_FROM_SIZE 4

const char *standard_counterpoint_leap_status_name(StandardCounterpointLeapStatus status) {
    switch (status) {
        case STANDARD_COUNTERPOINT_LEAP_RECOVERED:
            return "recovered";
        case STANDARD_COUNTERPOINT_LEAP_UNRECOVERED:
            return "unrecovered";
        case STANDARD_COUNTERPOINT_LEAP_NOT_REQUIRED:
            return "not-required";
        case STANDARD_COUNTERPOINT_LEAP_NOT_JUDGED:
            return "not-judged";
        default:
            return NULL;
    }
}

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

static double greater(double a, double b) {
    return a > b ? a : b;
}

/* --- leap recovery --- */

static void destroy_recovery(StandardCounterpointLeapRecovery *item) {
    interpretation_destroy(item->interpretation);
    structure_destroy(item->shell);
    memset(item, 0, sizeof(*item));
}

void standard_counterpoint_leap_recoveries_destroy(StandardCounterpointLeapRecoveries *recoveries) {
    if (recoveries == NULL) {
        return;
    }
    for (size_t i = 0; i < recoveries->count; i++) {
        destroy_recovery(&recoveries->items[i]);
    }
    free(recoveries->items);
    recoveries->items = NULL;
    recoveries->count = 0;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_leap_recoveries(
    const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicIntervals *intervals,
    StandardCounterpointLeapRecoveries *out) {
    if (line == NULL || intervals == NULL || out == NULL || (line->count > 0 && line->notes == NULL)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointLeapRecoveries found = {NULL, 0};
    if (intervals->count > 0) {
        found.items = (StandardCounterpointLeapRecovery *)calloc(intervals->count,
                                                                   sizeof(StandardCounterpointLeapRecovery));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t k = 0; k < intervals->count; k++) {
        const StandardCounterpointMelodicInterval *leap = &intervals->items[k];
        if (!leap->claim.leap) {
            continue;
        }
        const StandardCounterpointMelodicInterval *next =
            (k + 1 < intervals->count && intervals->items[k + 1].claim.from == leap->claim.to)
                ? &intervals->items[k + 1]
                : NULL;
        StandardCounterpointLeapRecovery *item = &found.items[found.count];
        StandardCounterpointLeapRecoveryClaim *claim = &item->claim;
        claim->from = leap->claim.from;
        claim->to = leap->claim.to;
        claim->semitones = leap->claim.semitones;
        claim->direction = leap->claim.direction;
        claim->generic_size = leap->claim.generic_size;
        claim->approximate = leap->claim.approximate;
        claim->next_index = next != NULL ? next->claim.to : -1;
        item->uncertainty = leap->uncertainty;
        if (leap->effective.size < RECOVERY_FROM_SIZE) {
            claim->status = (int)STANDARD_COUNTERPOINT_LEAP_NOT_REQUIRED;
        } else if (next == NULL) {
            claim->status = (int)STANDARD_COUNTERPOINT_LEAP_NOT_JUDGED;
        } else {
            item->uncertainty = greater(leap->uncertainty, next->uncertainty);
            claim->approximate = claim->approximate || next->claim.approximate;
            claim->status = (next->claim.step && next->claim.direction != leap->claim.direction)
                                ? (int)STANDARD_COUNTERPOINT_LEAP_RECOVERED
                                : (int)STANDARD_COUNTERPOINT_LEAP_UNRECOVERED;
        }
        const char *ids[3] = {line->notes[claim->from].note_id, line->notes[claim->to].note_id,
                              next != NULL ? line->notes[claim->next_index].note_id : NULL};
        char key[400];
        snprintf(key, sizeof(key), "%s-%s", ids[0], ids[1]);
        if (!standard_counterpoint_melody_emit("leap-recovery", key, ids, next != NULL ? 3 : 2,
                                               STANDARD_COUNTERPOINT_LEAP_RECOVERY_CLAIM_TYPE, claim, sizeof(*claim),
                                               item->uncertainty, &item->shell, &item->interpretation)) {
            standard_counterpoint_leap_recoveries_destroy(&found);
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

bool standard_counterpoint_leap_recovery_claim_read(const Interpretation *interpretation,
                                                    StandardCounterpointLeapRecoveryClaim *out) {
    return read_claim(interpretation, STANDARD_COUNTERPOINT_LEAP_RECOVERY_CLAIM_TYPE, out, sizeof(*out));
}

/* --- consecutive leaps --- */

static void destroy_consecutive(StandardCounterpointConsecutiveLeaps *item) {
    interpretation_destroy(item->interpretation);
    structure_destroy(item->shell);
    memset(item, 0, sizeof(*item));
}

void standard_counterpoint_consecutive_leap_list_destroy(StandardCounterpointConsecutiveLeapList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        destroy_consecutive(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* Two same-direction leaps, each a third, that together span a perfect fifth (a consonant triad). */
static bool outlines_triad(const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicInterval *first,
                           const StandardCounterpointMelodicInterval *second) {
    if (first->effective.size != 3 || second->effective.size != 3) {
        return false;
    }
    const StandardCounterpointMelodyNote *a = &line->notes[first->claim.from];
    const StandardCounterpointMelodyNote *c = &line->notes[second->claim.to];
    StandardCounterpointInterval outer;
    if (a->spelling != NULL && c->spelling != NULL && standard_counterpoint_interval_between(a->spelling, c->spelling, &outer)) {
        return outer.size == 5 && outer.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT;
    }
    return labs(first->claim.semitones + second->claim.semitones) == 7;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_consecutive_leaps(
    const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicIntervals *intervals,
    StandardCounterpointConsecutiveLeapList *out) {
    if (line == NULL || intervals == NULL || out == NULL || (line->count > 0 && line->notes == NULL)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointConsecutiveLeapList found = {NULL, 0};
    if (intervals->count > 1) {
        found.items = (StandardCounterpointConsecutiveLeaps *)calloc(intervals->count,
                                                                       sizeof(StandardCounterpointConsecutiveLeaps));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t k = 0; k + 1 < intervals->count; k++) {
        const StandardCounterpointMelodicInterval *first = &intervals->items[k];
        const StandardCounterpointMelodicInterval *second = &intervals->items[k + 1];
        if (!first->claim.leap || !second->claim.leap || second->claim.from != first->claim.to) {
            continue;
        }
        bool three = k + 2 < intervals->count && intervals->items[k + 2].claim.leap &&
                     intervals->items[k + 2].claim.from == second->claim.to;
        bool same = first->claim.direction == second->claim.direction && !outlines_triad(line, first, second);
        if (!three && !same) {
            continue;
        }
        StandardCounterpointConsecutiveLeaps *item = &found.items[found.count];
        item->claim.first_from = first->claim.from;
        item->claim.middle = first->claim.to;
        item->claim.last_to = second->claim.to;
        item->claim.same_direction = same;
        item->claim.three_in_a_row = three;
        item->uncertainty = greater(first->uncertainty, second->uncertainty);
        if (three) {
            item->uncertainty = greater(item->uncertainty, intervals->items[k + 2].uncertainty);
        }
        const char *ids[3] = {line->notes[item->claim.first_from].note_id, line->notes[item->claim.middle].note_id,
                              line->notes[item->claim.last_to].note_id};
        char key[400];
        snprintf(key, sizeof(key), "%s-%s-%s", ids[0], ids[1], ids[2]);
        if (!standard_counterpoint_melody_emit("consecutive-leaps", key, ids, 3,
                                               STANDARD_COUNTERPOINT_CONSECUTIVE_LEAPS_CLAIM_TYPE, &item->claim,
                                               sizeof(item->claim), item->uncertainty, &item->shell,
                                               &item->interpretation)) {
            standard_counterpoint_consecutive_leap_list_destroy(&found);
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

bool standard_counterpoint_consecutive_leaps_claim_read(const Interpretation *interpretation,
                                                        StandardCounterpointConsecutiveLeapsClaim *out) {
    return read_claim(interpretation, STANDARD_COUNTERPOINT_CONSECUTIVE_LEAPS_CLAIM_TYPE, out, sizeof(*out));
}
