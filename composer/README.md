# Ursatz-Composer

A seeded, reproducible composing partner. Every result carries its provenance: the seed, the
inputs and the generators that produced it. A piece is a plan that can be filled top-down,
bottom-up or middle-out.

It is a C11 static library (`ursatz_composer`) plus a CLI (`ursatz-composer`), built on
[Ursatz](https://github.com/Mitchell-Opitz/Ursatz) and
[Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer). Both are git submodules
under `external/`. Style-dependent generators live in this repo; Ursatz keeps only the
theory-neutral Generator interface.

Intents are structured (CLI flags or a JSON request), never natural language.

## Build

Requires CMake 3.20+, a C11 compiler, pkg-config and SQLite3 (for Ursatz).

```sh
git clone --recurse-submodules https://github.com/Mitchell-Opitz/Ursatz-Composer.git
cd Ursatz-Composer
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -LE slow
```

The tests labeled `slow` (the compose and heard sweeps of the form templates, about 3 minutes in all when run in
parallel) are left out of that command and of CI. Run them all, with plain `ctest --test-dir build -j8
--output-on-failure`, before pushing a branch.

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive`.

### Submodules

| Path | Pinned at |
| --- | --- |
| `external/ursatz` | Ursatz `main` at PR #142, the P1 analysis library (Framework v1). |
| `external/ursatz-analyzer` | Ursatz-Analyzer `main` at PR #11. |

## Structure

| Directory | Holds |
| --- | --- |
| `include/ursatz_composer/` | The public headers. |
| `src/plan/` | The piece plan, its lanes, states, constraints and JSON. |
| `src/render/` | Rendering a plan to NoteEvents, MIDI and MusicXML. |
| `src/fill/` | The fill loop, its operators, seeds and records. |
| `src/harmony/` | The harmony operators and hearing cadences. |
| `src/motive/` | Motives, their analysis and development. |
| `src/melody/` | The melody operator. |
| `src/form/` | Hearing the phrase form through standard-form. |
| `src/counterpoint/` | The counter-melody operator: species 1 to 4 against a given line, its search and its check. |
| `src/texture/` | The accompaniment and four-part voicing operators, and hearing the voice leading. |
| `src/piece/` | Form templates, the piece plan and the compose run over its sections. |
| `src/grow/` | Growing a theme from a motive: the plan, the structure operator, its checks and the stages. |
| `cli/` | The `ursatz-composer` command. |
| `tests/` | Tests and their fixtures. |

## Usage

```sh
ursatz-composer --version
ursatz-composer --help
ursatz-composer export plan.json --midi out.mid --musicxml out.xml
```

`export` loads a plan from JSON and writes its Given and Realized notes as a MIDI file, a
MusicXML file, or both, through Ursatz's renderer and exporter. Spans that are not written yet are
listed on stderr. Filling a plan is not implemented yet.

```sh
ursatz-composer progression --key "D minor" --meter 4/4 --bars 8 --cadence PAC --seed 7
```

`progression` writes a chord progression with a bass line for one phrase. See
[docs/cli-progression.md](docs/cli-progression.md).

```sh
ursatz-composer motive theme.txt --meter 4/4 --key "D minor"
```

`motive` loads a motive from MIDI, MusicXML, a note list or JSON and prints its analysis. See
[docs/cli-motive.md](docs/cli-motive.md).

```sh
ursatz-composer develop theme.txt --op restate --tonal --meter 4/4 --key "D minor" --suggest 3
```

`develop` makes motives related to a motive (restatements, fragments, rhythmic variants) and scores
how recognizable each is. See [docs/cli-develop.md](docs/cli-develop.md).

```sh
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --seed 7 --out theme.json --midi theme.mid
```

`grow` grows a theme from a motive: a sentence, a period or one of the seven hybrids, with its harmony,
bass and melody, written as JSON, MIDI and MusicXML, and prints the form standard-form hears in it.
With it the Composer reaches milestone M1: a motive, a key and a meter in, harmonized phrases out. See
[docs/cli-grow.md](docs/cli-grow.md). `--accompaniment` and `--texture` add an accompaniment in the inner voices; `--voicing` and `--texture chorale` voice the
harmony in four parts.

```sh
ursatz-composer plan --form ternary --key A --mode minor --meter 3/4 --seed 4 --out piece.json
ursatz-composer fill piece.json --seed 7 --midi piece.mid --musicxml piece.xml
```

`plan` writes the plan of a whole short piece from a form template (binary, rounded binary, ternary, variations, loop
or layered): its sections, their keys and cadences and the sections that return an earlier one. `fill` realizes any
plan, one from `plan` or from `grow --out`, and writes the piece as JSON, MIDI and MusicXML with the form, the
cadences and the returns heard in it. See [docs/cli-plan.md](docs/cli-plan.md) and [docs/cli-fill.md](docs/cli-fill.md).

A piece can start from the middle: `plan --given-melody phrase.txt --at B:1` (also `--given-progression "I IV V I"` and
`--given-bass`) places a phrase, a progression or a bass line anywhere in the plan, `fill` grows the form around it and
never changes it, and the result says so. See [docs/middle-out.md](docs/middle-out.md).

`fill --check` and `grow --check` send each composed theme section to the Ursatz Analyzer in memory and reject a run
whose analysis contradicts the plan (a planned cadence or the key not heard, a hard voice-leading fault);
`fill --candidates <n>` composes `n` candidates and writes the one the Analyzer agrees with most. See
[docs/analyzer-check.md](docs/analyzer-check.md).

`plan --style`, `fill --style` and `grow --style` take a style model file the Ursatz Analyzer wrote from a corpus. The harmony and
melody stages then prefer the candidates closest to it, and the result prints the Analyzer's distance from the style. A style
is a preference, never a rule. See [docs/style.md](docs/style.md).

```sh
ursatz-composer lock piece.json --section A --out piece-locked.json
ursatz-composer regenerate piece-locked.json --section B --seed 3 --out piece-2.json --midi piece-2.mid
```

With `lock` and `regenerate` the Composer reaches milestone M2: a whole short piece planned from either end, then taken
apart node by node. `lock` makes a section, a phrase or one lane Given; `regenerate` writes one node again with a seed
and writes again only what depends on it, so locked material is never changed. See
[docs/regenerate.md](docs/regenerate.md) and [docs/cli-regenerate.md](docs/cli-regenerate.md).

## Plan format

A plan is written and read as JSON in the shape of an Ursatz-Analyzer analysis document. See
[docs/plan-format.md](docs/plan-format.md).

## Fill loop

The engine that fills a plan with registered operators, seeded and recorded, with a suggestion mode.
See [docs/fill-loop.md](docs/fill-loop.md).

The melody operator writes a melody over a written harmony and bass, from a motive. See
[docs/melody-fill.md](docs/melody-fill.md).

## Accompaniment

The accompaniment operator writes block chords, arpeggios, Alberti figures or oom-pah under a written melody and bass,
chosen by pattern or by the texture it is to be heard as. See [docs/accompaniment.md](docs/accompaniment.md).

## Voicing

The voicing operator voices a written progression in four parts, soprano, alto, tenor and bass, by the standard voicing
rules with smooth voice leading and no parallels, and `voice_hear` hears the voice leading of a written plan. See
[docs/voicing.md](docs/voicing.md).

## Two-part counterpoint

The counterpoint operator writes a counter-melody against a given line in the first to fourth species, checked by the
standard-counterpoint rules; `ursatz-composer counterpoint` runs it from the command line. See
[docs/counterpoint.md](docs/counterpoint.md) and [docs/cli-counterpoint.md](docs/cli-counterpoint.md).

## Growing a theme

`grow_build_plan` and `grow_run` (`<ursatz_composer/grow.h>`) grow a theme of 8 or 4 bars from a motive, as a
sentence, a period or a hybrid: its structure, motives, harmony, bass and melody, in stages (a compose run over one section). See
[docs/grow.md](docs/grow.md).

## Pieces from form templates

`piece_build_plan` and `compose_run` (`<ursatz_composer/piece.h>`, `<ursatz_composer/compose.h>`) plan a short piece from a
named form (binary, rounded binary, ternary, theme and variations, loop, layered), invent a motive when none is given,
and fill every section with the grow stages. See [docs/piece.md](docs/piece.md). `plan_give_melody`,
`plan_give_progression` and `plan_give_bass` (`<ursatz_composer/given.h>`) place given material in such a plan, and the
stages fill around it ([docs/middle-out.md](docs/middle-out.md)).

## Hearing the form

`form_hear` hears the phrase-level form of a written plan with standard-form's analysis adapter. See
[docs/form-hear.md](docs/form-hear.md).
