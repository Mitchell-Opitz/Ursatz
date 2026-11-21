#include "flags.h"

#include <stdio.h>
#include <string.h>

const CliFlag CLI_FLAGS[] = {
    {"--above", NULL, CLI_FLAG_SWITCH, NULL, "the line is the bass and the counter-melody sings above it"},
    {"--accompaniment", NULL, CLI_FLAG_VALUE, "<pattern>", "write an accompaniment in the inner voices: block, arpeggiated, alberti, oom-pah or any"},
    {"--accompaniment-format", NULL, CLI_FLAG_VALUE, "<format>", "the format of the accompaniment motive's file, as --input-format"},
    {"--accompaniment-motive", NULL, CLI_FLAG_VALUE, "<file>", "a saved accompaniment motive, restated on each chord in chord tones"},
    {"--accompaniment-pickup", NULL, CLI_FLAG_VALUE, "<beats>", "the pickup of the accompaniment motive, as 1 or 1/2"},
    {"--actor", NULL, CLI_FLAG_VALUE, "<name>", "global: who a command is journaled as (default claude); stands before the subcommand"},
    {"--amount", NULL, CLI_FLAG_VALUE, "<n>", "interval: change the interval by n semitones, negative to narrow it"},
    {"--at", NULL, CLI_FLAG_REPEAT, "<L>[:<bar>]", "where the --given-* before it goes: a section letter and the bar of it the material starts in"},
    {"--axis", NULL, CLI_FLAG_VALUE, "<pitch>", "invert: the pitch to mirror around, D4 or a MIDI number"},
    {"--backup", NULL, CLI_FLAG_VALUE, "<dir>", "migrate: where the copy of the whole folder goes before the migration (default: beside it, named <folder>-backup-<time>)"},
    {"--bars", NULL, CLI_FLAG_VALUE, "<n>", "the length in bars"},
    {"--base", NULL, CLI_FLAG_VALUE, "<n>", "the version of the item a command is made against (default: its current version); a newer one refuses the command"},
    {"--basic", NULL, CLI_FLAG_VALUE, "<file>", "the basic motive the family of forms comes from; remoteness is measured from it"},
    {"--basic-format", NULL, CLI_FLAG_VALUE, "<format>", "the format of the basic motive's file, as --input-format"},
    {"--basic-harmony", NULL, CLI_FLAG_VALUE, "<text>", "the basic motive's own chords, as --harmony"},
    {"--basic-pickup", NULL, CLI_FLAG_VALUE, "<beats>", "the basic motive's pickup, as --pickup"},
    {"--bass", NULL, CLI_FLAG_SWITCH, NULL, "write a bass line (the default)"},
    {"--below", NULL, CLI_FLAG_SWITCH, NULL, "the line is the melody and the counter-melody sings below it"},
    {"--better", NULL, CLI_FLAG_VALUE, "<batch>", "rate: the batch of the round that sounded better, or same"},
    {"--blend", NULL, CLI_FLAG_VALUE, "<file>", "a second style model file, blended with --style"},
    {"--blend-weight", NULL, CLI_FLAG_VALUE, "<n/d>", "the second style's share in every dimension, 0 to 1"},
    {"--blend-weights", NULL, CLI_FLAG_VALUE, "<list>", "the second style's share per dimension, as harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>"},
    {"--by", NULL, CLI_FLAG_VALUE, "<n>", "restate: move the motive by n semitones, or n steps with --tonal"},
    {"--cadence", NULL, CLI_FLAG_VALUE, "<type>", "the cadence the phrase ends in: PAC, IAC, HC, DC or PC"},
    {"--candidates", NULL, CLI_FLAG_VALUE, "<n>", "compose n candidates and write the best"},
    {"--cells", NULL, CLI_FLAG_REPEAT, "<n>", "the cells each chord of the --given-progression before it lasts"},
    {"--chain", NULL, CLI_FLAG_VALUE, "<file>", "a JSON file of steps, the same chain of operations for every variant"},
    {"--check", NULL, CLI_FLAG_SWITCH, NULL, "analyze the result with the Ursatz Analyzer and pass over a seed whose analysis contradicts the plan"},
    {"--count", NULL, CLI_FLAG_VALUE, "<n>", "how many to make"},
    {"--data", NULL, CLI_FLAG_VALUE, "<dir>", "global: the data folder (or the environment variable URSATZ_DATA); stands before the subcommand"},
    {"--degree", NULL, CLI_FLAG_VALUE, "<n>", "restate: move the first note to scale degree n"},
    {"--depth", NULL, CLI_FLAG_VALUE, "<n>", "the most operations in the chain, 1 to 4"},
    {"--development", NULL, CLI_FLAG_VALUE, "<name>", "how a theme develops: melody, balanced or theme"},
    {"--distance", NULL, CLI_FLAG_VALUE, "<d>", "how far the variants are from the motive: close, medium or far"},
    {"--drop", NULL, CLI_FLAG_VALUE, "<n>", "liquidate: how many notes to drop at each step"},
    {"--dry-run", NULL, CLI_FLAG_SWITCH, NULL, "migrate: report every planned move and rewrite, and write nothing"},
    {"--each-op", NULL, CLI_FLAG_SWITCH, NULL, "one variant for every operation that applies to the motive's form"},
    {"--embellish", NULL, CLI_FLAG_VALUE, "<level>", "how often chords are prolonged: none, low, medium or high"},
    {"--ending-midi", NULL, CLI_FLAG_VALUE, "<file>", "write the ending version as a MIDI file"},
    {"--ending-musicxml", NULL, CLI_FLAG_VALUE, "<file>", "write the ending version as MusicXML"},
    {"--ending-out", NULL, CLI_FLAG_VALUE, "<file>", "write the ending version's plan as JSON"},
    {"--explain", NULL, CLI_FLAG_VALUE, "<file>", "write why each candidate was chosen or rejected"},
    {"--feature", NULL, CLI_FLAG_REPEAT, "<l:a-b>", "a labelled run of notes by 0-based index, as a:0-2; repeatable"},
    {"--first-cadence", NULL, CLI_FLAG_VALUE, "<type>", "the weaker cadence an antecedent ends in: HC or IAC"},
    {"--form", NULL, CLI_FLAG_VALUE, "<name>", "the form: for a motive full, rhythm or contour; for a plan binary, ternary, loop and the like"},
    {"--format", NULL, CLI_FLAG_VALUE, "<text|json>", "how to print the result on stdout, text (the default) or json"},
    {"--from", NULL, CLI_FLAG_VALUE, "<loop-plan.json>", "work on a loop plan the command wrote, with --request"},
    {"--from-corpus", NULL, CLI_FLAG_VALUE, "<dir>", "start from material in a corpus output folder, with --like"},
    {"--function", NULL, CLI_FLAG_REPEAT, "<name>", "keep only variants that suit a function: beginning, continuation, contrast or cadential"},
    {"--given-bass", NULL, CLI_FLAG_REPEAT, "<file>", "a bass line to place in the piece, no rests"},
    {"--given-melody", NULL, CLI_FLAG_REPEAT, "<file>", "a phrase to place in the piece, which fill never changes"},
    {"--given-progression", NULL, CLI_FLAG_REPEAT, "<numerals>", "Roman numerals, one chord each, to place in the piece"},
    {"--harmony", NULL, CLI_FLAG_VALUE, "<text>", "the motive's own chords as Roman numerals with onsets in beats after @, as 'I V6@2 I@3'"},
    {"--hybrid", NULL, CLI_FLAG_VALUE, "<b>+<e>", "with --into hybrid: the beginning and the ending, as antecedent+continuation"},
    {"--input-format", NULL, CLI_FLAG_VALUE, "<format>", "the format of the input: auto, midi, musicxml, notes, degrees, rhythm, contour or json"},
    {"--interval", NULL, CLI_FLAG_VALUE, "<n>", "interval: the 1-based interval to change, every one by default"},
    {"--into", NULL, CLI_FLAG_VALUE, "<form>", "the theme to grow: sentence, period or hybrid"},
    {"--keep-contour", NULL, CLI_FLAG_SWITCH, NULL, "interval: keep the contour of the motive"},
    {"--keep-rhythm", NULL, CLI_FLAG_SWITCH, NULL, "liquidate: keep the rhythm of the motive"},
    {"--key", NULL, CLI_FLAG_VALUE, "<key>", "the key, for example 'D minor'"},
    {"--lane", NULL, CLI_FLAG_VALUE, "<lane>", "harmony, bass, melody or inner (and all, where it is the default)"},
    {"--layers", NULL, CLI_FLAG_VALUE, "<dir>", "write the layers of the result into this folder: the realization, the reading and, for a composed piece, the plan"},
    {"--length", NULL, CLI_FLAG_VALUE, "<n>", "fragment: how many notes the fragment has"},
    {"--like", NULL, CLI_FLAG_VALUE, "<file>", "a MIDI or MusicXML passage; the first bar of a similar passage of the corpus becomes the motive"},
    {"--like-progression", NULL, CLI_FLAG_REPEAT, "<numerals>", "a progression like the one of the passage of the corpus that best matches these numerals"},
    {"--limit", NULL, CLI_FLAG_VALUE, "<n>", "how many records or results at most"},
    {"--list-ops", NULL, CLI_FLAG_SWITCH, NULL, "print every operation, with its parameters and the forms it applies to"},
    {"--list-schemas", NULL, CLI_FLAG_SWITCH, NULL, "list the schema and sequence names and exit"},
    {"--material", "--mode", CLI_FLAG_VALUE, "<word>", "major or minor (style, where the style names one)"},
    {"--measures", NULL, CLI_FLAG_VALUE, "<first>-<last>", "the measures to show, <first>-<last>, inclusive: as written in the score for analyze --view reading, bars from 1 for show"},
    {"--meter", NULL, CLI_FLAG_VALUE, "<n/m>", "the meter, for example 4/4"},
    {"--midi", NULL, CLI_FLAG_VALUE, "<file>", "write the result as a MIDI file"},
    {"--min", NULL, CLI_FLAG_VALUE, "<n>", "fragment or liquidate: the fewest notes that stay"},
    {"--model-plan", NULL, CLI_FLAG_VALUE, "<file>", "write the minimal plan of the Analyzer's reading (its sections, keys and chords) to this file"},
    {"--modulate", NULL, CLI_FLAG_SWITCH, NULL, "plan the keys and cadences across the sections from the seed"},
    {"--motif", NULL, CLI_FLAG_VALUE, "<file>", "the motif of every theme: anything the motive command reads"},
    {"--motive", NULL, CLI_FLAG_VALUE, "<file>", "the motive: a MIDI file, a MusicXML file, a note list or a motive JSON file"},
    {"--musicxml", NULL, CLI_FLAG_VALUE, "<file>", "write the result as a MusicXML file"},
    {"--name", NULL, CLI_FLAG_VALUE, "<name>", "the name of the style model"},
    {"--no-bass", NULL, CLI_FLAG_SWITCH, NULL, "write only the chords"},
    {"--no-wait", NULL, CLI_FLAG_SWITCH, NULL, "do not wait for the job a command starts: print its id"},
    {"--node", NULL, CLI_FLAG_VALUE, "<id>", "the node's id in the plan file"},
    {"--note", NULL, CLI_FLAG_VALUE, "<text>", "restore: the note of the new version (default: restore v<n>); rate: why the rating or the pick"},
    {"--op", NULL, CLI_FLAG_VALUE, "<name>", "the operation: restate, invert, retrograde, interval, fragment, liquidate, augment or diminish"},
    {"--out", NULL, CLI_FLAG_VALUE, "<file>", "where the output is written: a plan, a corpus output folder or a style model"},
    {"--out-dir", NULL, CLI_FLAG_VALUE, "<dir>", "where the files are written; created if missing"},
    {"--params", NULL, CLI_FLAG_VALUE, "<file>", "a JSON file with the params object, or - for standard input"},
    {"--pickup", NULL, CLI_FLAG_VALUE, "<beats>", "the beats before the first barline, as 1 or 1/2"},
    {"--plain-midi", NULL, CLI_FLAG_SWITCH, NULL, "MIDI: no track names and no section markers"},
    {"--port", NULL, CLI_FLAG_VALUE, "<n>", "global: the HTTP port of the workshop, 1 to 65535; stands before the subcommand"},
    {"--position", NULL, CLI_FLAG_VALUE, "<where>", "fragment: leading, trailing or middle"},
    {"--print-settings", NULL, CLI_FLAG_SWITCH, NULL, "print the default preferences document and exit"},
    {"--rating", NULL, CLI_FLAG_REPEAT, "<r>", "rate: <batch>=<1-5> for a sample of a session, or <1-5> for an item version; repeatable"},
    {"--ratio", NULL, CLI_FLAG_VALUE, "<n:m>", "augment or diminish: the ratio of the new durations to the old"},
    {"--record", NULL, CLI_FLAG_VALUE, "<file>", "write what the run did: seeds, candidates and the check"},
    {"--regenerate", NULL, CLI_FLAG_VALUE, "<id|none>", "write the section with that id again, the rest locked"},
    {"--render-key", NULL, CLI_FLAG_VALUE, "<key>", "hear the motive in this key, with the same degrees"},
    {"--request", NULL, CLI_FLAG_VALUE, "<request.json>", "the request a loop plan was made from, with --from"},
    {"--resume", NULL, CLI_FLAG_SWITCH, NULL, "skip the pieces unchanged since the last run"},
    {"--role", NULL, CLI_FLAG_VALUE, "<role>", "melodic or accompaniment"},
    {"--round", NULL, CLI_FLAG_VALUE, "<n>", "listen --session and rate: the round of the listening session, 1 to 10000"},
    {"--schema", NULL, CLI_FLAG_VALUE, "<name>", "build the start of the phrase on a galant schema (see --list-schemas)"},
    {"--section", NULL, CLI_FLAG_VALUE, "<letter>", "the section with that form letter or id"},
    {"--seed", NULL, CLI_FLAG_VALUE, "<n>", "a whole number from 0; the same seed gives the same output"},
    {"--sequence", NULL, CLI_FLAG_VALUE, "<name>", "build a continuation on a harmonic sequence (see --list-schemas)"},
    {"--settings", NULL, CLI_FLAG_VALUE, "<file>", "a preferences document"},
    {"--species", NULL, CLI_FLAG_VALUE, "<1-4>", "the species of counterpoint"},
    {"--strategy", NULL, CLI_FLAG_VALUE, "<name>", "how motifs are made: segments or drawn"},
    {"--style", NULL, CLI_FLAG_VALUE, "<file>", "a style model file from the Ursatz Analyzer; prefer the harmony and melody closest to it"},
    {"--style-weight", NULL, CLI_FLAG_VALUE, "<n/d>", "how strongly the style is preferred"},
    {"--style-without", NULL, CLI_FLAG_REPEAT, "<category>", "read the style without this category; repeatable"},
    {"--suggest", NULL, CLI_FLAG_VALUE, "<n>", "write n alternatives instead of one; with files, number k goes to <name>-k.<extension>"},
    {"--target-feature", NULL, CLI_FLAG_REPEAT, "<l:a-b>", "a labelled run of the target's notes; repeatable"},
    {"--target-format", NULL, CLI_FLAG_VALUE, "<format>", "the target's input format, as --input-format"},
    {"--target-harmony", NULL, CLI_FLAG_VALUE, "<text>", "the target's own chords, as --harmony"},
    {"--target-pickup", NULL, CLI_FLAG_VALUE, "<beats>", "the target's pickup, as --pickup"},
    {"--tempo", NULL, CLI_FLAG_VALUE, "<bpm>", "the tempo of the files in quarter notes per minute, 20 to 400, a whole number or a fraction like 195/2"},
    {"--texture", NULL, CLI_FLAG_VALUE, "<name>", "the texture the accompaniment is to be heard as"},
    {"--theme-request", NULL, CLI_FLAG_VALUE, "<file>", "the theme request every theme of the batch is made from"},
    {"--title", NULL, CLI_FLAG_VALUE, "<text>", "the title of the text export (with --view reading)"},
    {"--tonal", NULL, CLI_FLAG_SWITCH, NULL, "work in scale steps of the key and not in semitones"},
    {"--tone", NULL, CLI_FLAG_REPEAT, "<n>=<role>", "label note n (1-based) as chord or as a non-chord tone; repeatable"},
    {"--update-uses", NULL, CLI_FLAG_SWITCH, NULL, "compose: move every updated use to its source's current version first"},
    {"--value-scale", NULL, CLI_FLAG_VALUE, "<n or n/d>", "read the note values of the file scaled by this factor (a notation reading)"},
    {"--variations", NULL, CLI_FLAG_VALUE, "<id>", "make --count different versions of the section with that id"},
    {"--version", NULL, CLI_FLAG_VALUE, "<n>", "regenerate of an item: the version of the piece the section is written again from (default: the current one)"},
    {"--view", NULL, CLI_FLAG_VALUE, "<view>", "a view of the result: reading is the measure-addressed text export of analyze, framed as a view; show takes summary, structure, harmony, line:<name>, part:<name>, reading, full or json"},
    {"--voicing", NULL, CLI_FLAG_SWITCH, NULL, "keep the grown melody as the soprano and add an alto and a tenor under it"},
    {"--wait", NULL, CLI_FLAG_SWITCH, NULL, "wait for the job a command starts, and say how it ended"},
    {"--what", NULL, CLI_FLAG_VALUE, "<what>", "motifs or themes"},
    {"--write-midi", NULL, CLI_FLAG_SWITCH, NULL, "also write each version as a MIDI file"},
    {"--write-musicxml", NULL, CLI_FLAG_SWITCH, NULL, "also write each version as a MusicXML file"},
};

