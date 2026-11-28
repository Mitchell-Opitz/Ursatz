#include "commands.h"

#include <stdio.h>
#include <string.h>

#include "query.h"
#include "render.h"
#include "apply.h"
#include "show.h"
#include "status.h"
#include "analyze.h"
#include "corpus.h"
#include "counterpoint.h"
#include "critique.h"
#include "knowledge.h"
#include "develop.h"
#include "fill.h"
#include "grow.h"
#include "listen.h"
#include "motive.h"
#include "piece.h"
#include "plan_cmd.h"
#include "progression.h"
#include "random_motive.h"
#include "recognize.h"
#include "items.h"
#include "rate.h"
#include "migrate.h"
#include "regenerate.h"
#include "theme.h"
#include "variants_cmd.h"
#include "variations.h"

#include "flags.h"


static const char *const FLAGS_ANALYZE[] = {"--input-format", "--layers", "--measures", "--meter", "--model-plan", "--pickup", "--title", "--value-scale", "--view", NULL};
static const char *const FLAGS_CORPUS[] = {"--out", "--resume", NULL};
static const char *const FLAGS_STYLE_PROFILE[] = {"--name", "--out", NULL};
static const char *const FLAGS_STATUS[] = {"--format", NULL};
static const char *const FLAGS_QUERY[] = {"--format", "--out", "--params", NULL};
static const char *const FLAGS_SHOW[] = {"--measures", "--view", NULL};
static const char *const FLAGS_HISTORY[] = {"--format", "--limit", NULL};
static const char *const FLAGS_APPLY[] = {"--format", "--wait", NULL};
static const char *const FLAGS_PROGRESSION[] = {"--accompaniment", "--bars", "--bass", "--cadence", "--embellish", "--format", "--key", "--list-schemas", "--meter", "--midi", "--musicxml", "--no-bass", "--out", "--palette", "--plain-midi", "--schema", "--seed", "--sequence", "--suggest", "--tempo", NULL};
static const char *const FLAGS_MOTIVE[] = {"--feature", "--form", "--format", "--harmony", "--input-format", "--key", "--meter", "--midi", "--musicxml", "--pickup", "--render-key", "--role", "--tempo", "--tone", NULL};
static const char *const FLAGS_DEVELOP[] = {"--amount", "--axis", "--by", "--degree", "--drop", "--input-format", "--interval", "--keep-contour", "--keep-rhythm", "--key", "--length", "--meter", "--midi", "--min", "--musicxml", "--op", "--plain-midi", "--position", "--ratio", "--seed", "--suggest", "--tempo", "--tonal", NULL};
static const char *const FLAGS_VARIANTS[] = {"--basic", "--basic-format", "--basic-harmony", "--basic-pickup", "--chain", "--count", "--distance", "--each-op", "--feature", "--form", "--format", "--function", "--harmony", "--input-format", "--key", "--list-ops", "--meter", "--pickup", "--role", "--seed", NULL};
static const char *const FLAGS_RECOGNIZE[] = {"--basic", "--basic-format", "--basic-harmony", "--basic-pickup", "--depth", "--feature", "--form", "--format", "--harmony", "--input-format", "--key", "--meter", "--pickup", "--role", "--target-feature", "--target-format", "--target-harmony", "--target-pickup", NULL};
static const char *const FLAGS_RANDOM_MOTIVE[] = {"--count", "--form", "--format", "--key", "--material", "--meter", "--role", "--seed", "--strategy", "--style", "--style-without", NULL};
static const char *const FLAGS_COUNTERPOINT[] = {"--above", "--below", "--format", "--key", "--material", "--meter", "--midi", "--musicxml", "--out", "--plain-midi", "--seed", "--species", "--suggest", "--tempo", NULL};
static const char *const FLAGS_GROW[] = {"--accompaniment", "--accompaniment-format", "--accompaniment-motive", "--accompaniment-pickup", "--bars", "--blend", "--blend-weight", "--blend-weights", "--cadence", "--check", "--explain", "--feature", "--first-cadence", "--format", "--from-corpus", "--harmony", "--hybrid", "--into", "--key", "--like", "--material", "--meter", "--midi", "--musicxml", "--out", "--plain-midi", "--role", "--seed", "--style", "--style-weight", "--style-without", "--suggest", "--tempo", "--texture", "--voicing", NULL};
static const char *const FLAGS_THEME[] = {"--check", "--explain", "--format", "--layers", "--midi", "--musicxml", "--out", "--plain-midi", "--seed", "--suggest", NULL};
static const char *const FLAGS_PIECE[] = {"--check", "--count", "--ending-midi", "--ending-musicxml", "--ending-out", "--explain", "--format", "--from", "--lane", "--layers", "--midi", "--musicxml", "--out", "--out-dir", "--plain-midi", "--regenerate", "--request", "--seed", "--variations", NULL};
static const char *const FLAGS_PLAN[] = {"--accompaniment", "--at", "--bars", "--blend", "--blend-weight", "--blend-weights", "--cells", "--count", "--form", "--format", "--from-corpus", "--given-bass", "--given-melody", "--given-progression", "--key", "--like", "--like-progression", "--material", "--meter", "--modulate", "--motive", "--out", "--seed", "--style", "--style-weight", "--style-without", "--texture", NULL};
static const char *const FLAGS_FILL[] = {"--blend", "--blend-weight", "--blend-weights", "--candidates", "--check", "--explain", "--format", "--layers", "--midi", "--musicxml", "--out", "--plain-midi", "--record", "--seed", "--style", "--style-weight", "--style-without", "--tempo", NULL};
static const char *const FLAGS_LOCK[] = {"--format", "--lane", "--node", "--out", "--section", NULL};
static const char *const FLAGS_REGENERATE[] = {"--base", "--format", "--lane", "--midi", "--musicxml", "--no-wait", "--node", "--out", "--plain-midi", "--section", "--seed", "--tempo", "--version", NULL};
static const char *const FLAGS_MIGRATE[] = {"--backup", "--dry-run", NULL};
static const char *const FLAGS_COMPOSE[] = {"--base", "--no-wait", "--seed", "--update-uses", NULL};
static const char *const FLAGS_RESTORE[] = {"--base", "--note", NULL};
static const char *const FLAGS_EXPLORE[] = {"--base", "--distance", "--no-wait", "--seed", "--taste", NULL};
static const char *const FLAGS_KNOWLEDGE[] = {"--format", "--kind", NULL};
static const char *const FLAGS_CRITIQUE[] = {"--format", "--note", "--rated", "--settings", "--verdict", NULL};
static const char *const FLAGS_RATE[] = {"--better", "--note", "--rating", "--round", "--seed", NULL};
static const char *const FLAGS_VARIATIONS[] = {"--count", "--format", "--lane", "--node", "--out-dir", "--plain-midi", "--section", "--seed", "--tempo", "--write-midi", "--write-musicxml", NULL};
static const char *const FLAGS_HARMONIZE[] = {"--count", "--format", "--out-dir", "--plain-midi", "--section", "--seed", "--tempo", "--write-midi", "--write-musicxml", NULL};
static const char *const FLAGS_RENDER[] = {"--midi", "--musicxml", "--plain-midi", "--tempo", NULL};
static const char *const FLAGS_LISTEN[] = {"--bars", "--batch", "--blend", "--blend-weight", "--blend-weights", "--blueprint", "--check", "--count", "--development", "--into", "--material", "--meter", "--motif", "--out-dir", "--plans", "--preset", "--print-settings", "--round", "--seed", "--session", "--settings", "--strategy", "--style", "--taste", "--tempo", "--theme-request", "--what", NULL};

