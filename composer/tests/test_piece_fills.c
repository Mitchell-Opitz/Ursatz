#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/piece_structure.h>

/*
 * Theme and progression fills (R-170): a piece structure whose sections are given a composed theme or a progression.
 *
 *   fixture  the theme plan the other cases use: the `theme` command run on test_theme_cli's one-motive request
 *            (D minor, an 8-bar sentence), read back as a plan with a realized theme section
 */

#define THEME_REQUEST COMPOSER_FIXTURE_DIR "/theme/one-motive.json"
#define THEME_PLAN "piece_fills_theme.json"

/* The theme command on the request with this seed; its plan goes to THEME_PLAN. */
static void write_theme_plan(unsigned long seed) {
    char line[4096];
    snprintf(line, sizeof(line), "\"%s\" theme \"%s\" --seed %lu --out " THEME_PLAN " >piece_fills_out.txt 2>piece_fills_err.txt",
             COMPOSER_CLI_PATH, THEME_REQUEST, seed);
    int status = system(line);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/* The theme plan the command wrote, read back. */
static PiecePlan *read_theme_plan(unsigned long seed) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    write_theme_plan(seed);
    assert(plan_read_json_file(THEME_PLAN, &plan, &error) == PLAN_OK);
    return plan;
}

/* The theme section of a plan: the first section node that is not the material. */
static NodeId theme_section(const PiecePlan *plan) {
    NodeId form = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        bool material = false;
        for (size_t c = 0; c < plan_node_constraint_count(plan, child); c++) {
            material = material || plan_node_constraint_at(plan, child, c)->kind == PLAN_CONSTRAINT_MATERIAL;
        }
        if (plan_node_kind(plan, child) == PLAN_NODE_SECTION && !material) {
            return child;
        }
    }
    return PLAN_NODE_NONE;
}

static void test_fixture(void) {
    PiecePlan *plan = read_theme_plan(1);
    NodeId theme = theme_section(plan);
    assert(theme != PLAN_NODE_NONE);
    assert(plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    PlanTime eight_bars;
    assert(plan_time_make(8, 1, &eight_bars) == PLAN_OK);
    assert(plan_time_compare(plan_node_span(plan, theme).length, eight_bars) == 0);
    plan_destroy(plan);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "fixture") == 0) {
        test_fixture();
    } else {
        fprintf(stderr, "usage: test_piece_fills fixture\n");
        return 2;
    }
    printf("test_piece_fills %s: ok\n", mode);
    return 0;
}
