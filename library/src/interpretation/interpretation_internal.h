#ifndef MUSIC_INTERPRETATION_INTERPRETATION_INTERNAL_H
#define MUSIC_INTERPRETATION_INTERPRETATION_INTERNAL_H

#include "interpretation/interpretation.h"

/* The fields of an Interpretation: interpretation.c writes them; tests read them through tests/support/interpretation_test_access.h. */
struct Interpretation {
    EntityID *id;
    EntityID *target_id;
    Value *claim;
    EntityID *context_id;
    EntityID *evidence_id;
    FrameworkReference *framework_reference;
    Uncertainty *uncertainty;
    char *author;
    char *method;
    InterpretationStatus status;
    ProvenanceReference *provenance;
};

#endif
