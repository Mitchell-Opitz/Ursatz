#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plan/plan.h>
#include <plan/plan_json.h>
#include <realization/movement.h>
#include <realization/performer_part.h>
#include <realization/work.h>
#include <reading/reading.h>
#include <theory/vocabulary/cadence_type.h>

#include "check/hearing_result.h"
#include "render/movement_build.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"

/*
 * The reading a hearing makes of a span (R-633): the claims over the notes of the work of rendered_plan_work. A written period in A minor
 * (a half cadence, then a PAC) is heard, and the reading's claims give what the form hearing found in the chain's results; every note
 * an evidence list names is a performer note of the plan's work; hearing_event_of_note inverts rendered_plan_note_id; and two requests of
 * one span make one reading (one chain run).
 */

static Plan *load(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", COMPOSER_FIXTURE_DIR, name);
    Plan *plan = NULL;
    PlanJsonError error;
    assert(test_read_plan_file(path, &plan, &error) == PLAN_OK);
    return plan;
}

static PlanSpan bars(long start, long length) {
    PlanSpan span;
    assert(rational_make(start, 1, &span.start) == RATIONAL_OK);
    assert(rational_make(length, 1, &span.length) == RATIONAL_OK);
    return span;
}

static bool is_note_of(const Work *work, const char *id) {
    for (size_t m = 0; m < work_movement_count(work); m++) {
        const Movement *movement = work_movement_at(work, m);
        for (size_t p = 0; p < movement_part_count(movement); p++) {
            const PerformerPart *part = movement_part_at(movement, p);
            for (size_t n = 0; n < performer_part_note_count(part); n++) {
                if (strcmp(performer_part_note_at(part, n)->id, id) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void check_evidence(const Work *work, const ReadingEvidence *evidence, const char *kind) {
    for (size_t i = 0; i < evidence->count; i++) {
        if (!is_note_of(work, evidence->ids[i])) {
            fprintf(stderr, "%s evidence %s is not a performer note of the work\n", kind, evidence->ids[i]);
            abort();
        }
    }
}

int main(void) {
    Plan *plan = load("period_a_minor.json");
    ComposerHearing *hearing = NULL;
    assert(hearing_create(plan, test_knowledge(), &hearing) == HEARING_OK);

    size_t runs_before = atomic_load(&hearing_chain_runs_made);
    const Reading *reading = NULL;
    assert(hearing_section_reading(hearing, bars(0, 8), &reading) == HEARING_OK && reading != NULL);
    const Reading *again = NULL;
    assert(hearing_section_reading(hearing, bars(0, 8), &again) == HEARING_OK);
    assert(again == reading && atomic_load(&hearing_chain_runs_made) == runs_before + 1); /* one reading, one chain run */

    /* The source is the Composer and the target the work and movement of rendered_plan_work, with no digest. */
    const ReadingSource *source = reading_source(reading);
    const ReadingTarget *target = reading_target(reading);
    assert(source->kind == READING_SOURCE_COMPOSER && strcmp(source->name, "ursatz-composer") == 0);
    assert(strcmp(target->work, "w1") == 0 && strcmp(target->movement, "m1") == 0 && target->digest[0] == '\0');

    /* The cadence and theme claims give what the form hearing found: a period, a half cadence at bar 4 and the PAC of bar 8. */
    assert(reading_cadence_count(reading) == 2);
    assert(reading_cadence_at(reading, 0)->type == CADENCE_TYPE_HALF && reading_cadence_at(reading, 0)->arrival.num == 3 &&
           reading_cadence_at(reading, 0)->arrival.den == 1);
    assert(reading_cadence_at(reading, 1)->type == CADENCE_TYPE_PERFECT_AUTHENTIC && reading_cadence_at(reading, 1)->arrival.num == 7 &&
           reading_cadence_at(reading, 1)->arrival.den == 1);
    size_t themes = 0;
    size_t phrases = 0;
    for (size_t g = 0; g < reading_group_count(reading); g++) {
        const ReadingGroup *group = reading_group_at(reading, g);
        phrases += group->kind == READING_GROUP_PHRASE;
        if (group->kind == READING_GROUP_THEME) {
            themes++;
            assert(strcmp(group->form, "period") == 0 && group->phrase_count == 2);
            assert(group->part_cadence_count == 2 && group->part_cadences[0] == CADENCE_TYPE_HALF &&
                   group->part_cadences[1] == CADENCE_TYPE_PERFECT_AUTHENTIC);
        }
    }
    assert(themes == 1 && phrases == 2);
    assert(reading_region_count(reading) >= 1);

    /* Every evidence id is a performer note of the work of the same plan. */
    const RenderedPlan *rendered = hearing_rendered(hearing);
    Work *work = NULL;
    PlanExportError detail;
    assert(rendered_plan_work(rendered, NULL, test_knowledge(), &work, &detail) == PLAN_EXPORT_OK);
    for (size_t i = 0; i < reading_region_count(reading); i++) {
        check_evidence(work, &reading_region_at(reading, i)->evidence, "region");
    }
    for (size_t i = 0; i < reading_chord_count(reading); i++) {
        check_evidence(work, &reading_chord_at(reading, i)->evidence, "chord");
    }
    for (size_t i = 0; i < reading_cadence_count(reading); i++) {
        check_evidence(work, &reading_cadence_at(reading, i)->evidence, "cadence");
    }
    for (size_t i = 0; i < reading_group_count(reading); i++) {
        check_evidence(work, &reading_group_at(reading, i)->evidence, "group");
    }
    for (size_t i = 0; i < reading_voice_fault_count(reading); i++) {
        check_evidence(work, &reading_voice_fault_at(reading, i)->evidence, "fault");
    }
    assert(reading_chord_count(reading) > 0 && reading_chord_at(reading, 0)->evidence.count > 0);

    /* hearing_event_of_note inverts rendered_plan_note_id. */
    char id[REALIZATION_ID_MAX];
    for (size_t event = 0; event < rendered->count; event++) {
        assert(rendered_plan_note_id(rendered, event, id));
        assert(is_note_of(work, id));
        assert(hearing_event_of_note(hearing, id) == event);
    }
    assert(!rendered_plan_note_id(rendered, rendered->count, id));
    assert(hearing_event_of_note(hearing, "piano-n999999") == (size_t)-1);
    assert(hearing_event_of_note(hearing, NULL) == (size_t)-1);

    /* A span with no notes stays not written, with no reading. */
    const Reading *none = (const Reading *)0x1;
    assert(hearing_section_reading(hearing, bars(40, 4), &none) != HEARING_OK && none == NULL);

    work_destroy(work);
    hearing_destroy(hearing);
    plan_destroy(plan);
    printf("test_hearing_reading: ok\n");
    return 0;
}
