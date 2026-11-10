#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_cadence_kinds.h"
#include "harmony_cadence_rules.h"

static void test_every_core_type_has_one_entry(void) {
    for (int t = 0; t < CADENCE_TYPE_COUNT; t++) {
        CadenceType type = (CadenceType)t;
        const HarmonyCadenceKind *kind = harmony_cadence_kind_of(type);
        if (type == CADENCE_TYPE_UNKNOWN) {
            assert(kind == NULL);
            continue;
        }
        assert(kind != NULL);
        assert(strcmp(kind->name, cadence_type_name(type)) == 0);
        assert(kind->core == type);
        size_t named = 0;
        for (size_t i = 0; i < harmony_cadence_kind_count(); i++) {
            if (strcmp(harmony_cadence_kind_at(i)->name, cadence_type_name(type)) == 0) {
                named++;
            }
        }
        assert(named == 1);
    }
    assert(harmony_cadence_kind_of(CADENCE_TYPE_COUNT) == NULL);
    assert(harmony_cadence_kind_of((CadenceType)-1) == NULL);
}

static void test_caplin_and_sonata_kinds(void) {
    const HarmonyCadenceKind *evaded = harmony_cadence_kind_find("evaded");
    const HarmonyCadenceKind *abandoned = harmony_cadence_kind_find("abandoned");
    const HarmonyCadenceKind *medial = harmony_cadence_kind_find("medial-caesura");
    assert(evaded != NULL && evaded->core == CADENCE_TYPE_SUBVERTED);
    assert(abandoned != NULL && abandoned->core == CADENCE_TYPE_UNKNOWN);
    assert(medial != NULL && medial->core == CADENCE_TYPE_HALF);
    assert(abandoned->strength == -1);
}

static void test_strength_and_names(void) {
    for (size_t i = 0; i < harmony_cadence_kind_count(); i++) {
        const HarmonyCadenceKind *kind = harmony_cadence_kind_at(i);
        assert(kind->name != NULL && kind->label != NULL && kind->abbreviation != NULL);
        assert(harmony_cadence_kind_find(kind->name) == kind);
        if (strcmp(kind->name, "abandoned") != 0) {
            assert(kind->strength == harmony_cadence_strength(kind->core));
        }
    }
}

static void test_lookup_failures(void) {
    assert(harmony_cadence_kind_find("no-such-cadence") == NULL);
    assert(harmony_cadence_kind_find(NULL) == NULL);
    assert(harmony_cadence_kind_at(harmony_cadence_kind_count()) == NULL);
}

int main(void) {
    test_every_core_type_has_one_entry();
    test_caplin_and_sonata_kinds();
    test_strength_and_names();
    test_lookup_failures();
    printf("test_harmony_cadence_kinds: ok\n");
    return 0;
}
