# listen command

The `listen` command of `ursatz`: a batch of motifs or themes from fixed seeds and one settings file, written as files a listening session plays and reads. Back to the [docs index](../../composer/docs/README.md).

```sh
ursatz listen --what <motifs|themes> --out-dir <dir> [--count <n>] [--seed <n>] [--settings <file>] [--material <word>] [--meter <n/m>] [--style <file>] [--blend <file>] [--blend-weight <n/d>] [--blend-weights <list>] [--tempo <bpm>] [--strategy <name>] [--preset <name>] [--into <form>] [--development <name>] [--bars <n>] [--check] [--motif <file>] [--theme-request <file>] [--blueprint <file>]
ursatz listen --print-settings
```

`listen` is a client of the API ([api.md](../../composer/docs/api.md)): it calls `composer_api_random_motive`, `composer_api_motive`, `composer_api_motive_render` and `composer_api_theme` and writes what they answer. It plays nothing and writes no notation text; the session does both (R-404, R-412). With the same options, settings and seed, every file is byte-identical, so a round can be made again after one setting changes and the two rounds compared.

## Options

| Option | Meaning |
| --- | --- |
| `--what motifs\|themes` | What each sample is (required). |
| `--out-dir <dir>` | Where the files are written (required). The directory is created, with its parents. |
| `--count <n>` | How many samples, 1 to 64 (default 10). |
| `--seed <n>` | Whole number from 0 (default 1). Sample `i`, counted from 0, uses the seed `--seed` + `i`. |
| `--settings <file>` | A preferences document ([preferences.md](../../composer/docs/preferences.md)); every member is optional. Absent: no `preferences` member is sent and the Composer's defaults apply. A member outside its limits exits 1 with its path (`/melody/keep`). |
| `--print-settings` | Print the document of the defaults, as the vocabulary serves it, and exit 0. Edit a copy of it into a settings file. |
| `--material <word>` | Default major. Any material the Composer writes keys in: major, minor, or a church mode (dorian, lydian, mixolydian, phrygian, locrian). A word it refuses (harmonic minor, a synthetic scale, a word that is no material) fails the run with its message (a usage error, exit 1). `--mode` is read as the same option. The requests `listen` writes name `material`. |
| `--meter <n/m>` | Default 4/4. With `--motif`, the meter the motif is read in; absent, the motif's own. |
| `--style <file>` | A style model file ([style.md](../../composer/docs/style.md)). Motifs: 8 candidates are drawn for each and the closest kept. Themes: the style is preferred. |
| `--blend <file>`, `--blend-weight`, `--blend-weights` | Themes: the second style model and its shares, as `grow`'s ([cli-grow.md](cli-grow.md)). Need `--style`. |
| `--tempo <bpm>` | The tempo of the MIDI and MusicXML files, 20 to 400. |
| `--strategy segments\|drawn\|blueprint` | How a generated motif is made (default `segments`). |
| `--into sentence\|period` | Themes: the pairing (default `sentence`). |
| `--preset <name>` | A preset of the Composer (`ff7` or `hollow-knight`, [presets.md](../../composer/docs/presets.md)): the settings of her listening and, for themes, the accompaniment that goes with them; `--settings` and the session's settings apply over it. Motifs and themes (the manifest line records it as `preset`; an unknown name exits 1). |
| `--development melody\|balanced\|theme\|blueprint` | Themes: how far the theme strays from the motif (default `blueprint` where the theme fits it, else `balanced`: the request names no development unless the option is given, R-756); `blueprint` has the Composer write the theme's blueprint from the motif ([blueprint-generator.md](../../composer/docs/blueprint-generator.md)). |
| `--bars 4\|8` | Themes: the length (default 8). |
| `--check` | Themes: pass over a seed whose analysis contradicts the plan. |
| `--motif <file>` | Themes: the motif of every sample, anything the [`motive`](cli-motive.md) command reads (a note list, MusicXML, MIDI, a motive document). It is read in key C and the mode. Absent: a generated motif for each sample, made with the sample's seed, the mode, meter, strategy, style and settings. |
| `--theme-request <file>` | Themes: a [theme request](../../composer/docs/theme-request.md), for example a GUI composed theme's `request.json`, with its `seed`, `suggest`, `midi`, `musicxml` and, when `--settings` is given, `preferences` replaced. `--style`, `--blend`, `--tempo` and `--check` replace theirs when given. |
| `--blueprint <file>` | Themes: a [theme request with a blueprint](../../composer/docs/blueprints.md), as its JSON or as [Ursatz Text](../../composer/docs/blueprints.md#the-text-form) (`ursatz-text 1 blueprint`, read by the workshop's tool `blueprint-parse`), made as `--theme-request` is: the sample's `seed`, `suggest`, `midi`, `musicxml` and `preferences` replace the request's, and `--style`, `--blend`, `--tempo` and `--check` replace theirs when given. A file with no `blueprint` member, or a text with a mistake (`line 3: bar 3 is out of order: the next bar is 2`), exits 1. |
| `--plans` | Themes: put each theme's plan in its JSON (`themes[0].plan`), so `ursatz critique` can read the theme ([cli-critique.md](cli-critique.md)). The notes, the MIDI file and the MusicXML are the same bytes as without it. Refused with `--what motifs`. |

`--motif` and `--theme-request` together exit 1, and `--blueprint` with either of them. A theme built from a motif is a request of schema `ursatz-theme-request` version 2: key C, the mode, the meter, `--into`, `--bars`, `--development`, one motive `M1` (the motif's degrees, pickup, harmony and features, `form` full, role melodic) in the `basic` slot.

## Files

For sample `i` (two digits from 01), in `--out-dir`:

| File | Holds |
| --- | --- |
| `<i>.mid` | The motif or the theme (melody, bass and accompaniment) as MIDI. |
| `<i>.musicxml` | The same as MusicXML. |
| `<i>.json` | The API response of the sample. |
| `listen.jsonl` | One line per sample, appended. |

A line of `listen.jsonl`: `{"index", "seed", "what", "motif" ("generated" or the file path), "theme_request" (the path of `--theme-request` or `--blueprint`, or null), "style", "blend", "settings" (the file path or null), "preferences" (the document sent or null), "files" [...], "status"}`. `status` is `ok`, or why the sample was not made (`stuck: <path>: <message>` when no seed could be composed, `refused: <path>: <message>` for a request the API refused); such a sample has no files and the command goes on. A style can draw a motif too long for its theme; that sample is stuck and the next one is made.

Exit status: 0 when at least one sample was written; 1 for a bad option, a settings file outside its limits or an unreadable file; 2 when no sample could be composed; 3 when a file could not be written.

## A listening round

A round makes a batch, listens, changes one setting and makes the same batch again.

1. Build: `cmake --build build --target ursatz`.
2. A style from a folder of MIDI or MusicXML files: `ursatz corpus <folder> --out <dir>`, then `ursatz style-profile <dir> --name <n> --out <n>.style.json`.
3. The defaults as a settings file: `ursatz listen --print-settings > settings.json`; keep the members the round changes and delete the others.
4. Make the batch: `ursatz listen --what themes --count 10 --seed 1 --style <n>.style.json --settings settings.json --out-dir round-1`.
5. Notation text for each sample: `ursatz analyze round-1/<i>.musicxml --view reading`.
6. Audio is rendered by the session, not by the repository: for example FluidSynth with a General MIDI soundfont, `fluidsynth -ni <soundfont>.sf2 round-1/<i>.mid -F round-1/<i>.wav -r 44100`.
7. Change one setting in `settings.json` and make `round-2` with the same `--seed`: samples with the same number come from the same seed, so the difference between `round-1/03` and `round-2/03` is the setting.

Seed 2 of [`tests/fixtures/theme/one-motive.json`](../../composer/tests/fixtures/theme/one-motive.json) is a theme whose heard text changes with `melody.attempts` 1, `keep` 1 and `weights.against_bass` 20 (`--what themes --theme-request tests/fixtures/theme/one-motive.json --seed 2 --settings settings.json`).

## Data mode: a listening session

With `--session <ses-id>` (and `--round <n>` and `--batch <name>`, which are refused without it; `--taste <name>` is optional) `listen` works with the data folder (`--data`): it opens the workshop on it, reads the session's taste (`ratings.session`) and, for each sample, asks `settings.resolve` (kind `motif` or `theme`, the taste of the flag or of the session, the `--settings` document as the request scope) for the settings the sample is made with; a null document sends none. The samples the share of the resolved `explore.share` reserves explore: sample i (from 0) explores when floor((i + 1) * share) > floor(i * share), moving `explore.distance` settings away from the taste with the sample's seed as the exploration seed. Files and `listen.jsonl` are written as always (the manifest's `preferences` is the document sent). After the last sample one `ratings.batch` records each sample's index, seed, status, files (`{name, sha256}`) and resolution, so `rate` and `ratings.keep` can cite the batch ([ratings](../../workshop/docs/ratings.md)). A batch that is already recorded is refused. Without `--session` the run is as it was, even with `URSATZ_DATA` set.

## A varied batch

With `candidates.variety` above 0 in the settings file (`{"candidates": {"variety": 20}}`, [preferences](../../composer/docs/preferences.md#the-candidates-group-r-673)) a motif batch is chosen for being different: each sample's request carries `avoid`, the motives of the earlier samples that were written (the last 64), so each motif is the farthest of its candidates from the ones before it. Samples are still made from `--seed` plus their number, and a batch with no variety sends no `avoid` and is as it was. Themes made from a generated motif take it too (R-722): each theme's motif is drawn away from the motifs of the themes before it, which need `--what themes` without `--motif` or `--theme-request`; a theme made from a motif or a request you gave is as it was. A varied draw can be a motif a theme refuses (a pickup that makes it 3 bars); that sample is reported as stuck. With `critic.rank` on in the settings ([the critic](../../composer/docs/critic.md#ranking-theme-candidates-r-722)) each theme is the best of its candidates and its JSON holds the critic's judgment and the rejected candidates.

## A settings file for the L4 settings

Every L4 setting is off by default, so a settings file turns on only the ones a round compares (R-422). This file weighs the closure of every cadence, caps the bars in a row with one rhythm at 3, weighs against leaps and for a steady register, and lets a period's contrasting idea be a distant rhythm or new material as often as a similar one:

```json
{
  "melody": {
    "closure": {"final": 20, "first": 10},
    "rhythm_repeat_bars": 3,
    "leaps": 10,
    "register": 10
  },
  "development": {
    "contrast": {"distance": {"similar": 1, "distant": 1}, "new_material": 1}
  }
}
```

All of them reach themes (`--what themes`); each is a setting of the `melody` or `development` group, described in [preferences.md](../../composer/docs/preferences.md).

## A settings file for the contrast target

`development.functions.contrast` draws each contrasting idea of a period as a distant contrast that is at least half unlike the basic idea in rhythm and in contour (R-710), or drops the structure. It reaches themes (`--what themes`) and is off by default, like every function setting:

```json
{"development": {"functions": {"contrast": true}}}
```

## A settings file for the surface and the cadence

`development.functions.surface` draws a sentence's fragments from the densest segment of the motif and another, and `melody.cadence_long` holds the last note of every idea that ends a cadence from its last downbeat to its end (R-712). Both reach themes and are off by default; with `development.functions.harmony` and `.contrast` they are the settings of a theme quality round:

```json
{"development": {"functions": {"surface": true}}, "melody": {"cadence_long": true}}
```

## A settings file for the L3 settings

Every L3 setting is off by default, so a settings file turns on only the ones a round compares (R-422). This file syncopates one cell in two with an off-beat note or an anticipation at the eighth level, allows at most two skips, gives the plan `ii V@1:3` (a ii chord, then V from bar 1 beat 3), and turns on the accelerating continuation and the basic idea as written for a theme:

```json
{
  "motif": {
    "syncopation_one_in": 2,
    "syncopation_types": {"off_beat": 1, "tie_beat": 0, "tie_bar": 0, "anticipation": 1},
    "syncopation_levels": {"eighth": 1, "sixteenth": 0},
    "skips_per_motif": 2,
    "plan": ["ii", "V@1:3"]
  },
  "development": {
    "continuation": {"every_segment": true, "fragment_lengths": {"segment": 1, "half_bar": 1, "beat": 1},
                     "diminish": true, "liquidate": true, "faster_harmony": true},
    "basic_as_written": true
  }
}
```

The `motif` group reaches motifs (`--what motifs`); the `development` group reaches themes. A plan that does not fit the meter (`["I", "vi", "ii"]` in 3/4) is not a usage error: each sample is a manifest line `refused: /preferences/motif/plan: ...`, and the command exits 2 when none was written. The settings are described in [preferences-motif.md](../../composer/docs/preferences-motif.md) and [preferences.md](../../composer/docs/preferences.md).