const CliCommand CLI_COMMANDS[] = {
    {"progression",
    "progression --key <key> --meter <n/m> --bars <n> --cadence <type> [--seed <n>] [--embellish <level>] [--bass | --no-bass] [--schema <name>] [--sequence <name>] [--palette <file>] [--accompaniment <pattern>] [--list-schemas] [--suggest <n>] [--out <file>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format <text|json>]",
     "Write a chord progression with a bass line for one phrase",
    "Writes a chord progression for one phrase, with a bass line, ending in the cadence asked for.\n"
    "\n"
    "Notes on options:\n"
    "  --key takes tonic and mode in one argument (\"D minor\", \"F# major\"; any case); --meter is 2/4, 3/4, 4/4 or\n"
    "  6/8; --bars is 1 to 64 (a cadence needs at least 2 bars in 4/4 and 6/8, 3 in 2/4 and 3/4, more for a plagal\n"
    "  cadence); --cadence is PAC, IAC, HC, DC or PC (any case).\n"
    "  --seed: the same seed gives the same output; the default is derived from the clock and the seed used is\n"
    "  always printed (if no bass line suits the progression the next seeds are tried and the one that worked is\n"
    "  printed). --embellish is none (default), low, medium or high. --bass is the default; --no-bass writes only\n"
    "  the chords (and --midi and --musicxml need the bass). --suggest is 1 to 8; with files, alternative k goes to\n"
    "  <name>-k.<extension>. --out writes the plan as JSON. --tempo is a whole number or a fraction like 195/2; the\n"
    "  default is none (the file default of 120 applies). --palette names a JSON file holding the palette object of the\n"
    "  request (chords drawn from one or two modes, spans, schemas, shapes: cli/docs/cli-progression.md); with it --cadence\n"
    "  may be none, and --accompaniment (a pattern name or a K11 accompaniment id) sounds the chords.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied;\n"
    "3 an output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz progression --key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 7 \\\n"
    "      --embellish medium --out phrase.json --midi phrase.mid\n"
    "  ursatz progression --key \"G major\" --meter 4/4 --bars 8 --cadence PAC --schema romanesca\n"
    "  ursatz progression --key \"A minor\" --meter 3/4 --bars 6 --cadence HC --suggest 3",
     FLAGS_PROGRESSION, 0, 0, run_progression},
    {"motive",
    "motive <input> [--meter <n/m>] [--key <key>] [--input-format <format>] [--form <name>] [--pickup <beats>] [--harmony <text>] [--feature <l:a-b>]... [--role <role>] [--tone <n>=<role>]... [--render-key <key>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--format <text|json>]",
     "Analyze a motive: its intervals, contour, rhythm, range and harmony",
    "Loads a motive from a file and prints its analysis: intervals, contour, rhythm, range,\n"
    "scale degrees and implied harmony.\n"
    "\n"
    "  <input>              a MIDI file, a MusicXML file, a note list or a motive JSON file\n"
    "\n"
    "Notes on options:\n"
    "  --meter is for input that has none (a note list); a MIDI or MusicXML file that declares another meter is an\n"
    "  error. --key replaces a key the file states. --input-format is auto (default), midi, musicxml, notes,\n"
    "  degrees, rhythm, contour or json. --form is full (default), rhythm or contour: rhythm and contour read the\n"
    "  text as a rhythm (x:q x:e) or a contour (0 2 1 3) instead of a note list. --pickup defaults to 0. --harmony\n"
    "  is the motive's own chords as Roman numerals with onsets in beats after @, as \"I V6@2 I@3\" (the first needs\n"
    "  no onset), up to 8 chords. --feature is repeatable, up to 8. --role is melodic (default) or accompaniment.\n"
    "  --tone labels note n (1-based) as chord or as a non-chord tone: passing, neighbor, appoggiatura, suspension,\n"
    "  retardation, escape, anticipation, double-neighbor, changing-tone or pedal; repeatable. --render-key\n"
    "  defaults to the motive's key. --musicxml writes a pickup as an implicit first measure. --format json prints\n"
    "  the analysis as JSON, with the rendered notes when a render option is given.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the input cannot be loaded; 3 a file cannot be written.\n"
    "\n"
    "Example:\n"
    "  ursatz motive theme.txt --meter 4/4 --key \"D minor\"",
     FLAGS_MOTIVE, 1, 1, run_motive},
    {"develop",
    "develop <input> --op <name> [--input-format <format>] [--meter <n/m>] [--key <key>] [--seed <n>] [--suggest <n>] [--by <n>] [--degree <n>] [--tonal] [--axis <pitch>] [--interval <n>] [--amount <n>] [--keep-contour] [--position <where>] [--length <n>] [--min <n>] [--drop <n>] [--keep-rhythm] [--ratio <n:m>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi]",
     "Make motives related to a motive by one operation",
    "Makes motives related to the one in <motive-input> (a MIDI file, a MusicXML file, a note list or a\n"
    "motive JSON file) and prints them best first, each as a note list with its recognizability score.\n"
    "\n"
    "Options by operation (the operation chooses at random from what is left open):\n"
    "  restate     --by <n> (semitones; steps with --tonal), --degree <n> (move the first note to scale\n"
    "              degree n, tonal), --tonal;  with none, the exact repetition and transpositions\n"
    "  invert      --axis <pitch> (D4 or a MIDI number; the first note by default), --tonal\n"
    "  retrograde  none\n"
    "  interval    --interval <n> (1-based; every one by default), --amount <n> (semitones, negative to\n"
    "              narrow), --keep-contour\n"
    "  fragment    --position <leading|trailing|middle>, --length <n>, --min <n>\n"
    "  liquidate   --drop <n>, --keep-rhythm, --min <n>\n"
    "  augment     --ratio <n:m> (2:1 by default)\n"
    "  diminish    --ratio <n:m> (1:2 by default)\n"
    "\n"
    "Notes on options:\n"
    "  --input-format is auto (default), midi, musicxml, notes, degrees, rhythm, contour or json. --seed is a whole\n"
    "  number, 0 to 4294967295 (default 0). --suggest is how many candidates to print, 1 to 16 (default 5).\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the input cannot be loaded or nothing can be\n"
    "developed as asked; 3 an output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz develop theme.txt --op restate --tonal --meter 4/4 --key \"D minor\" --suggest 3",
     FLAGS_DEVELOP, 1, 1, run_develop},
    {"variants",
    "variants --list-ops [--format <text|json>]\n"
    "variants <input> [--input-format <format>] [--form <name>] [--meter <n/m>] [--key <key>] [--pickup <beats>] [--harmony <text>] [--feature <l:a-b>]... [--role <role>] [--basic <file>] [--basic-format <format>] [--basic-pickup <beats>] [--basic-harmony <text>] [--chain <file>] [--each-op] [--distance <d>] [--function <name>]... [--count <n>] [--seed <n>] [--format <text|json>]",
     "Make variants of a motive, or list the operations",
    "Lists the variant operations of the registry, or makes variants of a motive: each variant applies a chain\n"
    "of up to 4 operations. A parameter the chain leaves open is drawn from the seed, so the same motive,\n"
    "chain and seed give the same variant, and another seed is a reroll.\n"
    "\n"
    "  <input>              a MIDI file, a MusicXML file, a note list or a motive JSON file\n"
    "\n"
    "Notes on options:\n"
    "  --count is how many variants, 1 to 16 (default 6); variant i uses seed + i. --seed: a whole number from 0,\n"
    "  default from the clock, and the seed is printed. --each-op makes one variant for every operation that\n"
    "  applies to the motive's form, in registry order, each a one-step chain with its open parameters drawn from\n"
    "  seed + the operation's position; an operation the motive refuses is listed as skipped, with the reason; not\n"
    "  with --chain, --count, --distance or --function. --chain is a JSON file {\"steps\": [{\"op\": \"invert\",\n"
    "  \"params\": {\"tonal\": true}}]}, the same chain for every variant (default: each variant gets its own random\n"
    "  chain). --distance: close draws 1-step chains and keeps remoteness at most 1/3, medium 2 steps and over 1/3\n"
    "  up to 2/3, far 3 or 4 steps and over 2/3. --function is repeatable (a variant that suits any of them); with\n"
    "  --function or --distance up to 4 chains are drawn for each variant asked for, so fewer than --count may come\n"
    "  back. --basic: the basic motive the family of forms comes from, read in the source's form, meter, key and\n"
    "  role; remoteness is measured from it (default: the source is the basic motive). --format json prints the\n"
    "  seed, the source and for each variant its seed, chain, kind, kept, changed, derivation, recognizability,\n"
    "  remoteness, functions and motive (with its analysis).\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument, or an unreadable chain file; 2 the input cannot\n"
    "be loaded or a step of the chain is refused.\n"
    "\n"
    "Example:\n"
    "  ursatz variants --list-ops\n"
    "  ursatz variants theme.txt --meter 4/4 --key \"D minor\" --count 4 --seed 7",
     FLAGS_VARIANTS, 0, 1, run_variants},
    {"recognize",
    "recognize <source> <target> [--input-format <format>] [--form <name>] [--meter <n/m>] [--key <key>] [--pickup <beats>] [--harmony <text>] [--feature <l:a-b>]... [--role <role>] [--target-format <format>] [--target-pickup <beats>] [--target-harmony <text>] [--target-feature <l:a-b>]... [--basic <file>] [--basic-format <format>] [--basic-pickup <beats>] [--basic-harmony <text>] [--depth <n>] [--format <text|json>]",
     "Name the operations that turn one motive into another",
    "Names the chain of registry operations (see variants --list-ops) that turns the source motive into the\n"
    "target, with every parameter set, how far the target is from the source, and what it kept and changed.\n"
    "The chain replayed with the variants command gives a motive equal to the target. Operations may differ from\n"
    "those that made the target: inverting then transposing equals transposing then inverting.\n"
    "\n"
    "  <source> <target>    each a MIDI file, a MusicXML file, a note list or a motive JSON file\n"
    "\n"
    "Notes on options:\n"
    "  --meter is required. --target-format defaults to auto; --target-pickup to none; the target has the source's\n"
    "  --role. --depth is the most operations in the chain, 1 to 4 (default 4). --format json prints {found, chain,\n"
    "  distance, remoteness, kept, changed, derivation}.\n"
    "\n"
    "Exit status: 0 the search ran, found or not; 1 usage, an invalid argument or an input that cannot be loaded.\n"
    "\n"
    "Example:\n"
    "  ursatz recognize theme.txt variant.txt --meter 4/4 --key \"D minor\" --format json",
     FLAGS_RECOGNIZE, 2, 2, run_recognize},
    {"random-motive",
    "random-motive --material <word> --meter <n/m> [--key <key>] [--form <name>] [--strategy <name>] [--role <role>] [--style <file>] [--style-without <category>]... [--count <n>] [--seed <n>] [--format <text|json>]",
     "Make random motives that keep the hard melodic rules",
    "Makes random motives that keep the hard melodic rules of standard counterpoint. Their length, pickup,\n"
    "number of notes, rhythm and range are not options: they are drawn from defaults in the Composer. The\n"
    "same options and seed give the same motives.\n"
    "\n"
    "Notes on options:\n"
    "  --material is major or minor (required; random draws it from the seed; --mode is the same option). --meter\n"
    "  is required; random draws 4/4, 3/4, 2/4 or 6/8 from the seed. --key defaults to C major or A minor and its\n"
    "  mode must be --material. --form is full (the default), rhythm or contour. --strategy is segments (the\n"
    "  default: a harmony plan and repeated segments), drawn (a rhythm drawn note by note) or blueprint (two bars\n"
    "  from the basic idea's expectations of a sentence). --style: draw 8\n"
    "  candidates for each motive and keep the closest. --style-without is repeatable, at most 8, and needs\n"
    "  --style. --count is 1 to 16 (default 1). --seed is a whole number from 0, default from the clock; the seed\n"
    "  is printed. --format json prints {\"seed\", \"motives\"}, each motive as the motive command's JSON document.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument, or an unreadable style file; 2 no motive could be made.\n"
    "\n"
    "Example:\n"
    "  ursatz random-motive --material minor --meter 3/4 --count 4 --seed 7",
     FLAGS_RANDOM_MOTIVE, 0, 0, run_random_motive},
    {"counterpoint",
    "counterpoint <line> --species <1-4> [--above | --below] --key <key> --meter <n/m> [--material <word>] [--seed <n>] [--suggest <n>] [--out <file>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format <text|json>]",
     "Write a counter-melody against a line",
    "Writes a counter-melody against <line> (a MIDI file, a MusicXML file, a note list or a motive JSON file)\n"
    "in the first, second, third or fourth species of strict counterpoint, and checks it with Ursatz's\n"
    "standard-counterpoint rules.\n"
    "\n"
    "Notes on options:\n"
    "  --species 1 is 1 note against 1, 2 is 2 against 1, 3 is 4 against 1 and 4 is syncopated half notes tied over\n"
    "  the barline; species 2 to 4 need a cantus firmus of one whole note a bar, three bars or more, and species 3\n"
    "  a meter of four beats. --below (the default): the line is the melody and the counter-melody sings below it;\n"
    "  --above: the line is the bass. --key is tonic and mode in one argument (\"D minor\"), or a tonic with\n"
    "  --material. --seed: the same seed gives the same counter-melody; the default is derived from the clock and\n"
    "  the seed used is printed. --suggest is 1 to 16; with files, number k goes to <name>-k.<extension>. --midi\n"
    "  and --musicxml write the two parts.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied (the line cannot be\n"
    "read or is not one the species can be written against, or no counter-melody holds the rules); 3 an output\n"
    "file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz counterpoint cantus.txt --species 2 --above --key \"D minor\" --meter 4/4 --seed 3 --midi out.mid",
     FLAGS_COUNTERPOINT, 1, 1, run_counterpoint},
    {"grow",
    "grow <input> --into <form> --key <key> --meter <n/m> [--hybrid <b>+<e>] [--material <word>] [--bars <n>] [--cadence <type>] [--first-cadence <type>] [--accompaniment <pattern>] [--accompaniment-motive <file>] [--accompaniment-format <format>] [--accompaniment-pickup <beats>] [--texture <name>] [--voicing] [--check] [--style <file>] [--style-weight <n/d>] [--style-without <category>]... [--blend <file>] [--blend-weight <n/d>] [--blend-weights <list>] [--harmony <text>] [--feature <l:a-b>]... [--role <role>] [--seed <n>] [--suggest <n>] [--out <file>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--explain <file>] [--format <text|json>]\n"
    "grow --from-corpus <dir> --like <file> --into <form> --key <key> --meter <n/m> [options as above]",
     "Grow a motive into a sentence, a period or a hybrid theme",
    "Grows a theme from the motive in <motive-input> (a MIDI file, a MusicXML file, a note list or a motive\n"
    "JSON file): its structure, a harmony, a bass line and a melody, ending in the cadence asked for. With\n"
    "--from-corpus and --like the motive is not given but taken from a corpus.\n"
    "\n"
    "The form heard in the result (by Ursatz's standard-form) is printed with it; a theme need not be heard as\n"
    "the form it was grown as, and that is reported, not an error.\n"
    "\n"
    "Notes on options:\n"
    "  --into is sentence (presentation + continuation), period (antecedent + consequent) or hybrid (any other\n"
    "  pairing, named with --hybrid). --hybrid names the beginning (presentation, antecedent or cbi, a compound\n"
    "  basic idea) and the ending (continuation, consequent or cadential), for example antecedent+continuation.\n"
    "  --key takes tonic and mode in one argument (\"D minor\"; any case). --material is major, minor or style: it\n"
    "  replaces the material of --key (the key may then be a tonic alone); style takes the style's most common mode\n"
    "  and needs --style with a mode category. --meter is 2/4, 3/4, 4/4 or 6/8 and the motive must fit in one bar\n"
    "  of it. --bars is 3 to 16 (default 8; 4 uses a one-bar basic idea, others a chosen construction). --cadence\n"
    "  is PAC (default), IAC or HC; --first-cadence is HC (default) or IAC, the weaker cadence an antecedent ends\n"
    "  in. --accompaniment is block, arpeggiated, alberti (not in 6/8), oom-pah or any (the pattern is chosen from\n"
    "  the seed). --accompaniment-motive restates a saved motive on each chord (implies --accompaniment motive;\n"
    "  read in --meter, at most one bar long); --accompaniment-format and --accompaniment-pickup read its file.\n"
    "  --texture is \"homophony\", \"homorhythm\" (block chords only), \"melody and accompaniment\" or \"chorale\" (four\n"
    "  parts in a note a chord); it implies --accompaniment any, except for chorale. --voicing keeps the grown\n"
    "  melody as the soprano and is not used with --accompaniment. --style prints the Analyzer's distance from the\n"
    "  style; --style-weight is 1/1 by default and needs --style; --style-without is repeatable, at most 8, and\n"
    "  needs --style; --blend needs --style; --blend-weight is 0 to 1 (default 1/2) and needs --blend; --blend-\n"
    "  weights names harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d> (one not named takes --blend-weight).\n"
    "  --from-corpus is the output folder of the corpus command: it needs --like, and the passage it names, the\n"
    "  measures and the score are printed and kept in the plan; exit 2 when no passage scores 1/2 or more. --like:\n"
    "  with --from-corpus, the first bar of a similar passage becomes the motive, in --key. --feature is\n"
    "  repeatable, up to 8. --role is melodic (default) or accompaniment. --seed: the same seed gives the same\n"
    "  output; the default is derived from the clock; the seed used is always printed (if a seed gets stuck the\n"
    "  next seeds are tried, up to 16). --suggest is 1 to 16; with files, theme k goes to <name>-k.<extension>.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied;\n"
    "3 an output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz grow theme.txt --into sentence --key \"D minor\" --meter 4/4 --seed 7 --out theme.json\n"
    "  ursatz grow theme.txt --into hybrid --hybrid antecedent+continuation --key \"G major\" --meter 3/4",
     FLAGS_GROW, 0, 1, run_grow},
    {"theme",
    "theme <request.json> [--seed <n>] [--suggest <n>] [--check] [--out <file>] [--layers <dir>] [--midi <file>] [--musicxml <file>] [--plain-midi] [--explain <file>] [--format <text|json>]",
     "Grow a theme from the motives of a request",
    "Grows a theme from the motives of a request (JSON, schema ursatz-theme-request, version 2): up to 16 motives, which the\n"
    "slots of the theme (basic, contrasting, continuation and cadential) and its single ideas name (a motive may be\n"
    "in several), and the brief: key, mode, meter, cadences, an accompaniment or a voicing, and a style that gives\n"
    "the tempo, the mode and the note density. The forms of the ideas are invented. See\n"
    "docs/theme-request.md.\n"
    "\n"
    "Notes on options:\n"
    "  --seed: the same seed gives the same output; the default is from the clock. --suggest is 1 to 16; with\n"
    "  files, theme k goes to <name>-k.<extension>. --layers writes the first theme's three layers: plan.json,\n"
    "  realization.json and readings/composer.json. --midi and --musicxml write the melody, the bass and the\n"
    "  accompaniment (the tempo of the brief). --format json prints the theme with the slots and the brief.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid request; 2 the request cannot be satisfied; 3 an output file\n"
    "could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz theme request.json --seed 7 --out theme.json --midi theme.mid",
     FLAGS_THEME, 1, 1, run_theme},
    {"piece",
    "piece <request.json> [--seed <n>] [--check] [--out <file>] [--layers <dir>] [--ending-out <file>] [--midi <file>] [--musicxml <file>] [--ending-midi <file>] [--ending-musicxml <file>] [--plain-midi] [--explain <file>] [--format <text|json>]\n"
    "piece --from <loop-plan.json> --request <request.json> --regenerate <id|none> [--lane <lane>] [--seed <n>] [--check] [--out <file>] [--layers <dir>] [--ending-out <file>] [--midi <file>] [--musicxml <file>] [--ending-midi <file>] [--ending-musicxml <file>] [--plain-midi] [--explain <file>] [--format <text|json>]\n"
    "piece --from <loop-plan.json> --request <request.json> --variations <id> --lane <lane> --count <n> --out-dir <dir> [--seed <n>] [--check] [--plain-midi] [--format <text|json>]",
     "Compose a piece from a request, or work on one section of it",
    "Composes a looping game track from a request (JSON, schema ursatz-piece-request): an intro, a body, a turnaround and an\n"
    "ending, each a list of sections of any length in beats with a pickup, a cadence and a fill (nothing, a motive,\n"
    "a composed theme or a progression), in a key and a meter, with an accompaniment or a voicing and a style. Two\n"
    "versions are made: the loop (intro once, then the body and the turnaround, which lead back to the body's start)\n"
    "and the ending (the intro and the body as the loop has them, then the ending). See docs/piece-request.md.\n"
    "A request of schema_version 2 names another built-in form in \"form\" (rounded-binary, ternary, variations, ...)\n"
    "and takes its sections from the form's size (\"bars\", \"count\", \"modulate\"); it has the one version.\n"
    "\n"
    "Forms: with a request file the command composes the piece; with --from and --request it works on a loop plan\n"
    "the command wrote (--request is the request it was made from): --regenerate writes the section with that id\n"
    "again (the lane given, or all of it), the rest locked; the seam is written again when the section holds the\n"
    "body's first bar, and the ending version is always built again from the new loop; none: only the ending\n"
    "version; a section filled with a theme needs --lane. --variations makes --count different versions of the\n"
    "section (--lane needed; harmony over a given melody is a harmonization), each written to --out-dir as\n"
    "variation-<i>.json, .mid and .musicxml (the loop version) with result.json: the seeds, scores, seam and\n"
    "cadences.\n"
    "\n"
    "Notes on options:\n"
    "  --lane is harmony, bass, melody, inner or all (default all). --count is 1 to 8. --seed: the same seed gives\n"
    "  the same output; the default is from the clock. --check analyzes both versions with the Ursatz Analyzer.\n"
    "  --out writes the first version's plan (the loop's); --ending-out the ending version's plan (a form with an\n"
    "  ending version only). --midi writes the first version with loopStart and loopEnd markers for a loop\n"
    "  (--plain-midi keeps them). --layers writes the first version's three layers. --explain writes why each\n"
    "  candidate was chosen or rejected, in both versions.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid request; 2 the request cannot be satisfied; 3 an output file\n"
    "could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz piece request.json --seed 7 --out loop.json --midi loop.mid",
     FLAGS_PIECE, 0, 1, run_piece},
    {"plan",
    "plan --form <name> --key <key> --meter <n/m> --out <file> [--material <word>] [--bars <n>] [--count <n>] [--modulate] [--motive <file> | --from-corpus <dir> --like <file>] [--like-progression <numerals>]... [--accompaniment <pattern>] [--texture <name>] [--given-melody <file>]... [--given-progression <numerals>]... [--given-bass <file>]... [--at <L>[:<bar>]]... [--cells <n>]... [--seed <n>] [--style <file>] [--style-weight <n/d>] [--style-without <category>]... [--blend <file>] [--blend-weight <n/d>] [--blend-weights <list>] [--format <text|json>]",
     "Write the plan of a whole short piece from a form template",
    "Writes the plan of a whole short piece from a form template: its sections, their keys and cadences, the\n"
    "sections that return an earlier one, and the motive. Nothing is composed yet; 'ursatz fill'\n"
    "realizes the plan.\n"
    "\n"
    "Notes on options:\n"
    "  --form is binary, rounded-binary, ternary, variations, loop or layered; with --given-* material it may be\n"
    "  left out, and the form is a binary form of 16 bars. --key is the tonic (\"D\", \"F#\", \"Bb\") or tonic and mode\n"
    "  in one argument (\"D minor\"). --meter is 2/4, 3/4, 4/4 or 6/8. --material is major or minor when --key is a\n"
    "  tonic only (default major; --mode is the same option). --bars is the size of the piece in bars (the sizes of\n"
    "  each form are listed when one is refused); --count is the variations (variations) or the layers (layered),\n"
    "  instead of --bars. --modulate plans the keys and cadences across the sections from the seed (otherwise every\n"
    "  section is in the home key and the cadences are the form's). --motive: without it one is invented when the\n"
    "  plan is filled. --from-corpus is the output folder of the corpus command, for --like and --like-progression;\n"
    "  the passage used, its measures and its score are printed and kept in the plan; exit 2 when no passage scores\n"
    "  1/2 or more. --like is the motive: the first bar of a passage of the corpus similar to this MIDI or MusicXML\n"
    "  passage (picked by the seed among the best), in --key, instead of --motive. --like-progression places a\n"
    "  progression like --given-progression, with the chords of the passage of the corpus that best matches the\n"
    "  numerals; it goes with --at (and --cells). --given-melody, --given-progression and --given-bass place\n"
    "  material in the piece (up to 8 in all); fill never changes it: a melody is a MIDI file, a MusicXML file, a\n"
    "  note list or a motive JSON file of any length, a bass line has no rests, a progression is Roman numerals,\n"
    "  one chord each (\"I IV V I\"). --at after one of them names where it goes: the section's letter (\"B\", \"A'\")\n"
    "  and, from 1, the bar of it the material starts in (default 1; without --form the default is B:1). --cells is\n"
    "  the cells each chord of the --given-progression before it lasts (default: a bar). --accompaniment is block,\n"
    "  arpeggiated, alberti (not in 6/8), oom-pah or any (a layered form has one in any case). --texture implies\n"
    "  --accompaniment any. --seed: the same seed gives the same plan; the default is from the clock. --style: the\n"
    "  path is kept in the plan as given, so fill finds the file from where it is run. --style-weight is 1/1 by\n"
    "  default and needs --style; --blend needs --style and is kept in the plan; --blend-weight is 0 to 1 (default\n"
    "  1/2) and needs --blend; --blend-weights names harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument (an unknown form, a size the form does not have, a bad\n"
    "key or meter); 2 the request cannot be satisfied (an unreadable motive, a motive too long for a bar);\n"
    "3 the plan could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz plan --form ternary --key \"A\" --material minor --meter 3/4 --out piece.json\n"
    "  ursatz plan --form binary --key \"D minor\" --meter 4/4 --bars 8 --motive m.txt --out piece.json",
     FLAGS_PLAN, 0, 0, run_plan},
    {"fill",
    "fill <plan.json> [--seed <n>] [--check] [--candidates <n>] [--style <file>] [--style-weight <n/d>] [--style-without <category>]... [--blend <file>] [--blend-weight <n/d>] [--blend-weights <list>] [--out <file>] [--layers <dir>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--record <file>] [--explain <file>] [--format <text|json>]",
     "Realize a plan: compose every open theme section",
    "Realizes a plan: composes every open theme section of <plan.json> (a plan from 'ursatz plan' or\n"
    "'grow --out') in stages, structure, motives, harmony, bass, melody, texture and the returns, and writes the\n"
    "piece. A plan with no open theme section (one from 'grow --out' is written already) is only exported.\n"
    "\n"
    "The form and cadences heard in each section, and the thematic returns heard, are printed with the result; a\n"
    "section need not be heard as the form it was planned as, and that is reported, not an error. When no seed\n"
    "completes the plan, the targets left open are printed with the reason each is stuck, and nothing is written.\n"
    "\n"
    "Notes on options:\n"
    "  --seed: the same plan and seed give the same output; the default is from the clock; the seed used is always\n"
    "  printed (if a seed gets stuck the next seeds are tried, up to 16, and the one that worked is printed).\n"
    "  --check rejects a run whose analysis contradicts the plan (a planned cadence or the key not heard, a hard\n"
    "  voice-leading fault). --candidates composes n candidates (1 to 32, seeds from --seed) and writes the best,\n"
    "  ranked by agreement with the Analyzer, then score, then the lower seed; every candidate is listed. --style\n"
    "  overrides the plan's own style; its path is kept in the plan as given. --style-without overrides the plan's\n"
    "  list (repeatable, at most 8). --blend overrides the plan's blend; --style without --blend drops the plan's\n"
    "  blend. --out writes the realized plan as JSON. --layers writes the three layers: plan.json, realization.json\n"
    "  and readings/composer.json. --midi writes the melody, the bass and the accompaniment. --record writes what\n"
    "  the run did: the seeds, the candidates and the check. --explain writes why each candidate was chosen or\n"
    "  rejected: the chosen candidate, its runners-up and the rejected ones with their reasons per target, the\n"
    "  analyzer check and the form heard per section, and the runs thrown away; JSON when the file ends in .json,\n"
    "  else text.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read or cannot be completed;\n"
    "3 an output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz fill piece.json --seed 7 --midi piece.mid --musicxml piece.xml",
     FLAGS_FILL, 1, 1, run_fill},
    {"lock",
    "lock <plan.json> (--node <id> | --section <letter>) [--lane <lane>] --out <file> [--format <text|json>]",
     "Lock material of a plan as given",
    "Locks material of a plan as Given: a node and everything under it (a section, a phrase, an idea), or, with\n"
    "--lane, one lane of it. Nothing composes over Given material, and it is never regenerated. Locking is for\n"
    "good in the plan; to unlock, go back to an earlier plan file.\n"
    "\n"
    "Only written material can be locked: a node with a lane span still open is refused.\n"
    "\n"
    "Notes on options:\n"
    "  --node is the node's id in the plan file; --section is the section with that form letter (\"A\", \"B\", \"A'\")\n"
    "  instead of --node; --lane is harmony, bass, melody or inner: only that lane of the node and of what is under\n"
    "  it. --out is where the plan is written.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read, the node is not found or has\n"
    "open spans; 3 the output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz lock piece.json --section A --out piece-locked.json",
     FLAGS_LOCK, 1, 1, run_lock},
    {"regenerate",
    "regenerate <plan.json> (--node <id> | --section <letter>) [--lane <lane>] --out <file> [--seed <n>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format <text|json>]\n"
    "regenerate item:<id> --section <id> [--lane <lane>] [--seed <n>] [--version <n>] [--base <n>] [--no-wait]",
     "Write one node of a filled plan again",
    "Writes one node of a filled plan again and nothing but what depends on it. A section is built again whole\n"
    "(its form and its returns too); a phrase or an idea keeps its shape and has its lanes written again; with\n"
    "--lane, that lane of the node and what is written from it (the bass and melody over new chords, the next\n"
    "chord or idea that joins them). Given material is never changed.\n"
    "\n"
    "What was reopened, the seed, the form and cadences heard in each section written again, and the returns are\n"
    "printed. The ids in a plan file run from 1: a plan written after a section was built again numbers its nodes\n"
    "again, so read them from the file you are using.\n"
    "\n"
    "Notes on options:\n"
    "  --node is the node's id in the plan file; --section is the section with that form letter instead of --node;\n"
    "  --lane is harmony, bass, melody or inner. --seed: the same plan, node and seed give the same output; the\n"
    "  default is from the clock; the seed used is always printed (if a seed gets stuck the next seeds are tried,\n"
    "  up to 16).\n"
    "\n"
    "With an item address (a piece of the data folder) the section of --version (default: the current version) is\n"
    "written again as the next version of the item, as a job the command waits for; --base is the version the\n"
    "command is made against (default: the current one), --no-wait prints the job's id instead of waiting. The\n"
    "item is named after --data: ursatz --data <dir> regenerate item:<id> --section B.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read, the node is not found, is\n"
    "Given or cannot be regenerated, or no seed completes the plan, or the job failed; 3 an output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz regenerate piece.json --section B --seed 3 --out piece-2.json --midi piece-2.mid\n"
    "  ursatz --data ~/ursatz-data regenerate item:itm-3f2a --section B --seed 3",
     FLAGS_REGENERATE, 1, 1, run_regenerate},
    {"variations",
    "variations <plan.json> (--node <id> | --section <letter>) [--lane <lane>] --count <n> --out-dir <dir> [--seed <n>] [--write-midi] [--write-musicxml] [--tempo <bpm>] [--plain-midi] [--format <text|json>]",
     "Write several versions of one node of a plan",
    "Writes several distinct versions of one node of a filled plan. The node is written again (as regenerate\n"
    "writes it) on a copy of the plan for the seeds N, N+1, ..., at most four times --count of them; a version\n"
    "is kept when it sounds different from the plan and from the versions kept before. A seed that gets stuck\n"
    "is skipped. The plan file is not changed. Given material is never changed.\n"
    "\n"
    "Notes on options:\n"
    "  --node is the node's id in the plan file; --section is the section with that form letter instead of --node;\n"
    "  --lane is harmony, bass, melody or inner. --count is how many variations, 1 to 8. --seed is a whole number\n"
    "  from 0 for the first seed tried (default from the clock); the seed of every version is printed. --out-dir is\n"
    "  where variation-<i>.json (the plan) is written, i from 1; the directory is created; --write-midi and\n"
    "  --write-musicxml also write variation-<i>.mid and variation-<i>.musicxml.\n"
    "\n"
    "Exit status: 0 at least one version was kept; 1 usage or an invalid argument; 2 none was kept, the plan\n"
    "cannot be read, the node is not found, is Given or cannot be regenerated (nothing is written); 3 an output\n"
    "file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz variations piece.json --section B --count 4 --seed 3 --out-dir variations --write-midi",
     FLAGS_VARIATIONS, 1, 1, run_variations},
    {"harmonize",
    "harmonize <plan.json> --section <letter> --count <n> --out-dir <dir> [--seed <n>] [--write-midi] [--write-musicxml] [--tempo <bpm>] [--plain-midi] [--format <text|json>]",
     "Write several harmonizations of a melody that is given",
    "Writes several distinct harmonizations of a melody that is given. The whole melody of the section must be\n"
    "Given (a plan made with --given-melody for every bar of the section); its harmony, and what is written from\n"
    "the harmony (the bass and the voices between), is written again as variations does, on a copy of the plan\n"
    "for the seeds N, N+1, ..., at most four times --count of them. A version is kept when it sounds different\n"
    "from the plan and from the versions kept before. Every melody note is kept. The plan file is not changed.\n"
    "\n"
    "Notes on options:\n"
    "  --section is the section with that form letter (\"A\", \"B\", \"A'\") whose melody is given. --count is how many\n"
    "  versions to keep, 1 to 16. --seed is a whole number from 0 for the first seed tried (default from the\n"
    "  clock). --out-dir is where variation-<i>.json (the plan) is written, i from 1; the directory is created;\n"
    "  --write-midi and --write-musicxml also write variation-<i>.mid and variation-<i>.musicxml.\n"
    "\n"
    "Exit status: 0 at least one version was kept; 1 usage or an invalid argument; 2 none was kept, the plan\n"
    "cannot be read, the section is not found or has no given melody (nothing is written); 3 an output file\n"
    "could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz harmonize piece.json --section A --count 4 --seed 3 --out-dir harmonizations --write-midi",
     FLAGS_HARMONIZE, 1, 1, run_harmonize},
    {"render",
    "render <plan.json> [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi]\n"
    "render item:<id>[@v<n>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi]",
     "Render a plan to a MIDI or MusicXML file",
    "Renders a plan file (a plan from 'ursatz plan', 'fill', 'grow --out' and the like) to MIDI, MusicXML or both:\n"
    "at least one of --midi and --musicxml is needed. --tempo is a whole number or a fraction like 195/2.\n"
    "With an item address, the plan of that version of a theme or piece of the data folder (the current version\n"
    "when the address has none); a motif has no plan.\n"
    "\n"
    "Exit status: 0 success; 1 usage or an invalid argument, or a plan that cannot be read or rendered; 2 an item\n"
    "version without a plan.\n"
    "\n"
    "Example:\n"
    "  ursatz render piece.json --midi piece.mid --musicxml piece.xml --tempo 96\n"
    "  ursatz --data ~/ursatz-data render item:itm-3f2a@v2 --midi piece.mid",
     FLAGS_RENDER, 1, 1, run_render},
    {"knowledge",
    "knowledge find <words> [--kind <kind>] | show <id> [--format <text|json>] | check <file>",
     "Find, show or check knowledge entries",
    "Finds, shows or checks the entries of the knowledge (R-765): the materials, chord types, vocabulary, schemas, patterns and the rest that the Composer and the Analyzer read, built into the program and, with a data folder, the files of its knowledge folder (user.* entries). Name the data folder before the subcommand (ursatz --data <dir> knowledge find ...) or with URSATZ_DATA; without one only the built-in entries are read.\n"
    "\n"
    "  find <words>   the entries whose id, name or an alias holds every word (case ignored), one line each: <id>  <kind>/<subtype>  <name>; at most 50. Quote several words. --kind limits the kind.\n"
    "  show <id>      the entry as Ursatz Text (the default) or, with --format json, as loaded.\n"
    "  check <file>   whether the knowledge document in <file> loads beside the built-in entries and the data folder's files: prints ok: <n> entries, or the loader's refusal (the file, the entry id and the field).\n"
    "\n"
    "Exit status: 0 success; 1 usage, or an id or kind no entry has; 2 a document the loader refuses, or any other failure. See cli/docs/cli-knowledge.md.\n"
    "\n"
    "Example:\n"
    "  ursatz knowledge find mario\n"
    "  ursatz knowledge find shuttle --kind schema\n"
    "  ursatz knowledge show omt.schema.aeolian-cadence --format json\n"
    "  ursatz knowledge check my-voicings.json",
     FLAGS_KNOWLEDGE, 2, 2, run_knowledge},
    {"critique",
    "critique <file>... [--settings <file>] [--format text|json]\n"
    "critique --rated <file> [--settings <file>] [--format text|json]\n"
    "critique item:<id>[@v<n>] [--settings <file>] [--note <text>] [--format text|json]\n"
    "critique --verdict <obs-id> agree|disagree item:<id> [--note <text>]",
     "Judge candidates with the critic, in her words",
    "Judges candidate files with the critic (docs/critic.md): ten evaluators of what she has said about a melody give each candidate a\n"
    "score (minus the weighted sum of their penalties), and the Composer's hearing of a theme against its plan gives observations in plain\n"
    "words. A file is a listen sample (its motive, or the plan of its first theme: make themes with listen --plans), a plan or a motive\n"
    "document. The files of one call are one set, which the critic compares (one_rhythm). See cli/docs/cli-critique.md.\n"
    "\n"
    "--rated reads a rated set (ursatz-rated-set 1: pairs of files with her ratings and the set each belongs to, paths relative to the file),\n"
    "judges each set together and prints, for each pair, her ratings, the critic's scores and whether the critic and today's ranking (a\n"
    "theme's own score) chose the one she rated higher: match, miss or tie, then the totals. Pairs with equal ratings are not counted.\n"
    "With an item address (a motif or a theme of the data folder) the critic judges that version, as @v<n> or the current one, and the\n"
    "critique is kept with an id for each observation (crt-... and obs-...). --verdict records whether you agree with the critic about one\n"
    "observation (vrd-...). The critic's settings are the item's resolved settings unless --settings is given.\n"
    "Notes on options:\n"
    "  --settings is a preferences document (docs/preferences.md); its critic group sets the weights and the evaluators that count the other\n"
    "  way. --format is text (the default) or json (the Composer's response; with --rated the text is always printed).\n"
    "\n"
    "Text: for each candidate '<file>  score <n/d>', then each observation '  bar <b> beat <x>  <rule>  <message>' ('-' for no place).\n"
    "Exit status: 0 it ran; 1 usage, an invalid option or an unreadable file; 2 the Composer refused a candidate.\n"
    "\n"
    "Example:\n"
    "  ursatz critique 01.json 02.json 03.json\n"
    "  ursatz critique --rated rated.json\n"
    "  ursatz critique item:itm-3f2a@v2\n"
    "  ursatz critique --verdict obs-1c9e agree item:itm-3f2a",
     FLAGS_CRITIQUE, 0, 16, run_critique},
    {"listen",
    "listen --what <motifs|themes> --out-dir <dir> [--count <n>] [--seed <n>] [--settings <file>] [--material <word>] [--meter <n/m>] [--style <file>] [--blend <file>] [--blend-weight <n/d>] [--blend-weights <list>] [--tempo <bpm>] [--strategy <name>] [--into <form>] [--development <name>] [--bars <n>] [--check] [--plans] [--preset <name>] [--motif <file>] [--theme-request <file>] [--blueprint <file>] [--session <ses-id> --round <n> --batch <name> [--taste <name>]]\n"
    "listen --print-settings",
     "Make batches of motifs or themes from fixed seeds for listening",
    "Makes a batch of motifs or themes from fixed seeds, with a settings file, and writes them as files a listening\n"
    "session plays and reads. Sample i (from 0) uses the seed --seed + i, so the same options, settings and seed\n"
    "give the same files. See cli/docs/cli-listen.md.\n"
    "\n"
    "With --session <ses-id> (and then --round and --batch) the settings of each sample are resolved by the workshop of the data folder (the taste\n"
    "profile of --taste or of the session, the scopes, and for the share explore.share of the samples the settings moved away from the taste) and the\n"
    "batch is recorded in the listening session with the files' sha256 (ratings.batch). rate records what you thought of it. --round, --batch and\n"
    "--taste are refused without --session.\n"
    "\n"
    "Files, i from 01: <i>.mid, <i>.musicxml, <i>.json (the API response) and one line per sample in listen.jsonl.\n"
    "Notes on options:\n"
    "  --what and --out-dir are required; --out-dir is created if missing. --count is how many samples, 1 to 64\n"
    "  (default 10). --seed is a whole number from 0 for the first sample (default 1). --settings is a preferences\n"
    "  document (docs/preferences.md); absent: none is sent. --material is major (default), minor or a church mode (--mode is the\n"
    "  same option). --meter is beats over a beat unit (default 4/4). --style: draw 8 candidates for each motif and\n"
    "  keep the closest; themes prefer it too. --blend, --blend-weight and --blend-weights are for themes. --tempo\n"
    "  is quarter notes per minute from 20 to 400. --strategy is segments (default), drawn or blueprint, for motifs and for\n"
    "  the generated motif of a theme.\n"
    "  Themes only: --into is sentence (default) or period; --development is melody, balanced, theme or blueprint (default, where the theme fits it);\n"
    "  --bars is 4 or 8 (default 8); --check passes over a seed whose analysis contradicts the plan; --motif is the\n"
    "  motif of every theme, anything the motive command reads (default: a generated motif for each sample);\n"
    "  --theme-request is a theme request (docs/theme-request.md) with the sample's seed, instead of a motif.\n"
    "  --blueprint is a theme request with a blueprint (docs/blueprints.md), as JSON or as Ursatz Text, made as --theme-request is; it\n"
    "  cannot be given with --motif or --theme-request.\n"
    "  --plans puts each theme's plan in its JSON (themes[0].plan), for ursatz critique; the files' notes stay the same.\n"
    "  --preset names a preset of the Composer (ff7 or hollow-knight): the settings of her listening and, for themes, the\n"
    "  accompaniment that goes with them; --settings and the session's settings apply over it. It applies to motifs and themes.\n"
    "\n"
    "Exit status: 0 at least one sample was written; 1 usage, an invalid option or an unreadable file; 2 none could\n"
    "be composed; 3 an output file could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz listen --what motifs --count 10 --seed 1 --settings mine.json --out-dir round-1",
     FLAGS_LISTEN, 0, 0, run_listen},
    {"analyze",
    "analyze <input> [--input-format <format>] [--meter <n/m>] [--value-scale <n or n/d>] [--pickup <beats>] [--layers <dir>] [--model-plan <file>]\n"
    "analyze <input> --view reading [--measures <first>-<last>] [--title <text>] [--input-format <format>] [--meter <n/m>] [--value-scale <n or n/d>] [--pickup <beats>]",
     "Analyze a MIDI or MusicXML file into an analysis document, or print its text export",
    "Analyzes a MIDI or MusicXML file with the Ursatz Analyzer and writes the analysis document, <stem>.analysis.json,\n"
    "next to the input (a JSON document: the form, phrases, cadences, harmony, keys and motives it hears). With --view reading\n"
    "it prints the measure-addressed text export of the piece instead, the text a person can paste into a conversation to\n"
    "say \"measure 27 is flagged as X, I read it as Y\", and writes nothing.\n"
    "\n"
    "--layers <dir> also writes the layers of the analysis: realization.json and readings/analyzer.json. --model-plan <file>\n"
    "also writes the minimal plan of the Analyzer's reading (its sections, keys and chords). Neither goes with --view reading.\n"
    "\n"
    "Notes on options:\n"
    "  --input-format is auto (the default), midi or musicxml. --meter, --value-scale and --pickup are a notation reading\n"
    "  for a file whose meter or note values are not what it means: --meter 3/4, --value-scale 2 or 1/2, --pickup 1/4 (the\n"
    "  pickup must be shorter than one measure of the meter). --measures and --title need --view reading: --measures 12-16\n"
    "  exports those measure numbers as written in the score (a pickup measure is number 0).\n"
    "\n"
    "Exit status: 0 success; 1 usage, an input that cannot be read, a notation reading that cannot be applied or an analysis\n"
    "that failed; 2 an output file (the layers, the model plan) could not be written.\n"
    "\n"
    "Example:\n"
    "  ursatz analyze song.mid\n"
    "  ursatz analyze song.mid --view reading --measures 1-4",
     FLAGS_ANALYZE, 1, 1, run_analyze},
    {"corpus",
    "corpus <dir> [--out <dir>] [--resume]",
     "Analyze every piece of a folder into a corpus of analysis documents",
    "Analyzes every MIDI and MusicXML file of <dir> (found recursively, in path order) with the Ursatz Analyzer and writes one analysis document for each, and the corpus index, into the output folder: <dir>/.ursatz-corpus unless --out names another. A piece that cannot be read or analyzed is counted as failed; that does not fail the run. Each piece is printed as analyzed, skipped or failed with its path (and, for a failed one, why), the importer's messages and warnings on stderr, then the three counts.\n"
    "\n"
    "With --resume a piece whose file did not change since the last run is skipped.\n"
    "\n"
    "Exit status: 0 the run finished (also with failed pieces); 1 usage or an unknown option; 2 the run could not be made (a folder that does not read, an output that cannot be written).\n"
    "\n"
    "Example:\n"
    "  ursatz corpus sonatinas --out sonatinas-analyzed\n"
    "  ursatz corpus sonatinas --out sonatinas-analyzed --resume",
     FLAGS_CORPUS, 1, 1, run_corpus},
    {"style-profile",
    "style-profile <corpus output> --name <name> [--out <file>]",
     "Count the styles of a corpus into a style model file",
    "Reads the output folder of a corpus run (its corpus-index.json and the documents) and writes a style model: the harmony, rhythm, melody and form the corpus uses, counted. The model is written to --out, by default <corpus output>/style-model.json, under the name given. The style commands (grow, plan, fill, listen, random-motive) read it with --style.\n"
    "\n"
    "A document another version of the Analyzer wrote is used as it is, and a warning says how many there are.\n"
    "\n"
    "Exit status: 0 success; 1 usage (the name is required); 2 the corpus cannot be read or the model cannot be written.\n"
    "\n"
    "Example:\n"
    "  ursatz style-profile sonatinas-analyzed --name Sonatinas --out sonatinas.style.json",
     FLAGS_STYLE_PROFILE, 1, 1, run_style_profile},
    {"status",
    "status [--format <text|json>]",
     "Say how the data folder is reached and what runs there",
    "Says how this command reaches the data folder and what runs there: the folder, whether this command opened it (and with how many job workers) or another Ursatz process serves it (its process id and port), and the versions of the Composer and the Analyzer. A folder the last process stopped in the middle of a change is recovered when it opens, and the recovery line says what was done.\n"
    "\n"
    "The data folder is named before the subcommand: ursatz --data <dir> status (or the environment variable URSATZ_DATA). The folder is locked to one process: when a process holds it and serves HTTP (the GUI backend), this command connects to it; when it serves none, this command fails and says so.\n"
    "\n"
    "Exit status: 0 success; 1 usage (no data folder named); 2 the folder cannot be opened or reached.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data status\n"
    "  ursatz --data ~/ursatz-data status --format json",
     FLAGS_STATUS, 0, 0, run_status},
    {"query",
    "query <name> [--params <file>] [--out <file>] [--format <text|json>]",
     "Call a query of the workshop on the data folder",
    "Calls a query of the workshop by its name (project.list, item.get, job.get, ...) on the data folder, opened by this command or served by another Ursatz process, and prints the answer as JSON: indented by default, on one line with --format json. A query changes nothing.\n"
    "\n"
    "--params names a JSON file holding the params object ({\"id\": \"...\"}), or - for standard input; without it the params are {}. A query that answers a file (an export, a playback file) needs --out <file>, which receives the file.\n"
    "\n"
    "Exit status: 0 success; 1 usage, params that are not a JSON object, or a query the workshop refuses as a request (its message, and its path when it names one); 2 any other failure.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data query project.list\n"
    "  echo '{\"id\": \"item-1\"}' | ursatz --data ~/ursatz-data query item.get --params -",
     FLAGS_QUERY, 1, 1, run_query},
    {"show",
    "show <address|file> [--view <view>] [--measures <first>-<last>]",
     "Show what the data folder or a layer file holds, as text views or JSON",
    "Prints what the workshop holds under an address, or what a layer file (a plan, a realization, a reading) holds, as a view in Ursatz Text or as JSON. An address is <kind>:<id> with the kind item, piece, project, subject, collection, style, links or job, item:<id>@v<n> for one version of an item, and /<section> after an item or a piece to look at one section (item:<id>@v<n>/<section>, piece:<id>/<section>).\n"
    "\n"
    "  item:<id>        the current version of the item        (item.view, or item.get for json)\n"
    "  item:<id>@v<n>   one version of the item                (item.view, or item.version for json)\n"
    "  piece:<id>       a Library piece                        (library.view, or library.piece for json)\n"
    "  <file>           a layer file                           (the tool text)\n"
    "  project:<id>, subject:<id>, collection:<id>, style:<id>, links:<id>, job:<id>   json only         (project.get and the like)\n"
    "\n"
    "--view is summary (the default for an item, a piece and a file), structure, harmony, line:<name>, part:<name>, reading, full (the lossless text of every layer) or json. --measures limits the view to bars <first>-<last>. The other kinds of address show only --view json. The data folder is named before the subcommand: ursatz --data <dir> show item:<id>.\n"
    "\n"
    "Exit status: 0 success; 1 usage (a text that is no address, a view the address does not give), or a view or an address the workshop refuses; 2 the folder cannot be opened, or any other failure.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data show item:itm-3f2a\n"
    "  ursatz --data ~/ursatz-data show item:itm-3f2a@v2 --view harmony --measures 1-8\n"
    "  ursatz --data ~/ursatz-data show item:itm-3f2a@v2/A --view structure\n"
    "  ursatz show plan.json --view line:melody",
     FLAGS_SHOW, 1, 1, run_show},
    {"history",
    "history [<address>] [--limit <n>] [--format <text|json>]",
     "List what the commands changed in the data folder, newest first",
    "Lists the journal of the data folder: every command that changed it, newest first. With an address, only the records about it. One line a record: the sequence number, the time, the actor, the command and its target (- for none). --format json prints the records with the paths each wrote and removed.\n"
    "\n"
    "--limit is how many records, 1 to 1000 (default 20). The data folder is named before the subcommand: ursatz --data <dir> history.\n"
    "\n"
    "Exit status: 0 success; 1 usage (a text that is no address, a limit out of range); 2 the folder cannot be opened, or any other failure.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data history\n"
    "  ursatz --data ~/ursatz-data history item:itm-3f2a --limit 5",
     FLAGS_HISTORY, 0, 1, run_history},
    {"apply",
    "apply [<file>] [--wait] [--format <text|json>]",
     "Apply commands to the data folder from a file or standard input",
    "Applies commands to the data folder, one after the other, as the actor of the session (--actor, default claude): the journal records them under that actor. The commands come from <file>, or from standard input (- or no file): JSON Lines, one command object a line (blank lines skipped), or one JSON array.\n"
    "\n"
    "A command is {\"command\": <name>, \"target\": <id>, \"base_version\": <n>, \"params\": {...}, \"file\": <path>}: only the name is needed. file is a path whose bytes go with the command (library.import), named by its base name. An actor member is refused: the actor comes from --actor. The names are the workshop's commands (project.create, item.save, item.compose, ...).\n"
    "\n"
    "Each command prints its number, name, target (- for none) and the journal sequence it wrote, and, when it started a job, the job's id; with --wait the job is waited for and its state follows (a failed job also prints its error). The first command the workshop refuses stops the run (the commands before it stay applied, each is its own transaction): a stale base version says the current version. --format json prints each reply as one line.\n"
    "\n"
    "The data folder is named before the subcommand: ursatz --data <dir> apply commands.jsonl.\n"
    "\n"
    "Exit status: 0 every command was applied; 1 usage (the input does not read, an actor member) or the first command refused as a request; 2 the folder cannot be opened, or any other failure.\n"
    "\n"
    "Example:\n"
    "  echo '{\"command\":\"project.create\",\"params\":{\"name\":\"Songs\"}}' | ursatz --data ~/ursatz-data apply\n"
    "  ursatz --data ~/ursatz-data --actor claude apply edits.jsonl --wait",
     FLAGS_APPLY, 0, 1, run_apply},
    {"compose",
    "compose item:<id> [--seed <n>] [--update-uses] [--base <n>] [--no-wait]",
     "Compose a theme or piece of the data folder into its next version",
    "Composes the theme or piece the address names into its next version, with its files, as a job the command waits for. It prints item <id> version <n> when the job is done. --seed makes the same composition again; without it the seed is new. --update-uses moves every updated use to its source's current version first. --base is the version the command is made against (default: the current one): a newer version refuses it. --no-wait prints the job's id and returns. The data folder is named before the subcommand: ursatz --data <dir> compose item:<id>.\n"
    "\n"
    "Exit status: 0 success; 1 usage, or a command the workshop refuses (a stale --base, a motif); 2 the folder cannot be opened or the job failed.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data compose item:itm-3f2a --seed 7",
     FLAGS_COMPOSE, 1, 1, run_compose},
    {"rate",
    "rate <ses-id> --round <n> --seed <s> [--rating <batch>=<1-5>]... [--better <batch>|same] [--note <text>]\nrate item:<id>@v<n> --rating <1-5> [--note <text>]",
     "Record the ratings of a listening round or of an item version",
    "Records what you thought of the samples of a listening session (R-674): --rating <batch>=<1-5> rates the sample of that batch at --seed in round --round (repeat it for each batch), and --better <batch> or --better same says which batch of the round sounded better, as a pick. rate item:<id>@v<n> --rating <1-5> rates one version of an item. --note says why, on each rating and pick made. It prints one line for each id made: rat-<id> <batch> <rating>, pck-<id> better <batch> and, for an item version, rat-<id> v<n> <rating>. A new rating of a sample replaces the earlier one. A rating needs a sample the session recorded as made; a pick needs a batch the round recorded. The ids are what keep as default cites (ratings.keep). The data folder is named before the subcommand: ursatz --data <dir> rate <ses-id> ...\n"
    "\n"
    "Exit status: 0 success; 1 usage, or a rating or pick the workshop refuses (a sample that was not recorded, a rating outside 1 to 5, a batch the round does not have); 2 the folder cannot be opened or any other failure.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data rate ses-3f2a --round 1 --seed 1 --rating A=3 --rating B=4 --better B --note \"B is brighter\"\n"
    "  ursatz --data ~/ursatz-data rate item:thm-3f2a@v2 --rating 5",
     FLAGS_RATE, 1, 1, run_rate},
    {"explore",
    "explore item:<id> [--distance <n>] [--seed <n>] [--taste <name>] [--base <n>] [--no-wait]",
     "Compose a theme of the data folder with some settings moved away from your taste",
    "Composes the theme the address names into its next version as item.compose does, with some settings moved away from the ones you have chosen (R-673): --distance of them (1 to 8, default the setting explore.distance, 2), picked and moved from the seed. The same --seed moves the same settings and makes the same composition; without it the seed is new. --taste names the taste profile to move from. The version's settings.json records the settings that moved. It prints item <id> version <n> when the job is done; --no-wait prints the job's id and returns. --base is the version the command is made against (default: the current one). The data folder is named before the subcommand: ursatz --data <dir> explore item:<id>.\n"
    "\n"
    "Exit status: 0 success; 1 usage, or a command the workshop refuses (a stale --base, a piece, a distance outside 1 to 8); 2 the folder cannot be opened, the item is not there or the job failed.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data explore item:thm-3f2a --distance 2 --seed 7",
     FLAGS_EXPLORE, 1, 1, run_explore},
    {"restore",
    "restore item:<id>@v<n> [--base <n>] [--note <text>]",
     "Make the next version of an item a copy of an older one",
    "Makes the next version of the item a copy of version n, with its files: the version is not rewritten, the item gains a new one that holds what version n held (restoring is an undo that keeps the history). It prints item <id> version <new> (restored v<n>). The note of the new version is restore v<n> unless --note gives one. --base is the version the command is made against (default: the current one): a newer version refuses it. The current version cannot be restored. The data folder is named before the subcommand: ursatz --data <dir> restore item:<id>@v<n>.\n"
    "\n"
    "Exit status: 0 success; 1 usage, or a command the workshop refuses (a stale --base, the current version, a version that is not there); 2 the folder cannot be opened, or any other failure.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data restore item:itm-3f2a@v2\n"
    "  ursatz --data ~/ursatz-data restore item:itm-3f2a@v2 --note \"back to the short ending\"",
     FLAGS_RESTORE, 1, 1, run_restore},
    {"migrate",
    "migrate [--dry-run] [--backup <dir>]",
     "Migrate a data folder from an older layout to the current one",
    "Migrates the data folder to the current layout (2): a folder of the GUI's old layout (layout 0), or one in layout 1. It first checks the folder and refuses one the GUI's one-time migrations never ran on, with the check's message. --dry-run writes nothing and reports every planned move and rewrite. A real run copies the whole folder to --backup first (default: beside it, named <folder>-backup-<time>), then migrates, and prints the report as JSON. The folder must not be open: while a process holds it, stop that process first. The data folder is named before the subcommand: ursatz --data <dir> migrate.\n"
    "\n"
    "Exit status: 0 success; 1 usage; 2 the folder is in use by another process, or the migration was refused or failed.\n"
    "\n"
    "Example:\n"
    "  ursatz --data ~/ursatz-data migrate --dry-run\n"
    "  ursatz --data ~/ursatz-data migrate",
     FLAGS_MIGRATE, 0, 0, run_migrate},
};

