# Command line

The `ursatz-composer` commands with one example each, and the options that work across commands. Back to the [docs index](README.md).

## Commands

Each command has a page with its options and output; every command is a client of the [composer API](api.md).

```sh
ursatz-composer --version
ursatz-composer export plan.json --midi out.mid --musicxml out.xml
```

`export` writes a plan's Given and Realized notes as MIDI, MusicXML or both ([export.md](export.md)).

```sh
ursatz-composer progression --key "D minor" --meter 4/4 --bars 8 --cadence PAC --seed 7
```

`progression` writes a chord progression with a bass line for one phrase ([cli-progression.md](cli-progression.md)).

```sh
ursatz-composer motive theme.txt --meter 4/4 --key "D minor"
```

`motive` loads a motive, prints its analysis and renders it in any key ([cli-motive.md](cli-motive.md)).

```sh
ursatz-composer develop theme.txt --op restate --tonal --meter 4/4 --key "D minor" --suggest 3
```

`develop` makes motives related to a motive and scores how recognizable each is ([cli-develop.md](cli-develop.md)).

```sh
ursatz-composer variants theme.txt --meter 4/4 --key "D minor" --count 4 --seed 7
```

`variants` makes variants of a motive from chains of registry operations; `variants --list-ops` lists the operations ([cli-variants.md](cli-variants.md)).

```sh
ursatz-composer recognize theme.txt variant.txt --meter 4/4 --key "D minor"
```

`recognize` names the chain of registry operations that turns one motive into another ([cli-recognize.md](cli-recognize.md)).

```sh
ursatz-composer random-motive --mode minor --meter 3/4 --count 4 --seed 7
```

`random-motive` makes random motives from a mode and a meter ([cli-random-motive.md](cli-random-motive.md)).

```sh
ursatz-composer counterpoint line.txt --species 1 --above --key "D minor" --meter 4/4 --seed 7
```

`counterpoint` writes a counter-melody against a given line in the first to fourth species ([cli-counterpoint.md](cli-counterpoint.md)).

```sh
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --seed 7 --out theme.json --midi theme.mid
```

`grow` grows a theme from a motive: a sentence, a period or one of the seven hybrids ([cli-grow.md](cli-grow.md)). `--accompaniment` and `--texture` add an accompaniment in the inner voices; `--voicing` and `--texture chorale` voice the harmony in four parts.

```sh
ursatz-composer theme request.json --seed 7 --out theme.json --midi theme.mid
```

`theme` grows a theme from the motives of a request ([cli-theme.md](cli-theme.md), [theme-request.md](theme-request.md)).

```sh
ursatz-composer piece request.json --seed 7 --check --out loop.json --ending-out ending.json --midi loop.mid --ending-midi ending.mid
```

`piece` composes a looping game track from a request, as a loop and an ending ([cli-piece.md](cli-piece.md), [piece-request.md](piece-request.md)).

```sh
ursatz-composer plan --form ternary --key A --mode minor --meter 3/4 --seed 4 --out piece.json
ursatz-composer fill piece.json --seed 7 --midi piece.mid --musicxml piece.xml
```

`plan` writes the plan of a short piece from a form template; `fill` realizes any plan ([cli-plan.md](cli-plan.md), [cli-fill.md](cli-fill.md)).

```sh
ursatz-composer lock piece.json --section A --out piece-locked.json
ursatz-composer regenerate piece-locked.json --section B --seed 3 --out piece-2.json --midi piece-2.mid
```

`lock` makes a section, a phrase or one lane Given; `regenerate` writes one node again and only what depends on it ([regenerate.md](regenerate.md), [cli-regenerate.md](cli-regenerate.md)).

```sh
ursatz-composer variations piece.json --section B --count 4 --seed 3 --out-dir variations --midi
ursatz-composer harmonize piece.json --section B --count 4 --seed 1 --out-dir harmonizations --musicxml
```

