#ifndef URSATZ_COMPOSER_CLI_PLAN_H
#define URSATZ_COMPOSER_CLI_PLAN_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/piece_plan.h>

#define PLAN_USAGE_LINE                                                                                                \
    "Usage: ursatz-composer plan --form <name> --key <K> [--mode major|minor] --meter <m> [--bars N] [--count N] "     \
    "[--modulate] [--motive <file> | --from-corpus <dir> --like <file>] [--accompaniment <p>] [--texture <t>] [--given-melody <f> | --given-progression "   \
    "\"<I IV V I>\" | --like-progression \"<I IV V I>\" (with --from-corpus) | --given-bass <f> [--at <L>[:<bar>]] [--cells N]]... [--seed N] [--style <file>] [--style-weight N/D] [--style-without <category>]... [--blend <file>] [--blend-weight N/D | --blend-weights D=N/D,...] --out <plan.json> "           \
    "[--format text|json]"

/* "ursatz-composer plan ..."; argv[1] is "plan". Returns the exit status. */
int run_plan(int argc, char **argv);

#define CLI_MAX_SECTIONS 16

/* One theme section of a piece plan, as the plan and fill commands report it. */
typedef struct {
    NodeId node;
    char name[24];
    char letter[PLAN_SECTION_LETTER_MAX];
    size_t bars; /* 0 when the plan has no meter */
    bool has_key;
    PlanKey key; /* the key the section ends in */
    CadenceType cadences[2];
    size_t cadence_count;
    bool has_pairing;
    PlanThemePairing pairing;
    bool returns;
    char returns_letter[PLAN_SECTION_LETTER_MAX];
    PlanReturnVariation variation;
} CliSection;

/* The sections of the piece in order, material left out; returns how many there are (at most capacity are written). */
size_t cli_piece_sections(const PiecePlan *plan, CliSection *out, size_t capacity);

/* The bars in a length of the plan's meter, rounded; 0 when the plan has no meter. */
size_t cli_bars_in(const PiecePlan *plan, Rational length);
/* "presentation + continuation", "antecedent + consequent". */
void cli_pairing_name(const PlanThemePairing *pairing, char *out, size_t size);

/* "exact", "ornament", "reharmonize", "reregister". */
const char *cli_return_variation_name(PlanReturnVariation variation);

#endif
