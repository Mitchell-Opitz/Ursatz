#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "interpretation/evidence.h"

/*
 * Context + theory-tagged test: Evidence's strength, relevance, and
 * reliability MUST NOT collapse to a single scalar "evidence score" --
 * each is its own fraction, checked on its own: a dimension outside [0, 1]
 * or over a zero denominator is rejected whichever of the three it is,
 * while three different valid values are accepted together.
 */

static EvidenceResult create(unsigned long strength_n, unsigned long strength_d, unsigned long relevance_n,
                             unsigned long relevance_d, unsigned long reliability_n, unsigned long reliability_d,
                             const char *method, const char *source, Evidence **out) {
    EntityID *id;
    assert(entity_id_create("evidence-1", &id) == ENTITY_ID_OK);
    EvidenceResult result = evidence_create(id, strength_n, strength_d, relevance_n, relevance_d, reliability_n,
                                            reliability_d, true, method, source, out);
    if (result != EVIDENCE_OK) {
        entity_id_destroy(id);
    }
    return result;
}

int main(void) {
    Evidence *evidence;
    assert(create(9, 10, 1, 2, 3, 4, "corpus-frequency-analysis", "Bach chorale corpus", &evidence) == EVIDENCE_OK);
    evidence_destroy(evidence);

    /* Each dimension is checked on its own. */
    Evidence *rejected;
    assert(create(5, 4, 1, 2, 1, 2, "method", "source", &rejected) == EVIDENCE_ERR_SCORE_OUT_OF_RANGE);
    assert(create(1, 2, 5, 4, 1, 2, "method", "source", &rejected) == EVIDENCE_ERR_SCORE_OUT_OF_RANGE);
    assert(create(1, 2, 1, 2, 5, 4, "method", "source", &rejected) == EVIDENCE_ERR_SCORE_OUT_OF_RANGE);
    assert(create(1, 0, 1, 2, 1, 2, "method", "source", &rejected) == EVIDENCE_ERR_NON_POSITIVE_DENOMINATOR);
    assert(create(1, 2, 1, 0, 1, 2, "method", "source", &rejected) == EVIDENCE_ERR_NON_POSITIVE_DENOMINATOR);
    assert(create(1, 2, 1, 2, 1, 0, "method", "source", &rejected) == EVIDENCE_ERR_NON_POSITIVE_DENOMINATOR);

    /* Method and source are required and non-empty. */
    assert(create(1, 2, 1, 2, 1, 2, NULL, "source", &rejected) == EVIDENCE_ERR_NULL_METHOD);
    assert(create(1, 2, 1, 2, 1, 2, "", "source", &rejected) == EVIDENCE_ERR_NULL_METHOD);
    assert(create(1, 2, 1, 2, 1, 2, "method", NULL, &rejected) == EVIDENCE_ERR_NULL_SOURCE);
    assert(create(1, 2, 1, 2, 1, 2, "method", "", &rejected) == EVIDENCE_ERR_NULL_SOURCE);

    /* A missing id is rejected. */
    assert(evidence_create(NULL, 1, 2, 1, 2, 1, 2, false, "method", "source", &rejected) == EVIDENCE_ERR_NULL_ID);

    evidence_destroy(NULL);

    printf("test_evidence: ok\n");
    return 0;
}