const size_t CLI_COMMAND_COUNT = sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]);

const CliCommand *cli_command_find(const char *name) {
    for (size_t i = 0; name && i < CLI_COMMAND_COUNT; i++) {
        if (strcmp(CLI_COMMANDS[i].name, name) == 0) {
            return &CLI_COMMANDS[i];
        }
    }
    return NULL;
}

/* "Usage: ursatz <form>" for the first form of the usage and "       ursatz <form>" for the others, to the buffer. */
static size_t usage_lines(const CliCommand *command, char *out, size_t size) {
    size_t used = 0;
    const char *form = command->usage;
    bool first = true;
    while (form && *form && used < size) {
        const char *end = strchr(form, '\n');
        int length = end ? (int)(end - form) : (int)strlen(form);
        used += (size_t)snprintf(out + used, size - used, "%sursatz %.*s\n", first ? "Usage: " : "       ", length, form);
        first = false;
        form = end ? end + 1 : NULL;
    }
    return used;
}

const char *cli_usage_of(const char *name) {
    static char text[4096];
    const CliCommand *command = cli_command_find(name);
    text[0] = '\0';
    if (command) {
        size_t used = usage_lines(command, text, sizeof text);
        if (used > 0 && used < sizeof text) {
            text[used - 1] = '\0'; /* no final line break: the message adds it */
        }
    }
    return text;
}

void cli_print_help(const CliCommand *command, FILE *out) {
    char usage[4096];
    usage_lines(command, usage, sizeof usage);
    fprintf(out, "%s\n%s\n\nOptions:\n", usage, command->description);
    for (const char *const *name = command->flags; name && *name; name++) {
        const CliFlag *flag = cli_flag_find(*name);
        if (!flag) {
            continue;
        }
        char left[96];
        snprintf(left, sizeof left, "%s%s%s%s%s", flag->name, flag->alias ? ", " : "", flag->alias ? flag->alias : "", flag->value ? " " : "",
                 flag->value ? flag->value : "");
        if (strlen(left) <= 30) {
            fprintf(out, "  %-30s  %s\n", left, flag->help);
        } else {
            fprintf(out, "  %s\n  %30s  %s\n", left, "", flag->help);
        }
    }
    fprintf(out, "  %-30s  %s\n", "--help", "print this text");
}
