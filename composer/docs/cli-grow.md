# grow command

```sh
ursatz-composer grow (<motive-input> | --from-corpus <dir> --like <file>) --into <sentence|period|hybrid> [--hybrid <beginning>+<ending>] --key <key> --meter <n/m> [--bars 4|8] [--cadence PAC|IAC|HC] [--first-cadence HC|IAC] [--accompaniment block|arpeggiated|alberti|oom-pah|any] [--texture <name>|chorale] [--voicing] [--check] [--style <file>] [--style-weight <n/d>] [--blend <file>] [--blend-weight <n/d> | --blend-weights <dimension>=<n/d>,...] [--seed <n>] [--suggest <n>] [--out <file.json>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--explain <file>] [--format text|json]
```

Grows a theme from the motive in `<motive-input>`: its structure, a harmony, a bass line and a melody,
ending in the cadence asked for. It builds a request for `grow_build_plan` and runs `grow_run`
(`<ursatz_composer/grow.h>`, see [grow.md](grow.md)); the result is the plan of the theme.

The input is loaded as for the [motive command](cli-motive.md): a MIDI file, a MusicXML file, a note
list or a motive JSON file. `--key` and `--meter` are required and are given to the loader, so a file
that declares another meter is an error. The motive must fit in one bar of the meter and have at
least two notes. The meters are 2/4, 3/4, 4/4 and 6/8.

## Forms

| `--into` | Pairing | Notes |
| --- | --- | --- |
| `sentence` | presentation + continuation | |
| `period` | antecedent + consequent | The antecedent ends in `--first-cadence`, weaker than `--cadence`. |
| `hybrid` | `--hybrid <beginning>+<ending>` | Any of the other seven pairings. |

A hybrid is named with a beginning (`presentation`, `antecedent`, or `cbi` for a compound basic idea)
and an ending (`continuation`, `consequent`, `cadential`), as in `--hybrid antecedent+continuation`.
`presentation+continuation` and `antecedent+consequent` are the sentence and the period and are
refused as hybrids. `--hybrid` with any other form is an error.

## Options

| Option | Meaning |
| --- | --- |
| `--bars <4\|8>` | Length of the theme. 8 by default, with a two-bar basic idea; 4 has a one-bar basic idea. |
| `--cadence <type>` | PAC (default), IAC or HC: the cadence the theme ends in. |
| `--first-cadence <type>` | HC (default) or IAC: the weak cadence of an antecedent. Only for a period or a hybrid that begins with an antecedent. |
| `--accompaniment <pattern>` | Write an accompaniment in the inner voices: `block`, `arpeggiated`, `alberti` (not in 6/8), `oom-pah`, or `any`, which chooses from the seed. See [accompaniment.md](accompaniment.md). |
| `--texture <name>` | The texture the accompaniment is to be heard as, a standard-texture name in any case: `homophony`, `homorhythm` (block chords only) or `melody and accompaniment`. Alone it implies `--accompaniment any`. Other textures are refused. `--texture chorale` is a voicing, not an accompaniment: see `--voicing`. |
| `--voicing` | Voice the harmony and bass in four parts by the standard voicing rules ([voicing.md](voicing.md)): the grown melody stays the soprano and an alto and a tenor are added under it. With `--texture chorale` instead the melody is written by the voicing too, a note a chord (a chorale), in place of the grown melody. The two are not combined with each other or with `--accompaniment`. |
| `--check` | Analyze the theme with the Ursatz Analyzer ([analyzer-check.md](analyzer-check.md)) and pass over a seed whose analysis contradicts the plan: a planned cadence or the key not heard as planned, or a hard voice-leading fault. The next seed is tried, as for a stuck run, and the seed kept is printed. |
| `--style <file>` | Prefer the harmony and melody closest to a style, a style model file from the Ursatz Analyzer. The Analyzer's distance from the style is printed with the theme. See [style.md](style.md). |
| `--style-weight <n/d>` | How strongly the style is preferred (default `1/1`; needs `--style`). |
| `--blend <file>` | A second style model file, blended with the style (`--style`; needs `--style`). The share of each of the two in each dimension is set by the weights below. See [style.md](style.md). |
| `--blend-weight <n/d>` | The second style's share in every dimension, from 0 (only the first) to 1 (only the second); default `1/2`. Needs `--blend`. |
| `--blend-weights <dimension>=<n/d>,...` | The share per dimension: `harmony`, `rhythm`, `melody`, `form`. A dimension not named takes `--blend-weight`, else `1/2`. Needs `--blend`. |
| `--from-corpus <dir>` | Start from material in a corpus instead of a motive input: the output directory of `ursatz-analyzer --corpus`. Needs `--like`; not with a motive input. The source piece, its measures and the score are printed (JSON: `source`) and kept in the plan on the motive node. Exit 2 when no passage scores 1/2 or more. See [style.md](style.md). |
| `--like <file>` | With `--from-corpus`: a MIDI or MusicXML passage. One bar of a similar passage of the corpus, picked by the seed among the best, becomes the motive, in `--key`. |
| `--harmony "<text>"`, `--feature <label:first-last>`, `--role <role>` | The motive's own chords, labelled features and role, as for the [motive command](cli-motive.md#harmony-features-and-role). The chord onsets are in beats of `--meter`. They are kept with the motive; the theme is not grown from them yet. Not with `--from-corpus`. |
| `--seed <n>` | Whole number from 0. By default one is derived from the clock. The same input, options and seed give the same output. |
| `--suggest <n>` | 1 to 16 different themes instead of one. |
| `--out <file.json>` | The plan as JSON, as `plan_write_json_file` writes it. |
| `--midi <file>`, `--musicxml <file>` | The melody and the bass of the theme, and the accompaniment, one part for each voice of it. The source material is not part of the piece. |
| `--tempo <bpm>` | Write a tempo in the MIDI and MusicXML files: quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. See [export.md](export.md). |
| `--plain-midi` | MIDI without track names and section markers. See [export.md](export.md). |
| `--explain <file>` | Why each candidate was chosen or rejected while the theme was grown: per target the chosen candidate, its runners-up and the rejected ones with their reasons, the form heard, the Analyzer check (with `--check`) and the seeds passed over. JSON when the file name ends in `.json`, else text; with `--suggest`, theme k goes to `<name>-k.<extension>` like the other files. See [explain.md](explain.md). |
| `--format <text\|json>` | How the result is printed on stdout. Text by default. |

