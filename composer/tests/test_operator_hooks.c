#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <plan/plan.h>
#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/counterpoint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/grow_types.h>
#include <ursatz_composer/harmony_motive.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/texture_check.h>
#include <ursatz_composer/voicing.h>
#include "support/test_knowledge.h"

/*
 * The registrations and the checkers the operators hand the fill loop, which the compose runs reach only through the stages:
 *
 *   register   grow_form_register, grow_register, harmony_motive_register and form_return_register each add one operator of their
 *              name to a registry; a second call is a duplicate and a NULL registry is refused
 *   checkers   grow_motives_checker, grow_checker and form_return_checker leave a candidate of another operator to the default
 *              checker and refuse a candidate of their own with no payload
 *   payloads   the motive a candidate proposes is NULL for NULL, no payload and a payload of another kind
 *   texture    texture_checker answers what the checker of the operator it is asked about answers
 */

static void test_registrations(void) {
    static const char *const NAMES[4] = {"motive-form", "grow-theme", "harmony-motive", "form-return"};
    for (int which = 0; which < 4; which++) {
        FillOperatorRegistry *registry = fill_registry_create();
        assert(registry);
        FillResult first, second, none;
        switch (which) {
        case 0:
            first = grow_form_register(registry, NULL);
            second = grow_form_register(registry, NULL);
            none = grow_form_register(NULL, NULL);
            break;
        case 1:
            first = grow_register(registry, NULL);
            second = grow_register(registry, NULL);
            none = grow_register(NULL, NULL);
            break;
        case 2:
            first = harmony_motive_register(registry);
            second = harmony_motive_register(registry);
            none = harmony_motive_register(NULL);
            break;
        default:
            first = form_return_register(registry, NULL);
            second = form_return_register(registry, NULL);
            none = form_return_register(NULL, NULL);
            break;
        }
        assert(first == FILL_OK);
        assert(fill_registry_count(registry) == 1);
        assert(strcmp(fill_registry_at(registry, 0)->generator.name, NAMES[which]) == 0);
        assert(second == FILL_ERR_DUPLICATE_OPERATOR && fill_registry_count(registry) == 1);
        assert(none == FILL_ERR_NULL);
        fill_registry_destroy(registry);
    }
}

static FillOperator named(const char *name) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = name;
    return operator_;
}

static bool default_verdict(const FillCheck *check) {
    return !check->hard_violation && check->score_numerator == 0 && check->score_denominator == 1 && check->reason[0] == '\0';
}

static void test_checkers(void) {
    Plan *plan = plan_create();
    FillTarget target = {FILL_TARGET_NODE, 1};
    FillCandidate empty;
    FillCheck check;
    FillOperator other = named("other");
    FillOperator grow = named("grow-theme");
    FillOperator returned = named("form-return");
    memset(&empty, 0, sizeof(empty));
    assert(plan);
    /* A candidate of another operator with no payload: the default verdict, a score of 0 out of 1. */
    memset(&check, 0xff, sizeof(check));
    assert(grow_motives_checker(plan, target, &other, &empty, NULL, &check) && default_verdict(&check));
    memset(&check, 0xff, sizeof(check));
    assert(form_return_checker(plan, target, &other, &empty, NULL, &check) && default_verdict(&check));
    /* NULL arguments are a failure, not a rejection. */
    assert(!grow_checker(NULL, target, &grow, &empty, NULL, &check));
    assert(!grow_checker(plan, target, NULL, &empty, NULL, &check));
    assert(!grow_checker(plan, target, &grow, NULL, NULL, &check));
    assert(!grow_checker(plan, target, &grow, &empty, NULL, NULL));
    assert(!form_return_checker(NULL, target, &returned, &empty, NULL, &check));
    assert(!form_return_checker(plan, target, NULL, &empty, NULL, &check));
    assert(!form_return_checker(plan, target, &returned, NULL, NULL, &check));
    assert(!form_return_checker(plan, target, &returned, &empty, NULL, NULL));
    /* A candidate of the operator's own with no payload is a failure. */
    assert(!grow_checker(plan, target, &grow, &empty, NULL, &check));
    assert(!form_return_checker(plan, target, &returned, &empty, NULL, &check));
    plan_destroy(plan);
}

static void test_candidate_motives(void) {
    /* A payload of another kind starts with another magic number. */
    unsigned long other[8] = {0x1234UL};
    FillCandidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    assert(grow_form_candidate_motive(NULL) == NULL && motive_develop_candidate_motive(NULL) == NULL);
    assert(grow_form_candidate_motive(&candidate) == NULL && motive_develop_candidate_motive(&candidate) == NULL);
    candidate.payload = other;
    assert(grow_form_candidate_motive(&candidate) == NULL && motive_develop_candidate_motive(&candidate) == NULL);
}

static void test_texture(void) {
    Plan *plan = plan_create();
    FillTarget target = {FILL_TARGET_LANE_SPAN, 1};
    FillCandidate empty;
    memset(&empty, 0, sizeof(empty));
    assert(plan);
    for (int which = 0; which < 3; which++) {
        FillOperator operator_ = named(which == 0 ? VOICING_OPERATOR_NAME : which == 1 ? COUNTERPOINT_OPERATOR_NAME : "other");
        FillCheck through, direct;
        memset(&through, 0, sizeof(through));
        memset(&direct, 0, sizeof(direct));
        bool answered = texture_checker(plan, target, &operator_, &empty, NULL, &through);
        bool expected = which == 0   ? voicing_checker(plan, target, &operator_, &empty, NULL, &direct)
                        : which == 1 ? counterpoint_checker(plan, target, &operator_, &empty, NULL, &direct)
                                     : accompaniment_checker(plan, target, &operator_, &empty, NULL, &direct);
        assert(answered == expected);
        assert(through.hard_violation == direct.hard_violation && through.score_numerator == direct.score_numerator &&
               through.score_denominator == direct.score_denominator && strcmp(through.reason, direct.reason) == 0);
    }
    plan_destroy(plan);
}

int main(void) {
    test_registrations();
    test_checkers();
    test_candidate_motives();
    test_texture();
    printf("test_operator_hooks: ok\n");
    return 0;
}