const size_t CLI_FLAG_COUNT = sizeof(CLI_FLAGS) / sizeof(CLI_FLAGS[0]);

_Static_assert(sizeof(CLI_FLAGS) / sizeof(CLI_FLAGS[0]) <= CLI_FLAG_CAPACITY, "CLI_FLAG_CAPACITY is smaller than the flag table");

/* The index of the flag named name (or whose alias it is), CLI_FLAG_COUNT when there is none. */
static size_t index_of(const char *name) {
    size_t low = 0;
    size_t high = CLI_FLAG_COUNT;
    while (name && low < high) {
        size_t middle = (low + high) / 2;
        int order = strcmp(name, CLI_FLAGS[middle].name);
        if (order == 0) {
            return middle;
        }
        if (order < 0) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    for (size_t i = 0; name && i < CLI_FLAG_COUNT; i++) {
        if (CLI_FLAGS[i].alias && strcmp(CLI_FLAGS[i].alias, name) == 0) {
            return i;
        }
    }
    return CLI_FLAG_COUNT;
}

const CliFlag *cli_flag_find(const char *name) {
    size_t index = index_of(name);
    return index < CLI_FLAG_COUNT ? &CLI_FLAGS[index] : NULL;
}

/* Whether the command's row names the flag. */
static bool command_takes(const CliCommand *command, const CliFlag *flag) {
    for (const char *const *name = command->flags; name && *name; name++) {
        if (strcmp(*name, flag->name) == 0) {
            return true;
        }
    }
    return false;
}

static void inputs_message(const CliCommand *command, char *message, size_t size) {
    if (command->inputs_max == 0) {
        snprintf(message, size, "%s takes no input", command->name);
    } else if (command->inputs_min == command->inputs_max) {
        snprintf(message, size, "%s takes %d input%s", command->name, command->inputs_max, command->inputs_max == 1 ? "" : "s");
    } else {
        snprintf(message, size, "%s takes at most %d input%s", command->name, command->inputs_max, command->inputs_max == 1 ? "" : "s");
    }
}

bool cli_parse(const CliCommand *command, int argc, char **argv, CliArgs *out, char *message, size_t size) {
    memset(out, 0, sizeof *out);
    out->argc = argc;
    out->argv = argv;
    out->command = command;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            out->help = true;
            continue;
        }
        if (arg[0] != '-' || arg[1] == '\0') {
            if ((int)out->input_count >= command->inputs_max || out->input_count >= CLI_MAX_INPUTS) {
                inputs_message(command, message, size);
                return false;
            }
            out->inputs[out->input_count++] = arg;
            continue;
        }
        size_t index = index_of(arg);
        const CliFlag *flag = index < CLI_FLAG_COUNT ? &CLI_FLAGS[index] : NULL;
        if (!flag || !command_takes(command, flag)) {
            snprintf(message, size, "unknown option '%s' (see ursatz %s --help)", arg, command->name);
            return false;
        }
        if (flag->kind == CLI_FLAG_SWITCH) {
            out->given[index] = true;
            continue;
        }
        if (i + 1 >= argc) {
            snprintf(message, size, "%s needs a value", arg);
            return false;
        }
        const char *value = argv[++i];
        if (flag->kind == CLI_FLAG_VALUE) {
            if (out->given[index]) {
                snprintf(message, size, "%s given twice", arg);
                return false;
            }
            out->given[index] = true;
            out->value[index] = value;
        } else {
            if (out->repeat_count >= CLI_MAX_REPEATS) {
                snprintf(message, size, "too many %s", arg);
                return false;
            }
            out->given[index] = true;
            out->repeats[out->repeat_count++] = (CliRepeat){index, value, i - 1};
        }
    }
    return true;
}

