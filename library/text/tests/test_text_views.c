#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plan/plan_json.h"
#include "reading/reading_json.h"
#include "realization/realization_json.h"
#include "text/text_view.h"
#include "ursatz_test.h"

/*
 * The views of Ursatz Text (R-663), printed from the loop (plan, realization and reading), a binary form's plan and Bach 114
 * (realization and reading), against goldens in snapshots/ (URSATZ_UPDATE_GOLDENS=1 rewrites them); the limits of a measure range
 * and a section; every refusal; the frame.
 */

static KnowledgeSnapshot *snapshot;

typedef struct {
    Plan *plan;
    Work *work;
    Reading *reading;
    TextViewSource source;
} Fixture;

static void load(Fixture *f, const char *address, const char *plan, const char *realization, const char *reading) {
    memset(f, 0, sizeof *f);
    f->source.address = address;
    f->source.knowledge = snapshot;
    if (plan) {
        char *text = test_must_read_file(plan, NULL);
        PlanJsonError error = {{0}, {0}, 0};
        CHECKF(plan_from_json(snapshot, text, NULL, &f->plan, &error) == PLAN_OK, "%s: %s", plan, error.message);
        free(text);
        f->source.plan = f->plan;
    }
    if (realization) {
        char *text = test_must_read_file(realization, NULL);
        RealizationJsonError error = {{0}, {0}};
        CHECKF(realization_work_from_json(snapshot, text, &f->work, &error) == REALIZATION_OK, "%s: %s", realization, error.message);
        free(text);
        f->source.realization = f->work;
    }
    if (reading) {
        char *text = test_must_read_file(reading, NULL);
        ReadingJsonError error = {{0}, {0}};
        CHECKF(reading_from_json(snapshot, text, &f->reading, &error) == REALIZATION_OK, "%s: %s", reading, error.message);
        free(text);
        f->source.reading = f->reading;
    }
}

static void unload(Fixture *f) {
    plan_destroy(f->plan);
    work_destroy(f->work);
    reading_destroy(f->reading);
}

static int updated;

