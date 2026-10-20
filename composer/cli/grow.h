#ifndef URSATZ_COMPOSER_CLI_GROW_H
#define URSATZ_COMPOSER_CLI_GROW_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>

/*
 * The `grow` subcommand: argv[1] is "grow". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the request cannot be satisfied (the motive cannot be loaded or is too long, no seed works)
 *   3  an output file could not be written
 */
int run_grow(int argc, char **argv);

/* What the brief of a run was, for GrowExt.print_brief. */
typedef struct {
    ComposerRatio tempo;      /* a denominator of 0 for none */
    const char *tempo_from;   /* "option", "style" or "none" */
    bool minor;               /* the key's mode */
    const char *mode_from;    /* "request" or "style" */
    bool has_density;         /* the style's density (attacks per beat) */
    ComposerRatio density;
    const char *const *without;
    size_t without_count;
} GrowBrief;

/* At most two chords a bar in a theme of eight bars. */
#define GROW_EXT_CHORDS 16

/*
 * What the `theme` command adds to a grow run (R-166). run_grow_ext reads the grow options from argv as run_grow does,
 * except that the basic motive is not a file: once the key and the meter are known (the style has given the mode), `load`
 * fills the motives, and the run grows from them. Everything else, the seeds, the checks, the files and the output, is
 * grow's, with the slots of each theme and the brief added to it.
 */
typedef struct GrowExt {
    /*
     * Fills basic, the slot motives, the idea motives, the accompaniment motive and the pool; returns 0 or the status of
     * the error printed. The pairing is the theme's, for the ideas it has.
     */
    int (*load)(struct GrowExt *ext, const PlanMeter *meter, const PlanKey *key, const PlanThemePairing *pairing);
    /* Prints, after a theme's developments, the slots of the theme as text, or as the JSON member "slots":[...] when json. */
    void (*print_theme)(const struct GrowExt *ext, const PiecePlan *plan, const GrowOutcome *outcome, bool json);
    /* Prints the brief as text lines, or as the JSON member "brief":{...} (no leading comma) when json. */
    void (*print_brief)(const struct GrowExt *ext, const GrowBrief *brief, bool json);
    bool accompaniment_motive_given; /* set before the run: the accompaniment is the motive, so `--accompaniment motive` needs no file */
    Motive basic;
    bool has_slot[GROW_SLOT_COUNT]; /* the slots with a motive of their own (not basic) */
    Motive slot_motives[GROW_SLOT_COUNT];
    bool has_idea[GROW_IDEA_COUNT]; /* the ideas with a motive of their own (not the basic idea) */
    Motive idea_motives[GROW_IDEA_COUNT];
    bool has_accompaniment_motive;
    Motive accompaniment_motive;
    /* The chords Given over the theme (GrowRequest.chords), numerals written in chord_numerals; set by `load`. */
    GivenChord chords[GROW_EXT_CHORDS];
    char chord_numerals[GROW_EXT_CHORDS][PLAN_ROMAN_NUMERAL_MAX];
    size_t chord_count;
    const GrowPoolForm *pool;
    size_t pool_count;
} GrowExt;

int run_grow_ext(int argc, char **argv, GrowExt *ext);

/* The line of the usage text that names the subcommand. */
#define GROW_USAGE_LINE                                                                                          \
    "       ursatz-composer grow <motive-input> --into <sentence|period|hybrid> [--hybrid <beginning>+<ending>] " \
    "--key <key> --meter <n/m> [options]\n"                                                               \
    "       ursatz-composer grow --from-corpus <dir> --like <file> --into <...> --key <key> --meter <n/m> [options]"

#endif
