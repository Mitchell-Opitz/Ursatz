# Flags

The flag table of `ursatz` (`cli/src/flags.c`): every flag has one meaning in every command that takes it. `ursatz <command> --help` prints the flags of one command; this page lists them all with the commands that take each. It is the output of `ursatz help flags --format markdown`: do not edit. Back to the [cli docs](README.md).

| Flag | Meaning | Commands |
| --- | --- | --- |
| `--above` | the line is the bass and the counter-melody sings above it | `counterpoint` |
| `--accompaniment <pattern>` | write an accompaniment in the inner voices: block, arpeggiated, alberti, oom-pah or any | `grow`, `plan` |
| `--accompaniment-format <format>` | the format of the accompaniment motive's file, as --input-format | `grow` |
| `--accompaniment-motive <file>` | a saved accompaniment motive, restated on each chord in chord tones | `grow` |
| `--accompaniment-pickup <beats>` | the pickup of the accompaniment motive, as 1 or 1/2 | `grow` |
| `--actor <name>` | global: who a command is journaled as (default claude); stands before the subcommand | every command (stands before it) |
| `--amount <n>` | interval: change the interval by n semitones, negative to narrow it | `develop` |
| `--at <L>[:<bar>]` | where the --given-* before it goes: a section letter and the bar of it the material starts in | `plan` |
| `--axis <pitch>` | invert: the pitch to mirror around, D4 or a MIDI number | `develop` |
| `--backup <dir>` | migrate: where the copy of the whole folder goes before the migration (default: beside it, named <folder>-backup-<time>) | `migrate` |
| `--bars <n>` | the length in bars | `progression`, `grow`, `plan`, `listen` |
| `--base <n>` | the version of the item a command is made against (default: its current version); a newer one refuses the command | `regenerate`, `compose`, `restore` |
| `--basic <file>` | the basic motive the family of forms comes from; remoteness is measured from it | `variants`, `recognize` |
| `--basic-format <format>` | the format of the basic motive's file, as --input-format | `variants`, `recognize` |
| `--basic-harmony <text>` | the basic motive's own chords, as --harmony | `variants`, `recognize` |
| `--basic-pickup <beats>` | the basic motive's pickup, as --pickup | `variants`, `recognize` |
| `--bass` | write a bass line (the default) | `progression` |
| `--below` | the line is the melody and the counter-melody sings below it | `counterpoint` |
| `--better <batch>` | rate: the batch of the round that sounded better, or same | `rate` |
| `--blend <file>` | a second style model file, blended with --style | `grow`, `plan`, `fill`, `listen` |
| `--blend-weight <n/d>` | the second style's share in every dimension, 0 to 1 | `grow`, `plan`, `fill`, `listen` |
| `--blend-weights <list>` | the second style's share per dimension, as harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d> | `grow`, `plan`, `fill`, `listen` |
| `--by <n>` | restate: move the motive by n semitones, or n steps with --tonal | `develop` |
| `--cadence <type>` | the cadence the phrase ends in: PAC, IAC, HC, DC or PC | `progression`, `grow` |
| `--candidates <n>` | compose n candidates and write the best | `fill` |
| `--cells <n>` | the cells each chord of the --given-progression before it lasts | `plan` |
| `--chain <file>` | a JSON file of steps, the same chain of operations for every variant | `variants` |
| `--check` | analyze the result with the Ursatz Analyzer and pass over a seed whose analysis contradicts the plan | `grow`, `theme`, `piece`, `fill`, `listen` |
| `--count <n>` | how many to make | `variants`, `random-motive`, `piece`, `plan`, `variations`, `harmonize`, `listen` |
| `--data <dir>` | global: the data folder (or the environment variable URSATZ_DATA); stands before the subcommand | every command (stands before it) |
| `--degree <n>` | restate: move the first note to scale degree n | `develop` |
| `--depth <n>` | the most operations in the chain, 1 to 4 | `recognize` |
| `--development <name>` | how a theme develops: melody, balanced or theme | `listen` |
| `--distance <d>` | how far the variants are from the motive: close, medium or far | `variants` |
| `--drop <n>` | liquidate: how many notes to drop at each step | `develop` |
| `--dry-run` | migrate: report every planned move and rewrite, and write nothing | `migrate` |
| `--each-op` | one variant for every operation that applies to the motive's form | `variants` |
| `--embellish <level>` | how often chords are prolonged: none, low, medium or high | `progression` |
| `--ending-midi <file>` | write the ending version as a MIDI file | `piece` |
| `--ending-musicxml <file>` | write the ending version as MusicXML | `piece` |
| `--ending-out <file>` | write the ending version's plan as JSON | `piece` |
| `--explain <file>` | write why each candidate was chosen or rejected | `grow`, `theme`, `piece`, `fill` |
| `--feature <l:a-b>` | a labelled run of notes by 0-based index, as a:0-2; repeatable | `motive`, `variants`, `recognize`, `grow` |
| `--first-cadence <type>` | the weaker cadence an antecedent ends in: HC or IAC | `grow` |
| `--form <name>` | the form: for a motive full, rhythm or contour; for a plan binary, ternary, loop and the like | `motive`, `variants`, `recognize`, `random-motive`, `plan` |
| `--format <text|json>` | how to print the result on stdout, text (the default) or json | `progression`, `motive`, `variants`, `recognize`, `random-motive`, `counterpoint`, `grow`, `theme`, `piece`, `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize`, `status`, `query`, `history`, `apply` |
| `--from <loop-plan.json>` | work on a loop plan the command wrote, with --request | `piece` |
| `--from-corpus <dir>` | start from material in a corpus output folder, with --like | `grow`, `plan` |
| `--function <name>` | keep only variants that suit a function: beginning, continuation, contrast or cadential | `variants` |
| `--given-bass <file>` | a bass line to place in the piece, no rests | `plan` |
| `--given-melody <file>` | a phrase to place in the piece, which fill never changes | `plan` |
| `--given-progression <numerals>` | Roman numerals, one chord each, to place in the piece | `plan` |
| `--harmony <text>` | the motive's own chords as Roman numerals with onsets in beats after @, as 'I V6@2 I@3' | `motive`, `variants`, `recognize`, `grow` |
| `--hybrid <b>+<e>` | with --into hybrid: the beginning and the ending, as antecedent+continuation | `grow` |
| `--input-format <format>` | the format of the input: auto, midi, musicxml, notes, degrees, rhythm, contour or json | `motive`, `develop`, `variants`, `recognize`, `analyze` |
| `--interval <n>` | interval: the 1-based interval to change, every one by default | `develop` |
| `--into <form>` | the theme to grow: sentence, period or hybrid | `grow`, `listen` |
| `--keep-contour` | interval: keep the contour of the motive | `develop` |
| `--keep-rhythm` | liquidate: keep the rhythm of the motive | `develop` |
| `--key <key>` | the key, for example 'D minor' | `progression`, `motive`, `develop`, `variants`, `recognize`, `random-motive`, `counterpoint`, `grow`, `plan` |
| `--lane <lane>` | harmony, bass, melody or inner (and all, where it is the default) | `piece`, `lock`, `regenerate`, `variations` |
| `--layers <dir>` | write the layers of the result into this folder: the realization, the reading and, for a composed piece, the plan | `theme`, `piece`, `fill`, `analyze` |
| `--length <n>` | fragment: how many notes the fragment has | `develop` |
| `--like <file>` | a MIDI or MusicXML passage; the first bar of a similar passage of the corpus becomes the motive | `grow`, `plan` |
| `--like-progression <numerals>` | a progression like the one of the passage of the corpus that best matches these numerals | `plan` |
| `--limit <n>` | how many records or results at most | `history` |
| `--list-ops` | print every operation, with its parameters and the forms it applies to | `variants` |
| `--list-schemas` | list the schema and sequence names and exit | `progression` |
| `--material <word>` | major or minor (style, where the style names one) | `random-motive`, `counterpoint`, `grow`, `plan`, `listen` |
| `--measures <first>-<last>` | the measures to show, <first>-<last>, inclusive: as written in the score for analyze --view reading, bars from 1 for show | `analyze`, `show` |
| `--meter <n/m>` | the meter, for example 4/4 | `progression`, `motive`, `develop`, `variants`, `recognize`, `random-motive`, `counterpoint`, `grow`, `plan`, `listen`, `analyze` |
| `--midi <file>` | write the result as a MIDI file | `progression`, `motive`, `develop`, `counterpoint`, `grow`, `theme`, `piece`, `fill`, `regenerate`, `render` |
| `--min <n>` | fragment or liquidate: the fewest notes that stay | `develop` |
| `--model-plan <file>` | write the minimal plan of the Analyzer's reading (its sections, keys and chords) to this file | `analyze` |
| `--modulate` | plan the keys and cadences across the sections from the seed | `plan` |
| `--motif <file>` | the motif of every theme: anything the motive command reads | `listen` |
| `--motive <file>` | the motive: a MIDI file, a MusicXML file, a note list or a motive JSON file | `plan` |
| `--musicxml <file>` | write the result as a MusicXML file | `progression`, `motive`, `develop`, `counterpoint`, `grow`, `theme`, `piece`, `fill`, `regenerate`, `render` |
| `--name <name>` | the name of the style model | `style-profile` |
| `--no-bass` | write only the chords | `progression` |
| `--no-wait` | do not wait for the job a command starts: print its id | `regenerate`, `compose` |
| `--node <id>` | the node's id in the plan file | `lock`, `regenerate`, `variations` |
| `--note <text>` | restore: the note of the new version (default: restore v<n>); rate: why the rating or the pick | `rate`, `restore` |
| `--op <name>` | the operation: restate, invert, retrograde, interval, fragment, liquidate, augment or diminish | `develop` |
| `--out <file>` | where the output is written: a plan, a corpus output folder or a style model | `progression`, `counterpoint`, `grow`, `theme`, `piece`, `plan`, `fill`, `lock`, `regenerate`, `corpus`, `style-profile`, `query` |
| `--out-dir <dir>` | where the files are written; created if missing | `piece`, `variations`, `harmonize`, `listen` |
| `--params <file>` | a JSON file with the params object, or - for standard input | `query` |
| `--pickup <beats>` | the beats before the first barline, as 1 or 1/2 | `motive`, `variants`, `recognize`, `analyze` |
| `--plain-midi` | MIDI: no track names and no section markers | `progression`, `develop`, `counterpoint`, `grow`, `theme`, `piece`, `fill`, `regenerate`, `variations`, `harmonize`, `render` |
| `--port <n>` | global: the HTTP port of the workshop, 1 to 65535; stands before the subcommand | every command (stands before it) |
| `--position <where>` | fragment: leading, trailing or middle | `develop` |
| `--print-settings` | print the default preferences document and exit | `listen` |
| `--rating <r>` | rate: <batch>=<1-5> for a sample of a session, or <1-5> for an item version; repeatable | `rate` |
| `--ratio <n:m>` | augment or diminish: the ratio of the new durations to the old | `develop` |
| `--record <file>` | write what the run did: seeds, candidates and the check | `fill` |
| `--regenerate <id|none>` | write the section with that id again, the rest locked | `piece` |
| `--render-key <key>` | hear the motive in this key, with the same degrees | `motive` |
| `--request <request.json>` | the request a loop plan was made from, with --from | `piece` |
| `--resume` | skip the pieces unchanged since the last run | `corpus` |
| `--role <role>` | melodic or accompaniment | `motive`, `variants`, `recognize`, `random-motive`, `grow` |
| `--round <n>` | listen --session and rate: the round of the listening session, 1 to 10000 | `rate` |
| `--schema <name>` | build the start of the phrase on a galant schema (see --list-schemas) | `progression` |
| `--section <letter>` | the section with that form letter or id | `lock`, `regenerate`, `variations`, `harmonize` |
| `--seed <n>` | a whole number from 0; the same seed gives the same output | `progression`, `develop`, `variants`, `random-motive`, `counterpoint`, `grow`, `theme`, `piece`, `plan`, `fill`, `regenerate`, `variations`, `harmonize`, `listen`, `compose`, `rate` |
| `--sequence <name>` | build a continuation on a harmonic sequence (see --list-schemas) | `progression` |
| `--settings <file>` | a preferences document | `listen` |
| `--species <1-4>` | the species of counterpoint | `counterpoint` |
| `--strategy <name>` | how motifs are made: segments or drawn | `random-motive`, `listen` |
| `--style <file>` | a style model file from the Ursatz Analyzer; prefer the harmony and melody closest to it | `random-motive`, `grow`, `plan`, `fill`, `listen` |
| `--style-weight <n/d>` | how strongly the style is preferred | `grow`, `plan`, `fill` |
| `--style-without <category>` | read the style without this category; repeatable | `random-motive`, `grow`, `plan`, `fill` |
| `--suggest <n>` | write n alternatives instead of one; with files, number k goes to <name>-k.<extension> | `progression`, `develop`, `counterpoint`, `grow`, `theme` |
| `--target-feature <l:a-b>` | a labelled run of the target's notes; repeatable | `recognize` |
| `--target-format <format>` | the target's input format, as --input-format | `recognize` |
| `--target-harmony <text>` | the target's own chords, as --harmony | `recognize` |
| `--target-pickup <beats>` | the target's pickup, as --pickup | `recognize` |
| `--tempo <bpm>` | the tempo of the files in quarter notes per minute, 20 to 400, a whole number or a fraction like 195/2 | `progression`, `motive`, `develop`, `counterpoint`, `grow`, `fill`, `regenerate`, `variations`, `harmonize`, `render`, `listen` |
| `--texture <name>` | the texture the accompaniment is to be heard as | `grow`, `plan` |
| `--theme-request <file>` | the theme request every theme of the batch is made from | `listen` |
| `--title <text>` | the title of the text export (with --view reading) | `analyze` |
| `--tonal` | work in scale steps of the key and not in semitones | `develop` |
| `--tone <n>=<role>` | label note n (1-based) as chord or as a non-chord tone; repeatable | `motive` |
| `--update-uses` | compose: move every updated use to its source's current version first | `compose` |
| `--value-scale <n or n/d>` | read the note values of the file scaled by this factor (a notation reading) | `analyze` |
| `--variations <id>` | make --count different versions of the section with that id | `piece` |
| `--version <n>` | regenerate of an item: the version of the piece the section is written again from (default: the current one) | `regenerate` |
| `--view <view>` | a view of the result: reading is the measure-addressed text export of analyze, framed as a view; show takes summary, structure, harmony, line:<name>, part:<name>, reading, full or json | `analyze`, `show` |
| `--voicing` | keep the grown melody as the soprano and add an alto and a tenor under it | `grow` |
| `--wait` | wait for the job a command starts, and say how it ended | `apply` |
| `--what <what>` | motifs or themes | `listen` |
| `--write-midi` | also write each version as a MIDI file | `variations`, `harmonize` |
| `--write-musicxml` | also write each version as a MusicXML file | `variations`, `harmonize` |

`--mode` is another name of `--material`. A flag that may be repeated (`--given-melody`, `--feature`, `--tone`, ...) says so in its meaning; the position of a repeated flag among the others is kept, so an `--at` follows the `--given-*` it places.
