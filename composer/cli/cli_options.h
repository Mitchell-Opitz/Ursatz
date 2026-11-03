#ifndef URSATZ_COMPOSER_CLI_OPTIONS_H
#define URSATZ_COMPOSER_CLI_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/corpus_seed.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/style.h>

/* The exit statuses every command shares. */
#define CLI_EXIT_USAGE 1
#define CLI_EXIT_UNSATISFIABLE 2
#define CLI_EXIT_WRITE 3

/* The largest seed, and how many seeds a command tries, one after another from the one given, before it gives up. */
#define CLI_SEED_MAX 4294967295UL
#define CLI_MAX_SEED_TRIES 16

/* Prints "Error: <message>" to stderr and returns status. */
int cli_fail(int status, const char *format, ...);

bool cli_equals_ignore_case(const char *a, const char *b);

/* A whole number from 0 to maximum, digits only. */
bool cli_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out);

/* A tonic letter, an optional # or b, whitespace, then major or minor ("D minor", "F# major"); any case for the mode. */
bool cli_parse_key(const char *text, PlanKey *key);
const char *cli_key_name(const PlanKey *key, char *out, size_t size);

/* "3/4", "6/8". The meters the harmony operators support are checked by the caller (harmony_meter_supported). */
bool cli_parse_meter(const char *text, PlanMeter *meter);

/* PAC, IAC, HC, DC or PC, any case. */
bool cli_parse_cadence(const char *text, PlanCadenceType *cadence);
const char *cli_cadence_name(PlanCadenceType cadence);

/* "any", "block", "arpeggiated", "alberti", "oom-pah", "motive", by PlanAccompanimentPattern. */
const char *cli_accompaniment_name(PlanAccompanimentPattern pattern);

/*
 * Reads --accompaniment (pattern_text) and --texture (texture_text), either of which may be NULL, into *out. Returns 0, or
 * the status of the usage error it printed. meter_text is the --meter argument, for the message about alberti.
 */
int cli_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text, const PlanMeter *meter,
                           PlanAccompaniment *out);

/*
 * The export options of --tempo (tempo_text, NULL when not given) and --plain-midi: the tempo is a whole number or a
 * fraction like "195/2" of quarter notes per minute, from 20 to 400; --plain-midi turns the MIDI track names and
 * section markers off. Returns 0, or the status of the usage error it printed.
 */
int cli_read_export_options(const char *tempo_text, bool plain_midi, PlanExportOptions *out);

/* "3/2" or "3": a weight, both parts whole numbers up to 4294967295 and the denominator at least 1. */
bool cli_parse_ratio(const char *text, ComposerRatio *out);

/* --style-without values: at most CLI_STYLE_WITHOUT_MAX category names, each one the Analyzer defines. */
#define CLI_STYLE_WITHOUT_MAX 8
typedef struct {
    const char *names[CLI_STYLE_WITHOUT_MAX];
    size_t count;
} CliStyleWithout;

/* Adds one --style-without value. Returns 0, or the status of the usage error it printed (more than 8, or not a category). */
int cli_style_without_add(CliStyleWithout *list, const char *name);

/*
 * Reads the style model file at path into *out, without the named categories (none when count is 0). Returns 0, or the
 * status of the usage error it printed.
 */
int cli_load_style(const char *path, const char *const *without, size_t without_count, ComposerStyle **out);

/*
 * The blend options of grow, plan and fill: --blend <file> (blend_path), --blend-weight <n/d> (all_text) and
 * --blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d> (list_text); any may be NULL. Usage errors, as
 * 0 or the status of the one printed: --blend without a style (has_style false), a weight option without --blend, a
 * weight that is not a fraction from 0 to 1, a dimension that is not one or is named twice.
 */
int cli_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                   ComposerStyleWeights *out);

/* Loads the two style model files and blends them, weights as cli_read_blend read them. Returns 0 or the status. */
int cli_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                   const char *const *without, size_t without_count,
                   ComposerStyle **out);

