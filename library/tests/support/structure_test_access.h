#ifndef URSATZ_TESTS_SUPPORT_STRUCTURE_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_STRUCTURE_TEST_ACCESS_H

#include "model/structure/cadence.h"
#include "model/structure/cadence_internal.h"
#include "model/structure/motif.h"
#include "model/structure/motif_internal.h"
#include "model/structure/motif_occurrence.h"
#include "model/structure/motif_occurrence_internal.h"
#include "model/structure/phrase_occurrence.h"
#include "model/structure/phrase_occurrence_internal.h"

/*
 * What the tests read of a Cadence, a Motif, a MotifOccurrence and a PhraseOccurrence. No application reads these, so
 * they are not in the structure/ headers.
 */

static inline const EntityID *cadence_id(const Cadence *cadence) {
    return cadence->id;
}

static inline size_t cadence_event_id_count(const Cadence *cadence) {
    return cadence->event_id_count;
}

static inline const EntityID *cadence_event_id_at(const Cadence *cadence, size_t index) {
    return cadence->event_ids[index];
}

static inline size_t motif_event_id_count(const Motif *motif) {
    return motif->event_id_count;
}

/* The id of the Motif this is an occurrence of. */
static inline const EntityID *motif_occurrence_motif_id(const MotifOccurrence *motif_occurrence) {
    return occurrence_target_id(motif_occurrence->occurrence);
}

static inline size_t motif_occurrence_region_event_id_count(const MotifOccurrence *motif_occurrence) {
    return motif_occurrence->region_event_id_count;
}

static inline size_t phrase_occurrence_region_event_id_count(const PhraseOccurrence *phrase_occurrence) {
    return phrase_occurrence->region_event_id_count;
}

#endif