## Seeds

The seed is always printed. A run can get stuck (a stage leaves a target open); the next seeds are tried,
up to 16, and the seed that worked is printed. Giving that seed again gives the same theme.

## Suggestions

`--suggest N` gives N different themes. Theme 1 is the first seed that works; theme k is the k-th
distinct successful seed after it. Two themes whose harmony, bass and melody are the same count once.
At most 16 seeds are tried for each theme asked for; if fewer are found, a note on stderr says how many.
With files, theme k goes to `<name>-k.<extension>`, as for `progression --suggest`.

## Output

Per theme, in text:

- the seed;
- the structure: the nodes of the theme, their kind and where they sit, in bars from the start of the
  theme (`continuation (subphrase) bars 4-8`);
- the chords, one row for each, with the bar and the beat (both from 1), the roman numeral, the
  function and the bass notes under the chord;
- the motive developments: `m0` is the motive given; each other motive is made from it for an idea
  (`m1 for repetition: restated, exact`);
- the standard-form rule score: the share of the structure's soft rules that hold, with the heard form
  counted as one more (`5/5 (100%)`);
- the form heard.

### The form heard

`form_hear` hears the finished theme with standard-form's analysis adapter. The result is one of

- `heard as <form>`: the form asked for. A hybrid is heard as asked only with the beginning and the
  ending asked for, and is written with them (`hybrid (compound basic idea + continuation)`);
- `heard as <form>, not <requested>`;
- `no theme heard`.

A theme need not be heard as the form it was grown as; that is reported, not an error, and does not
change the exit status. Most periods are heard as hybrids and every theme of 4 bars is heard as no
theme (see [grow.md](grow.md)).

`--format json` prints the same as one object: the request (`key`, `meter`, `bars`, `into`,
`pairing`, `cadence`, `first_cadence` for an antecedent, and `accompaniment` and `texture`, or `voicing` (`satb` or `chorale`), when asked for), `themes` (each with `seed`, `structure`,
`chords`, `developments`, `score`, `requested`, `heard`, `heard_as_requested` and `heard_text`) and
`files`.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument. |
| 2 | The request cannot be satisfied: the input cannot be loaded, the motive is longer than a bar or has one note, a period asks for a weak cadence that is not weaker than the theme's, or no seed works. |
| 3 | An output file could not be written. No output file is left behind. |

## Example

```sh
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --seed 7 --out theme.json --midi theme.mid
ursatz-composer grow theme.txt --into period --first-cadence IAC --key "D minor" --meter 4/4 --suggest 3 --out period.json
ursatz-composer grow theme.txt --into hybrid --hybrid antecedent+continuation --key "D minor" --meter 4/4
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --accompaniment alberti --midi theme.mid
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --texture "melody and accompaniment"
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --texture chorale --midi chorale.mid
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --voicing --seed 3
ursatz-composer grow --from-corpus corpus-out --like passage.mid --into sentence --key "D major" --meter 4/4 --seed 3
```