`variations` writes a node again with other seeds and keeps several versions that sound different; `harmonize` does the same for the harmony of a section whose melody is given ([cli-variations.md](cli-variations.md)).

```sh
ursatz-composer listen --what themes --count 10 --seed 1 --settings settings.json --out-dir round-1
```

`listen` makes a batch of motifs or themes from fixed seeds and a settings file and writes MIDI, MusicXML and JSON files for a listening session ([cli-listen.md](cli-listen.md)).

## MIDI files

A MIDI file opens in a DAW with named tracks (`melody`, `bass`, `inner_voice_<n>`, the form on the conductor track), a
marker at the start of every section (`A`, `B`, `A'`) and, with `--tempo <bpm>` on any command that writes `--midi` or
`--musicxml`, a tempo; `--plain-midi` turns the names and markers off. See [export.md](export.md).

## Middle-out, checking and style

A piece can start from the middle: `plan --given-melody phrase.txt --at B:1` (also `--given-progression "I IV V I"` and
`--given-bass`) places a phrase, a progression or a bass line anywhere in the plan, `fill` grows the form around it and
never changes it, and the result says so. See [middle-out.md](middle-out.md).

One analysis path: what a listener finds in the written music (the form, the cadences, the key, the texture, the style
distance) comes from the Ursatz Analyzer's result over one render of the plan per run; the checks of the plan's own planned
chords and lines stay framework rule calls (see [form-hear.md](form-hear.md)).

`fill --check` and `grow --check` send each composed theme section to the Ursatz Analyzer in memory and reject a run
whose analysis contradicts the plan (a planned cadence or the key not heard, a hard voice-leading fault);
`fill --candidates <n>` composes `n` candidates and writes the one the Analyzer agrees with most. See
[analyzer-check.md](analyzer-check.md).

`plan --style`, `fill --style` and `grow --style` take a style model file the Ursatz Analyzer wrote from a corpus. The harmony and
melody stages then prefer the candidates closest to it, and the result prints the Analyzer's distance from the style. A style
is a preference, never a rule. `--blend <file>` with `--blend-weight` or `--blend-weights harmony=1/2,form=0,...` mixes two
styles, with a share for each of harmony, rhythm, melody and form. See [style.md](style.md).

## From a corpus

A piece can also start from a corpus. End to end, with the Ursatz Analyzer (`ursatz-analyzer`, built with the Composer):

```sh
# 1. analyze a folder of MIDI or MusicXML files
ursatz-analyzer --corpus chorales/ --out chorales-out
ursatz-analyzer --corpus sonatinas/ --out sonatinas-out
# 2. profile it: what its harmony, rhythm, melody and form are like
ursatz-analyzer --style-profile chorales-out --name chorales --out chorales.style.json
ursatz-analyzer --style-profile sonatinas-out --name sonatinas --out sonatinas.style.json
# 3. compose in it, or in a blend of two
ursatz-composer plan --form binary --key C --meter 4/4 --style chorales.style.json --out piece.json
ursatz-composer plan --form binary --key C --meter 4/4 --style chorales.style.json --blend sonatinas.style.json \
    --blend-weights harmony=1/4,melody=3/4 --out piece.json
ursatz-composer fill piece.json --seed 3 --midi piece.mid
# 4. start from material in the corpus similar to a passage
ursatz-composer grow --from-corpus sonatinas-out --like passage.mid --into sentence --key "D major" --meter 4/4 --seed 3
ursatz-composer plan --form rounded-binary --key "D major" --meter 4/4 --from-corpus sonatinas-out --like passage.mid \
    --like-progression "I IV V I" --at B --out piece.json
```

`--from-corpus <dir> --like <file>` takes one bar of a similar passage as the motive (the seed picks among the best); on `plan`,
`--like-progression "<numerals>"` places the chords of the best matching passage like a `--given-progression`. The source
piece, measures and score are printed and recorded in the plan. The chords come from the Analyzer's joined chord sequence, and `Warning: N stale documents` goes to stderr when the corpus was analyzed by another version. See [style.md](style.md).
