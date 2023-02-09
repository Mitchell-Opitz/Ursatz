#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "analysis/period_detection.h"
#include "core/occurrence.h"
#include "harmony_cadence_rules.h"
#include "identity/entity_id.h"
#include "structure/phrase_occurrence.h"

/*
 * Kernel period detection, given standard-harmony's ranking: the order it pairs cadences by
 * is harmony_cadence_strength's. The kernel analyzer never includes this framework; the
 * caller passes the function in.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static PhraseOccurrence *make_phrase(const char *occurrence_id, const char *phrase_id, const char *event_id) {
    Occurrence *occurrence;
    assert(occurrence_create(make_id(occurrence_id), make_id(phrase_id), &occurrence) == OCCURRENCE_OK);
    EntityID **events = (EntityID **)malloc(sizeof(EntityID *));
    events[0] = make_id(event_id);
    PhraseOccurrence *phrase;
    assert(phrase_occurrence_create(occurrence, events, 1, &phrase) == PHRASE_OCCURRENCE_OK);
    return phrase;
}

static size_t period_count(PhraseOccurrence *first, CadenceType first_type, PhraseOccurrence *second,
                           CadenceType second_type) {
    PeriodDetectionEntry entries[2] = {{first, first_type}, {second, second_type}};
    PeriodDetectionInput input = {entries, 2, harmony_cadence_strength};
    AnalysisResult *result;
    assert(period_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    size_t count = analysis_result_interpretation_count(result);
    analysis_result_destroy(result);
    return count;
}

int main(void) {
    PhraseOccurrence *a = make_phrase("occ-a", "phrase-a", "note-a");
    PhraseOccurrence *b = make_phrase("occ-b", "phrase-b", "note-b");

    /* Weaker then stronger is a period, at every step of the order. */
    assert(period_count(a, CADENCE_TYPE_HALF, b, CADENCE_TYPE_DECEPTIVE) == 1);
    assert(period_count(a, CADENCE_TYPE_DECEPTIVE, b, CADENCE_TYPE_PLAGAL) == 1);
    assert(period_count(a, CADENCE_TYPE_PLAGAL, b, CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) == 1);
    assert(period_count(a, CADENCE_TYPE_AUTHENTIC_UNSPECIFIED, b, CADENCE_TYPE_IMPERFECT_AUTHENTIC) == 1);
    assert(period_count(a, CADENCE_TYPE_IMPERFECT_AUTHENTIC, b, CADENCE_TYPE_PERFECT_AUTHENTIC) == 1);
    assert(period_count(a, CADENCE_TYPE_HALF, b, CADENCE_TYPE_PERFECT_AUTHENTIC) == 1);

    /* Stronger then weaker, and equal strength, are not. */
    assert(period_count(a, CADENCE_TYPE_PERFECT_AUTHENTIC, b, CADENCE_TYPE_HALF) == 0);
    assert(period_count(a, CADENCE_TYPE_PLAGAL, b, CADENCE_TYPE_DECEPTIVE) == 0);
    assert(period_count(a, CADENCE_TYPE_HALF, b, CADENCE_TYPE_HALF) == 0);

    /* Unranked or non-closing cadences are skipped, on either side. */
    assert(period_count(a, CADENCE_TYPE_UNKNOWN, b, CADENCE_TYPE_PERFECT_AUTHENTIC) == 0);
    assert(period_count(a, CADENCE_TYPE_HALF, b, CADENCE_TYPE_UNKNOWN) == 0);
    assert(period_count(a, CADENCE_TYPE_SUBVERTED, b, CADENCE_TYPE_PERFECT_AUTHENTIC) == 0);
    assert(period_count(a, CADENCE_TYPE_HALF, b, CADENCE_TYPE_COUNT) == 0);

    phrase_occurrence_destroy(a);
    phrase_occurrence_destroy(b);
    printf("test_harmony_period_ranking: ok\n");
    return 0;
}
