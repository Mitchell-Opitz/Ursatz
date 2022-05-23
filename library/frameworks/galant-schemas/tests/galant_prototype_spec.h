#ifndef URSATZ_GALANT_SCHEMAS_TESTS_GALANT_PROTOTYPE_SPEC_H
#define URSATZ_GALANT_SCHEMAS_TESTS_GALANT_PROTOTYPE_SPEC_H

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "galant_prototypes.h"
#include "galant_test_util.h"

/*
 * The data of a prototype stated again, in a form that reads like the stage
 * tables of the chapters, so a test can compare it with the registry. A spec
 * has the schema, its name and function, the strengths of its stages as a
 * string ("SWSW"), the melody and bass degrees (DN natural, DE either, DL
 * lowered, DR raised), a second melody degree for stages that have one (NONE
 * for none), the figures and Roman numerals, and the two flags.
 */

#define SPEC_MAX 5

typedef struct {
    int degree;
    GalantAlteration alteration;
} SpecDegree;

#define DN(d) {(d), GALANT_ALTERATION_NATURAL}
#define DE(d) {(d), GALANT_ALTERATION_EITHER}
#define DL(d) {(d), GALANT_ALTERATION_LOWERED}
#define DR(d) {(d), GALANT_ALTERATION_RAISED}
#define NONE {0, GALANT_ALTERATION_NATURAL}

typedef struct {
    GalantSchemaId id;
    const char *name;
    SchemaFunction function;
    const char *strengths;
    SpecDegree melody[SPEC_MAX];
    SpecDegree bass[SPEC_MAX];
    SpecDegree melody_next[SPEC_MAX];
    const char *figures[SPEC_MAX];
    const char *numerals[SPEC_MAX];
    bool bass_defining;
    bool tenths;
} PrototypeSpec;

static inline void assert_same_degree(const GalantDegree *actual, const SpecDegree *expected) {
    assert(actual->degree == expected->degree);
    if (expected->degree != 0) {
        assert(actual->alteration == expected->alteration);
    }
}

/* The registry's prototype of the spec's schema matches the spec. */
static inline void verify_spec(const PrototypeSpec *spec) {
    const GalantPrototype *p = galant_prototype(spec->id);
    assert(p != NULL);
    assert(p->id == spec->id);
    assert(strcmp(p->name, spec->name) == 0);
    assert(strcmp(galant_schema_name(spec->id), spec->name) == 0);
    assert(p->function == spec->function);
    assert(p->stage_count == strlen(spec->strengths));
    assert(p->bass_defining == spec->bass_defining);
    assert(p->melody_in_tenths == spec->tenths);
    for (size_t i = 0; i < p->stage_count; i++) {
        const GalantStage *stage = &p->stages[i];
        assert(stage->strength == (spec->strengths[i] == 'S' ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_WEAK));
        assert_same_degree(&stage->melody, &spec->melody[i]);
        assert_same_degree(&stage->bass, &spec->bass[i]);
        assert_same_degree(&stage->melody_next, &spec->melody_next[i]);
        assert(spec->figures[i] != NULL && stage->figures != NULL && strcmp(stage->figures, spec->figures[i]) == 0);
        assert(spec->numerals[i] != NULL && stage->numeral != NULL && strcmp(stage->numeral, spec->numerals[i]) == 0);
    }
}

#endif
