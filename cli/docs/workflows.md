# Workflows

The commands of `ursatz` in the order a piece is made, with the options that work across commands. Back to the [cli docs](README.md).

## A piece from a plan

```sh
ursatz plan --form ternary --key A --material minor --meter 3/4 --seed 4 --out piece.json
ursatz fill piece.json --seed 7 --midi piece.mid --musicxml piece.xml
ursatz lock piece.json --section A --out piece-locked.json
ursatz regenerate piece-locked.json --section B --seed 3 --out piece-2.json --midi piece-2.mid
ursatz render piece-2.json --midi again.mid --tempo 96
```

`plan` writes the plan of a short piece from a form template; `fill` realizes any plan ([cli-plan.md](cli-plan.md), [cli-fill.md](cli-fill.md)); `lock` makes a section, a phrase or one lane Given and `regenerate` writes one node again and only what depends on it ([cli-regenerate.md](cli-regenerate.md)); `render` writes the plan's Given and Realized notes as MIDI, MusicXML or both ([export.md](../../composer/docs/export.md)). `variations` and `harmonize` keep several versions of a node that sound different ([cli-variations.md](cli-variations.md)).

## Changing a piece in a data folder

A piece or theme in a [data folder](data-folder.md) is read as text and composed again with commands written as text:

```sh
ursatz --data ~/ursatz-data show item:pce-2c8d54834a9c499f --view structure
ursatz --data ~/ursatz-data apply compose.txt --wait
```

`compose.txt` holds one command a line (see [apply](data-folder.md#apply) and the [command reference](../../workshop/docs/command-reference.md)); `--wait` waits for the job `item.compose` starts and prints the version it made:

```
# compose the current version again with another seed
item.compose item:pce-2c8d54834a9c499f@v2 seed 3
```

## MIDI files

A MIDI file opens in a DAW with named tracks (`melody`, `bass`, `inner_voice_<n>`, the form on the conductor track), a
marker at the start of every section (`A`, `B`, `A'`) and, with `--tempo <bpm>` on any command that writes `--midi` or
`--musicxml`, a tempo; `--plain-midi` turns the names and markers off. See [export.md](../../composer/docs/export.md).

## Middle-out, checking and style

A piece can start from the middle: `plan --given-melody phrase.txt --at B:1` (also `--given-progression "I IV V I"` and
`--given-bass`) places a phrase, a progression or a bass line anywhere in the plan, `fill` grows the form around it and
never changes it, and the result says so. See [middle-out.md](../../composer/docs/middle-out.md).

One analysis path: what a listener finds in the written music (the form, the cadences, the key, the texture, the style
distance) comes from the Ursatz Analyzer's result over one render of the plan per run; the checks of the plan's own planned
chords and lines stay framework rule calls (see [form-hear.md](../../composer/docs/form-hear.md)).

`fill --check` and `grow --check` send each composed theme section to the Ursatz Analyzer in memory and reject a run
whose analysis contradicts the plan (a planned cadence or the key not heard, a hard voice-leading fault);
`fill --candidates <n>` composes `n` candidates and writes the one the Analyzer agrees with most. See
[analyzer-check.md](../../composer/docs/analyzer-check.md).

`plan --style`, `fill --style` and `grow --style` take a style model file the Ursatz Analyzer wrote from a corpus. The harmony and
melody stages then prefer the candidates closest to it, and the result prints the Analyzer's distance from the style. A style
is a preference, never a rule. `--blend <file>` with `--blend-weight` or `--blend-weights harmony=1/2,form=0,...` mixes two
styles, with a share for each of harmony, rhythm, melody and form. See [style.md](../../composer/docs/style.md).

## From a corpus

A piece can also start from a corpus. End to end, with the Ursatz Analyzer (`ursatz corpus` and `ursatz style-profile`):

```sh
# 1. analyze a folder of MIDI or MusicXML files
ursatz corpus chorales --out chorales-out
ursatz corpus sonatinas --out sonatinas-out
# 2. profile it: what its harmony, rhythm, melody and form are like
ursatz style-profile chorales-out --name chorales --out chorales.style.json
ursatz style-profile sonatinas-out --name sonatinas --out sonatinas.style.json
# 3. compose in it, or in a blend of two
ursatz plan --form binary --key C --meter 4/4 --style chorales.style.json --out piece.json
ursatz plan --form binary --key C --meter 4/4 --style chorales.style.json --blend sonatinas.style.json \
    --blend-weights harmony=1/4,melody=3/4 --out piece.json
ursatz fill piece.json --seed 3 --midi piece.mid
# 4. start from material in the corpus similar to a passage
ursatz grow --from-corpus sonatinas-out --like passage.mid --into sentence --key "D major" --meter 4/4 --seed 3
ursatz plan --form rounded-binary --key "D major" --meter 4/4 --from-corpus sonatinas-out --like passage.mid \
    --like-progression "I IV V I" --at B --out piece.json
```

`--from-corpus <dir> --like <file>` takes one bar of a similar passage as the motive (the seed picks among the best); on `plan`,
`--like-progression "<numerals>"` places the chords of the best matching passage like a `--given-progression`. The source
piece, measures and score are printed and recorded in the plan. The chords come from the Analyzer's joined chord sequence, and `Warning: N stale documents` goes to stderr when the corpus was analyzed by another version. See [style.md](../../composer/docs/style.md).