bool cli_parse_or_fail(const CliCommand *command, int argc, char **argv, CliArgs *out) {
    char message[256];
    if (cli_parse(command, argc, argv, out, message, sizeof message)) {
        return true;
    }
    fprintf(stderr, "Error: %s\n", message);
    return false;
}

const char *cli_value(const CliArgs *args, const char *flag) {
    size_t index = index_of(flag);
    return index < CLI_FLAG_COUNT ? args->value[index] : NULL;
}

bool cli_switch(const CliArgs *args, const char *flag) {
    size_t index = index_of(flag);
    return index < CLI_FLAG_COUNT && args->given[index];
}

size_t cli_repeat_count(const CliArgs *args, const char *flag) {
    size_t index = index_of(flag);
    size_t count = 0;
    for (size_t i = 0; index < CLI_FLAG_COUNT && i < args->repeat_count; i++) {
        count += args->repeats[i].flag == index;
    }
    return count;
}

const char *cli_repeat_value(const CliArgs *args, const char *flag, size_t n, int *position) {
    size_t index = index_of(flag);
    for (size_t i = 0; index < CLI_FLAG_COUNT && i < args->repeat_count; i++) {
        if (args->repeats[i].flag == index && n-- == 0) {
            if (position) {
                *position = args->repeats[i].position;
            }
            return args->repeats[i].value;
        }
    }
    return NULL;
}

bool cli_flag_global(const char *name) {
    return name && (strcmp(name, "--actor") == 0 || strcmp(name, "--data") == 0 || strcmp(name, "--port") == 0);
}