/* The Analyzer's distance from the style, per dimension and overall (0 is the same, 1 is nothing alike): text lines. */
void cli_print_style_distance(const StyleComparison *comparison);
/* The same as the JSON member "style_distance":{...}, with no leading comma. */
void cli_print_style_distance_json(const StyleComparison *comparison);

/* Seeding from a corpus (corpus_seed.h). Each returns 0 or the status of the failure it printed: a passage or numerals
   that cannot be read is a usage error, a corpus with no match at 1/2 or more exits 2. */
int cli_seed_motive(const char *corpus_dir, const char *like_path, const PlanMeter *meter, const PlanKey *key,
                    unsigned long seed, CorpusSeed *out);
int cli_seed_progression(const char *corpus_dir, const char *numerals_text, PlanKeyMode mode, CorpusSeed *out);
/* The numerals of a progression seed in one string, separated by spaces. */
void cli_seed_numerals_text(const CorpusSeed *seed, char *out, size_t size);
/* "motive from <piece> m.1 b.1 - m.2 b.1, score 7/8", the line the text output carries. */
void cli_print_seed_line(const char *what, const CorpusSeed *seed);
/* The same as the JSON object {"piece":..,"start":..,"end":..,"score":..}, with no leading comma. */
void cli_print_seed_json(const CorpusSeed *seed);
/* The seed as the plan records it. Returns 0 or the status of the failure it printed. */
int cli_seed_source(const CorpusSeed *seed, PlanSource *out);
/* Records the seed in a SOURCE constraint on the node, which must not be Given. Returns 0 or the status of the failure it printed. */
int cli_add_source(PiecePlan *plan, NodeId node, const CorpusSeed *seed);
/* The first node of the plan with the name, or PLAN_NODE_NONE. */
NodeId cli_find_node(const PiecePlan *plan, const char *name);

/* Prints text as a JSON string to stdout: quotes and backslashes escaped, control characters dropped. */
void cli_print_json_string(const char *text);

/* A seed from the clock, for a command given none. */
unsigned long cli_default_seed(void);

/*
 * Writes the explanation (explain.h) to write_path, as JSON when final_path ends in ".json" (any case), else as text.
 * Returns 0, or -1 when the file cannot be written.
 */
int cli_write_explanation(const char *final_path, const char *write_path, const Explanation *explanation);

/* The name of alternative k (from 1) of a file: before the extension, "-k". With one result the name as given. */
bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size);

/*
 * The node (or lane) a command works on: --node <id> or --section <letter>, and --lane. The commands that lock,
 * regenerate and vary share it.
 */
typedef struct {
    const char *node_text;
    const char *section_text;
    const char *lane_text;
    unsigned long node;
    bool has_lane;
    PlanLaneKind lane;
} CliTargetOptions;

/* Checks the three texts of target (exactly one of node and section, a lane word). Returns 0 or the usage status printed. */
int cli_check_target(CliTargetOptions *target, const char *command, const char *usage);
/* The node the options name, or the exit status of the error it printed (none printed when quiet). */
int cli_find_target_node(const PiecePlan *plan, const CliTargetOptions *target, bool quiet, NodeId *out);
/* The targets: the node, or with a lane every span of it under the node. Into a new array the caller frees. */
int cli_find_targets(const PiecePlan *plan, const CliTargetOptions *target, FillTarget **out, size_t *count);
/* The target in words: "section B", "node 12", "the harmony of section B". */
void cli_describe_target(const PiecePlan *plan, const CliTargetOptions *target, char *out, size_t size);
/* Prints why a regenerate result refused the target and returns the exit status. */
int cli_report_refused(RegenerateResult result, const char *what);

/* A file written beside its final name and renamed into place once every file of the command is written. */
typedef struct {
    char final_path[1024];
    char temporary[1032];
} CliPendingFile;

/* Removes the temporary files of the first `count` pending files. */
void cli_discard_pending(CliPendingFile *pending, size_t count);

/* A pickup length ("1", "3/2") in beats of the unit as a time; false when it is not one. */
bool cli_parse_pickup(const char *text, int beat_unit, PlanTime *out);

/* The text of a motive in its own form (notes, rhythm or contour); malloc'd, the caller frees it. */
char *cli_motive_text(const Motive *motive);

#endif
