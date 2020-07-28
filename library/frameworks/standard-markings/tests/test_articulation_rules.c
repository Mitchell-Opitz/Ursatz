#include <assert.h>
#include <stdio.h>

#include "articulation_rules.h"
#include "framework.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_MARKINGS_ARTICULATION_RULE_COUNT; i++) {
        const Rule *rule = standard_markings_articulation_rule((StandardMarkingsArticulationRule)i);
        const Constraint *constraint =
            standard_markings_articulation_constraint((StandardMarkingsArticulationRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_markings_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_markings_articulation_rule(STANDARD_MARKINGS_ARTICULATION_RULE_COUNT) == NULL);
}

static void test_classes(void) {
    assert(standard_markings_articulation_class("legato") == ARTICULATION_CLASS_CONNECTED);
    assert(standard_markings_articulation_class("slur") == ARTICULATION_CLASS_CONNECTED);
    assert(standard_markings_articulation_class("tenuto") == ARTICULATION_CLASS_CONNECTED);
    assert(standard_markings_articulation_class("staccato") == ARTICULATION_CLASS_SEPARATED);
    assert(standard_markings_articulation_class("accent") == ARTICULATION_CLASS_ACCENTED);
    assert(standard_markings_articulation_class("marcato") == ARTICULATION_CLASS_ACCENTED);
    assert(standard_markings_articulation_class("sfz") == ARTICULATION_CLASS_ACCENTED);

    assert(standard_markings_articulation_class("STACCATO") == ARTICULATION_CLASS_SEPARATED);
    assert(standard_markings_articulation_class("pizzicato") == ARTICULATION_CLASS_UNKNOWN);
    assert(standard_markings_articulation_class("") == ARTICULATION_CLASS_UNKNOWN);
    assert(standard_markings_articulation_class(NULL) == ARTICULATION_CLASS_UNKNOWN);
}

static void test_sforzando_forms(void) {
    const char *forms[] = {"sfz", "sf", "fz", "sforzando", "forzando", "forzato", "SFZ"};
    for (size_t i = 0; i < sizeof(forms) / sizeof(forms[0]); i++) {
        assert(standard_markings_is_sforzando(forms[i]));
        assert(standard_markings_articulation_is_dynamic_like(forms[i]));
        assert(standard_markings_articulation_class(forms[i]) == ARTICULATION_CLASS_ACCENTED);
    }
    assert(!standard_markings_is_sforzando("accent"));
    assert(!standard_markings_is_sforzando("f"));
    assert(!standard_markings_is_sforzando(NULL));
    assert(!standard_markings_articulation_is_dynamic_like("marcato"));
}

/* Only the stated relations are ordered. */
static void test_accent_force(void) {
    assert(standard_markings_articulation_more_forceful("marcato", "accent"));
    assert(standard_markings_articulation_more_forceful("sfz", "accent"));
    assert(standard_markings_articulation_more_forceful("sforzando", "accent"));

    assert(!standard_markings_articulation_more_forceful("accent", "marcato"));
    assert(!standard_markings_articulation_more_forceful("accent", "accent"));
    assert(!standard_markings_articulation_more_forceful("marcato", "marcato"));
    /* Marcato versus sforzando is not stated, so neither outranks the other. */
    assert(!standard_markings_articulation_more_forceful("marcato", "sfz"));
    assert(!standard_markings_articulation_more_forceful("sfz", "marcato"));
    assert(!standard_markings_articulation_more_forceful("staccato", "accent"));
    assert(!standard_markings_articulation_more_forceful(NULL, "accent"));
    assert(!standard_markings_articulation_more_forceful("marcato", NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_classes();
    test_sforzando_forms();
    test_accent_force();
    printf("test_articulation_rules (standard-markings): ok\n");
    return 0;
}
