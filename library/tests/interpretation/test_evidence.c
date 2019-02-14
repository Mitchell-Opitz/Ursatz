#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "interpretation/evidence.h"

/*
 * Context + theory-tagged test: Evidence's strength, relevance, and
 * reliability MUST NOT collapse to a single scalar "evidence score" --
 * this test constructs an Evidence where the three dimensions hold
 * different values and confirms each comes back independently,
 * distinct from the others, rather than one field driving all three.
 */

int main(void) {
    EntityID *id;
    assert(entity_id_create("evidence-1", &id) == ENTITY_ID_OK);

    Evidence *evidence;
    EvidenceResult result = evidence_create(id, 9, 10, 1, 2, 3, 4, true, "corpus-frequency-analysis",
                                             "Bach chorale corpus", &evidence);
    assert(result == EVIDENCE_OK);

    /* Each dimension is independently queryable and distinct from the others. */
    assert(evidence_strength_numerator(evidence) == 9);
    assert(evidence_strength_denominator(evidence) == 10);
    assert(evidence_relevance_numerator(evidence) == 1);
    assert(evidence_relevance_denominator(evidence) == 2);
    assert(evidence_reliability_numerator(evidence) == 3);
    assert(evidence_reliability_denominator(evidence) == 4);

    /* No pair of dimensions was forced equal by construction. */
    assert(evidence_strength_numerator(evidence) * evidence_relevance_denominator(evidence) !=
           evidence_relevance_numerator(evidence) * evidence_strength_denominator(evidence));
    assert(evidence_relevance_numerator(evidence) * evidence_reliability_denominator(evidence) !=
           evidence_reliability_numerator(evidence) * evidence_relevance_denominator(evidence));

    assert(evidence_independence(evidence) == true);
    assert(strcmp(evidence_method(evidence), "corpus-frequency-analysis") == 0);
    assert(strcmp(evidence_source(evidence), "Bach chorale corpus") == 0);

    evidence_destroy(evidence);

    /* A dimension outside [0, 1] is rejected. */
    EntityID *id_2;
    assert(entity_id_create("evidence-2", &id_2) == ENTITY_ID_OK);
    Evidence *rejected;
    assert(evidence_create(id_2, 5, 4, 1, 2, 1, 2, false, "method", "source", &rejected) ==
           EVIDENCE_ERR_SCORE_OUT_OF_RANGE);
    entity_id_destroy(id_2);

    printf("test_evidence: ok\n");
    return 0;
}