/* The view against snapshots/<name>.<view>.txt (':' becomes '-'). */
static void check_golden(const Fixture *f, const char *name, const char *view, const TextViewLimits *limits, const char *suffix) {
    TextError error = {-1, ""};
    char *text = text_view_print(&f->source, view, limits, &error);
    if (!CHECKF(text != NULL, "%s %s: %s", name, view, error.message)) {
        return;
    }
    char file[128];
    snprintf(file, sizeof file, "%s.%s%s.txt", name, view, suffix);
    for (char *p = file; *p; p++) {
        if (*p == ':') {
            *p = '-';
        }
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%s", TEXT_SNAPSHOTS, file);
    if (test_update_goldens()) {
        CHECKF(test_write_file(path, text, strlen(text)), "cannot write %s", path);
        updated++;
    } else {
        char *golden = test_read_file(path, NULL);
        if (CHECKF(golden != NULL, "no golden %s (run with URSATZ_UPDATE_GOLDENS=1)", path)) {
            CHECKF(strcmp(golden, text) == 0, "%s differs from its golden (URSATZ_UPDATE_GOLDENS=1 rewrites it)\n%s", file, text);
        }
        free(golden);
    }
    free(text);
}

static void expect_refusal(const Fixture *f, const char *view, const TextViewLimits *limits, const char *message) {
    TextError error = {-1, ""};
    char *text = text_view_print(&f->source, view, limits, &error);
    CHECKF(text == NULL, "%s: printed", view);
    CHECKF(error.line == 0 && strcmp(error.message, message) == 0, "%s: wanted '%s', got line %d '%s'", view, message, error.line, error.message);
    free(text);
}

static bool contains(const char *text, const char *part) {
    return text && strstr(text, part) != NULL;
}

static void test_goldens(void) {
    char plan[512], real[512], reading[512];
    snprintf(plan, sizeof plan, "%s/loop.plan.json", TEXT_FIXTURES);
    snprintf(real, sizeof real, "%s/loop.realization.json", TEXT_FIXTURES);
    snprintf(reading, sizeof reading, "%s/loop.reading.json", TEXT_FIXTURES);
    Fixture loop;
    load(&loop, "loop", plan, real, reading);
    static const char *const LOOP_VIEWS[] = {"summary", "structure", "harmony", "line:melody", "part:piano", "reading"};
    for (size_t i = 0; i < sizeof LOOP_VIEWS / sizeof LOOP_VIEWS[0]; i++) {
        check_golden(&loop, "loop", LOOP_VIEWS[i], NULL, "");
    }
    check_golden(&loop, "loop", "line:bass", NULL, "");

    /* a measure range and a section */
    TextViewLimits bars = {3, 4, NULL};
    check_golden(&loop, "loop", "summary", &bars, ".bars-3-4");
    check_golden(&loop, "loop", "harmony", &bars, ".bars-3-4");
    TextViewLimits section = {0, 0, "a"};
    check_golden(&loop, "loop", "summary", &section, ".section-a");
    check_golden(&loop, "loop", "harmony", &section, ".section-a");
    check_golden(&loop, "loop", "reading", &bars, ".bars-3-4");
    TextError error = {0};
    char *text = text_view_print(&loop.source, "harmony", &bars, &error);
    CHECK(contains(text, "# leaves out: notes, chord spellings, functions (--view full), bars outside 3-4\n"));
    CHECK(contains(text, "ursatz-text 1 view harmony loop\n") && contains(text, "\nbar 3  ") && !contains(text, "\nbar 2 ") && !contains(text, "\nbar 5 "));
    free(text);
    text = text_view_print(&loop.source, "summary", &section, &error);
    CHECK(contains(text, "  a  bars 3-10") && !contains(text, "  intro  ") && !contains(text, "  b  "));
    free(text);
    TextViewLimits both = {5, 6, "a"};
    text = text_view_print(&loop.source, "harmony", &both, &error);
    CHECK(contains(text, "bar 5") && contains(text, "bar 6") && !contains(text, "bar 4") && !contains(text, "bar 7"));
    free(text);

    /* every refusal that needs the loop */
    expect_refusal(&loop, "nothing", NULL, "unknown view 'nothing'");
    expect_refusal(&loop, "line:tenor", NULL, "unknown line 'tenor'");
    expect_refusal(&loop, "part:organ", NULL, "unknown part 'organ'");
    TextViewLimits unknown = {0, 0, "zz"};
    expect_refusal(&loop, "summary", &unknown, "unknown section 'zz'");
    TextViewLimits outside = {40, 50, NULL};
    char message[128];
    text = text_view_print(&loop.source, "summary", NULL, &error);
    int last = 0;
    const char *at = text ? strstr(text, "bars 1-") : NULL;
    if (at) {
        last = atoi(at + 7);
    }
    free(text);
    snprintf(message, sizeof message, "measures 40-50 are outside the piece (bars 1-%d)", last);
    CHECK(last > 0);
    outside.first_measure = last + 1;
    outside.last_measure = last + 5;
    snprintf(message, sizeof message, "measures %d-%d are outside the piece (bars 1-%d)", last + 1, last + 5, last);
    expect_refusal(&loop, "harmony", &outside, message);
    unload(&loop);

    Fixture fill;
    snprintf(plan, sizeof plan, "%s/composer/tests/snapshots/fill.plan.json", TEXT_SOURCE_ROOT);
    load(&fill, "fill", plan, NULL, NULL);
    static const char *const FILL_VIEWS[] = {"summary", "structure", "harmony", "line:melody"};
    for (size_t i = 0; i < sizeof FILL_VIEWS / sizeof FILL_VIEWS[0]; i++) {
        check_golden(&fill, "fill", FILL_VIEWS[i], NULL, "");
    }
    TextViewLimits section_b = {0, 0, "B"};
    check_golden(&fill, "fill", "summary", &section_b, ".section-B");
    expect_refusal(&fill, "part:piano", NULL, "part:piano needs a realization");
    expect_refusal(&fill, "reading", NULL, "reading needs a reading");
    unload(&fill);

    Fixture bach;
    snprintf(real, sizeof real, "%s/bach-114.realization.json", TEXT_FIXTURES);
    snprintf(reading, sizeof reading, "%s/bach-114.reading.json", TEXT_FIXTURES);
    load(&bach, "bach-114", NULL, real, reading);
    static const char *const BACH_VIEWS[] = {"summary", "harmony", "part:P1", "reading"};
    for (size_t i = 0; i < sizeof BACH_VIEWS / sizeof BACH_VIEWS[0]; i++) {
        check_golden(&bach, "bach-114", BACH_VIEWS[i], NULL, "");
    }
    check_golden(&bach, "bach-114", "structure", NULL, "");
    unload(&bach);

    /* a layer short */
    Fixture only_real;
    load(&only_real, "bach-114", NULL, real, NULL);
    expect_refusal(&only_real, "summary", NULL, "summary needs a plan or a reading");
    expect_refusal(&only_real, "structure", NULL, "structure needs a plan or a reading");
    expect_refusal(&only_real, "harmony", NULL, "harmony needs a plan or a reading");
    expect_refusal(&only_real, "reading", NULL, "reading needs a reading");
    unload(&only_real);
    Fixture only_reading;
    load(&only_reading, "x", NULL, NULL, reading);
    expect_refusal(&only_reading, "line:melody", NULL, "line:melody needs a realization or a plan");
    expect_refusal(&only_reading, "part:P1", NULL, "part:P1 needs a realization");
    unload(&only_reading);
}

static void test_frame(void) {
    char *framed = text_view_frame("reading", "piece:abc", "bar 1\n  chord I  @1", "nothing the document holds", "ursatz show piece:abc --view reading --measures a-b");
    CHECK(framed && strcmp(framed, "ursatz-text 1 view reading piece:abc\nbar 1\n  chord I  @1\n# leaves out: nothing the document holds\n"
                                   "# expand: ursatz show piece:abc --view reading --measures a-b\n") == 0);
    free(framed);
    framed = text_view_frame("summary", "x", "", NULL, NULL);
    CHECK(framed && strcmp(framed, "ursatz-text 1 view summary x\n# leaves out: nothing\n# expand: ursatz show x --view full\n") == 0);
    free(framed);
}

int main(void) {
    char message[256] = "";
    if (knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof message) != KNOWLEDGE_OK) {
        fprintf(stderr, "snapshot: %s\n", message);
        return 1;
    }
    test_goldens();
    test_frame();
    knowledge_snapshot_destroy(snapshot);
    if (test_update_goldens()) {
        printf("text_views: %d goldens written\n", updated);
        return test_goldens_updated("text_views");
    }
    return test_finish("text_views");
}
