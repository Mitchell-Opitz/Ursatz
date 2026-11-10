# listen command

The `listen` command of the Composer command line: a batch of motifs or themes from fixed seeds and one settings file, written as files a listening session plays and reads. Back to the [docs index](README.md).

```sh
ursatz-composer listen --what motifs|themes --out-dir <dir> [--count <n>] [--seed <n>] [--settings <file>] [--mode major|minor] [--meter <n/m>] [--style <file>] [--blend <file>] [--blend-weight <n/d>] [--blend-weights <list>] [--tempo <bpm>] [--strategy segments|drawn] [--into sentence|period] [--development melody|balanced|theme] [--bars 4|8] [--check] [--motif <file> | --theme-request <file>]
ursatz-composer listen --print-settings
```

`listen` is a client of the API ([api.md](api.md)): it calls `composer_api_random_motive`, `composer_api_motive`, `composer_api_motive_render` and `composer_api_theme` and writes what they answer. It plays nothing and writes no notation text; the session does both (R-404, R-412). With the same options, settings and seed, every file is byte-identical, so a round can be made again after one setting changes and the two rounds compared.

## Options

| Option | Meaning |
| --- | --- |
| `--what motifs\|themes` | What each sample is (required). |
| `--out-dir <dir>` | Where the files are written (required). The directory is created, with its parents. |
| `--count <n>` | How many samples, 1 to 64 (default 10). |
| `--seed <n>` | Whole number from 0 (default 1). Sample `i`, counted from 0, uses the seed `--seed` + `i`. |
| `--settings <file>` | A preferences document ([preferences.md](preferences.md)); every member is optional. Absent: no `preferences` member is sent and the Composer's defaults apply. A member outside its limits exits 1 with its path (`/melody/keep`). |
| `--print-settings` | Print the document of the defaults, as the vocabulary serves it, and exit 0. Edit a copy of it into a settings file. |
| `--mode major\|minor` | Default major. |
| `--meter <n/m>` | Default 4/4. With `--motif`, the meter the motif is read in; absent, the motif's own. |
| `--style <file>` | A style model file ([style.md](style.md)). Motifs: 8 candidates are drawn for each and the closest kept. Themes: the style is preferred. |
| `--blend <file>`, `--blend-weight`, `--blend-weights` | Themes: the second style model and its shares, as `grow`'s ([cli-grow.md](cli-grow.md)). Need `--style`. |
| `--tempo <bpm>` | The tempo of the MIDI and MusicXML files, 20 to 400. |
| `--strategy segments\|drawn` | How a generated motif is made (default `segments`). |
| `--into sentence\|period` | Themes: the pairing (default `sentence`). |
| `--development melody\|balanced\|theme` | Themes: how far the theme strays from the motif (default `balanced`). |
| `--bars 4\|8` | Themes: the length (default 8). |
| `--check` | Themes: pass over a seed whose analysis contradicts the plan. |
| `--motif <file>` | Themes: the motif of every sample, anything the [`motive`](cli-motive.md) command reads (a note list, MusicXML, MIDI, a motive document). It is read in key C and the mode. Absent: a generated motif for each sample, made with the sample's seed, the mode, meter, strategy, style and settings. |
| `--theme-request <file>` | Themes: a [theme request](theme-request.md), for example a GUI composed theme's `request.json`, with its `seed`, `suggest`, `midi`, `musicxml` and, when `--settings` is given, `preferences` replaced. `--style`, `--blend`, `--tempo` and `--check` replace theirs when given. |

`--motif` and `--theme-request` together exit 1. A theme built from a motif is a request of schema `ursatz-theme-request` version 2: key C, the mode, the meter, `--into`, `--bars`, `--development`, one motive `M1` (the motif's degrees, pickup, harmony and features, `form` full, role melodic) in the `basic` slot.

## Files

For sample `i` (two digits from 01), in `--out-dir`:

| File | Holds |
| --- | --- |
| `<i>.mid` | The motif or the theme (melody, bass and accompaniment) as MIDI. |
| `<i>.musicxml` | The same as MusicXML. |
| `<i>.json` | The API response of the sample. |
| `listen.jsonl` | One line per sample, appended. |

A line of `listen.jsonl`: `{"index", "seed", "what", "motif" ("generated" or the file path), "theme_request" (path or null), "style", "blend", "settings" (the file path or null), "preferences" (the document sent or null), "files" [...], "status"}`. `status` is `ok`, or why the sample was not made (`stuck: <path>: <message>` when no seed could be composed, `refused: <path>: <message>` for a request the API refused); such a sample has no files and the command goes on. A style can draw a motif too long for its theme; that sample is stuck and the next one is made.

Exit status: 0 when at least one sample was written; 1 for a bad option, a settings file outside its limits or an unreadable file; 2 when no sample could be composed; 3 when a file could not be written.

## A listening round

A round makes a batch, listens, changes one setting and makes the same batch again.

1. Build: `cmake --build build --target ursatz-composer ursatz-analyzer`.
2. A style from a folder of MIDI or MusicXML files: `ursatz-analyzer --corpus <folder> --out <dir>`, then `ursatz-analyzer --style-profile <dir> --name <n> --out <n>.style.json`.
3. The defaults as a settings file: `ursatz-composer listen --print-settings > settings.json`; keep the members the round changes and delete the others.
4. Make the batch: `ursatz-composer listen --what themes --count 10 --seed 1 --style <n>.style.json --settings settings.json --out-dir round-1`.
5. Notation text for each sample: `ursatz-analyzer export round-1/<i>.musicxml`.
6. Audio is rendered by the session, not by the repository: for example FluidSynth with a General MIDI soundfont, `fluidsynth -ni <soundfont>.sf2 round-1/<i>.mid -F round-1/<i>.wav -r 44100`.
7. Change one setting in `settings.json` and make `round-2` with the same `--seed`: samples with the same number come from the same seed, so the difference between `round-1/03` and `round-2/03` is the setting.

Seed 2 of [`tests/fixtures/theme/one-motive.json`](../tests/fixtures/theme/one-motive.json) is a theme whose heard text changes with `melody.attempts` 1, `keep` 1 and `weights.against_bass` 20 (`--what themes --theme-request tests/fixtures/theme/one-motive.json --seed 2 --settings settings.json`).
