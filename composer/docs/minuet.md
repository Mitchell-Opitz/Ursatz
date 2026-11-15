# The minuet and trio

The built-in `minuet`, `trio` and `minuet-and-trio` forms after Schoenberg, *Fundamentals of Musical Composition*, ch. XV (R-407, R-485 to R-488, P48 B5). Back to the [docs index](README.md). The forms are data (`forms/minuet.json`, `forms/trio.json`, `forms/minuet-and-trio.json`, read like any form document, [forms.md](forms.md)); what makes them possible is in [form-performance.md](form-performance.md): written-out repeats, a section that leads into the next, a connective, compound forms, and a form's meters and tempo.

## The forms

A minuet is in 3/4 at a moderate tempo with harmony that changes every one or two bars. Its form is ||: A :||: B A1 :||. A is a sentence (a presentation and a continuation) closed in the tonic or modulating to V (or III in minor). B contrasts with it, over a dominant pedal, and ends on the upbeat chord V, leading into A1, sometimes with a short stepwise connective. A1 is a varied repetition of A with its cadence changed. The trio is a second minuet in the tonic minor or major, IV, vi, III or VI, with a motive of its own and a period for A; then the minuet is played again without its repeats.

| Form | Sizes | What it is |
| --- | --- | --- |
| `minuet` | 12, **24** | A, B, A return, a third of the bars each. A: presentation + continuation, PAC, closed, ending in `away` with `modulate`. B: presentation + continuation, HC, inverted motive, `"pedal": "dominant"`, `"leads_in": true`, `"connective": "sometimes"`, starting in `away`. A return: returns A `varied`. `"meters": ["3/4"]`, `"tempo": 112`, `"repeats": [["A"], ["B", "A return"]]`. `away` is V or iii in major, III or v in minor. |
| `trio` | 12, **24** | The minuet with antecedent + consequent (a period) for A and A return. |
| `minuet-and-trio` | 12, **24** | A compound form: `Minuet` (form `minuet`), `Trio` (form `trio`, key from the region `trio`, a motive of its own), `Minuet da capo` (again `Minuet`, no repeats). The region `trio` is the tonic's parallel key (degree 1), IV and vi in major, and the parallel key, III and VI in minor. |

Composed once and performed in order (R-485): a minuet of 24 bars is performed A A B A1 B A1, 48 bars; the minuet and trio at 24 is 48 + 48 + 24 = 120 bars (60 at 12). The performed plan is what the piece version renders (plan JSON, MIDI, MusicXML) and what its rows list: 6 rows for a minuet (the ids are the source sections', `A`, `A`, `B`, `A return`, `B`, `A return`; the plan's section names are `A`, `A (repeat)`, `B`, `A return`, `B (repeat)`, `A return (repeat)`), 15 for the minuet and trio (the plan names `Trio A`, `Minuet da capo A`). The tempo is 112 quarters a minute unless the request or its style gives one (`tempo` `from` `form` in the brief). A request in another meter is refused at `/request/meter` (`the Minuet form is in 3/4`). The composed plans and their compose outcomes give the hearing: the rows' `heard`, `key_heard` and `changes` come from them.

The trio is composed as a piece of its own form in its own home key, with its own invented motive (the keys are drawn first from the seed, each part is composed from the seed plus 40 times its index). It is not sections of one plan in another key: a section that opens in a new key at its first bar is read by grow as a pivot modulation, and with that bypassed the varied return of the trio was refused in every attempt.

## Composed, not heard

The pedal, the region, the upbeat chord and the connective are composed and recorded in the plan; the hearing does not find them (R-408). B's last chord is the dominant (V or V7) and A return opens on the tonic: the join is read from the plan's chords, and from the Analyzer only as the half cadence heard at the end of B. The connective is a constraint with `written`; it is written when B's last bar can keep its downbeat note and walk by steps to the first note of A return (the rules are in [form-performance.md](form-performance.md#the-connective)).

## What the rows say (R-485, R-486, API 1.20)

The piece response lists one row for each performed section, and each row carries the checks of this page: `key_heard` (the trio's rows are heard in another key than the home key), `upbeat` (B's last chord, V or V7, and A return's first, I or i), `connective` (whether B's stepwise link was written), `repeat_of` (`A (repeat)` plays `A` again, `Minuet da capo A` plays `A` again) and `part` (`Minuet`, `Trio`, `Minuet da capo`), and `performed.bars` the length of the whole (48 for a minuet, 120 for a minuet and trio at 24). The forms entry serves the same members before a piece is composed: `meters`, `tempo`, `repeats`, `parts`, `performed`, and for each section `leads_in` and `connective`. A form that is not performed has `null` for `part` and `repeat_of` on every row.

## Later (R-488)

- An artificial dominant upbeat chord when the harmony of B does not end on V.
- A trio that modulates inside, and a transition between parts (P49 builds the liquidated one).
- Repeat signs and first and second endings in the notation (the repeats are written out).
- Regenerate and variations of a performed form (refused at `/request/form`).
- A reformulated A1: shortened or extended.

## Measured

Seeds 1 to 10, the API path (the CLI `piece` is the same entry), an invented motive, 3/4, up to 16 seeds each (the piece entry tries the next seeds when a run is stuck). Composed is the number of requests that composed. Not on the first seed counts the requests that needed another seed. Returns heard counts the runs where `return_hear` finds A return as A's return, of the returns planned (`compose_run` with the seeds the entry used; a compound form's parts are in the trio rows). The longest chord is in bars, over A, B and A return. Seconds a piece is the mean wall time of a request, retries included, on a shared 4-core container.

| Form, size, key, meter | Composed | Not on the first seed | Returns heard | Longest chord (bars) | Seconds a piece |
| --- | --- | --- | --- | --- | --- |
| minuet 24, C major 3/4 | 10 of 10 | 0 | 10 of 10 | 2 | 0.6 |
| minuet 24, C major 3/4, modulate | 10 of 10 | 0 | 9 of 10 | 2 | 0.6 |
| minuet 24, A minor 3/4 | 10 of 10 | 0 | 9 of 10 | 2 | 0.7 |
| minuet 24, A minor 3/4, modulate | 10 of 10 | 8 | 10 of 10 | 2 | 1.8 |
| minuet 24, D minor 3/4 | 10 of 10 | 0 | 10 of 10 | 2 | 0.6 |
| minuet 24, D minor 3/4, modulate | 10 of 10 | 8 | 10 of 10 | 2 | 1.7 |
| minuet 24, G major 3/4 | 10 of 10 | 1 | 10 of 10 | 2 | 0.7 |
| minuet 24, G major 3/4, modulate | 10 of 10 | 0 | 9 of 10 | 2 | 0.5 |
| minuet 12, C major 3/4 | 10 of 10 | 0 | 0 of 10 | 1 | 0.2 |
| minuet 12, C major 3/4, modulate | 10 of 10 | 8 | 0 of 10 | 1 | 0.5 |
| minuet 12, A minor 3/4 | 10 of 10 | 3 | 0 of 10 | 1 | 0.4 |
| minuet 12, A minor 3/4, modulate | 10 of 10 | 8 | 0 of 10 | 1 | 0.5 |
| minuet 12, D minor 3/4 | 10 of 10 | 0 | 0 of 10 | 1 | 0.2 |
| minuet 12, D minor 3/4, modulate | 10 of 10 | 8 | 0 of 10 | 1 | 0.6 |
| minuet 12, G major 3/4 | 10 of 10 | 0 | 0 of 10 | 1 | 0.2 |
| minuet 12, G major 3/4, modulate | 10 of 10 | 8 | 0 of 10 | 1 | 0.5 |
| trio 24, C major 3/4 | 10 of 10 | 0 | 10 of 10 | 2 | 0.6 |
| trio 24, C major 3/4, modulate | 10 of 10 | 0 | 5 of 10 | 2 | 0.7 |
| trio 24, A minor 3/4 | 10 of 10 | 1 | 9 of 10 | 2 | 0.8 |
| trio 24, A minor 3/4, modulate | 10 of 10 | 8 | 10 of 10 | 2 | 5.8 |
| minuet-and-trio 24, C major 3/4 | 10 of 10 | 0 | parts | 2 | 1.6 |
| minuet-and-trio 24, A minor 3/4 | 10 of 10 | 0 | parts | 2 | 1.4 |

The second table is read from the performed plan of each composed piece (the first of a section's performances; for the minuet and trio, the minuet's and the trio's sections each). B's last chord and A return's first are the last chord of B's span and the first of A return's. B heard HC and A return heard PAC are the rows' `heard`. A return's cadence chords changed counts the rows whose `changes` has different `cadence_from` and `cadence_to`. Connective written is the pattern the connective writes: B's last bar is a one-beat downbeat note and four eighths. The join is B's last melody note to A return's first.

| Form, size, key, meter | B's last chord | A return's first chord | B heard HC | A return heard PAC | A return's cadence chords changed | Connective written | Join at most 2 semitones |
| --- | --- | --- | --- | --- | --- | --- | --- |
| minuet 24, C major 3/4 | V 9, V7 1 | I 10 | 10 | 9 | 3 | 3 | 7 |
| minuet 24, C major 3/4, modulate | V 8, V7 2 | I 10 | 3 | 10 | 1 | 3 | 6 |
| minuet 24, A minor 3/4 | V 10 | i 10 | 10 | 9 | 9 | 3 | 6 |
| minuet 24, A minor 3/4, modulate | V 10 | i 10 | 10 | 10 | 10 | 10 | 10 |
| minuet 24, D minor 3/4 | V 10 | i 10 | 10 | 10 | 7 | 3 | 8 |
| minuet 24, D minor 3/4, modulate | V 10 | i 10 | 10 | 10 | 10 | 7 | 10 |
| minuet 24, G major 3/4 | V 10 | I 10 | 10 | 9 | 7 | 1 | 5 |
| minuet 24, G major 3/4, modulate | V 8, V7 2 | I 10 | 4 | 9 | 0 | 2 | 8 |
| minuet 12, C major 3/4 | V 10 | I 10 | 6 | 2 | 0 | 2 | 5 |
| minuet 12, C major 3/4, modulate | V 10 | I 10 | 3 | 0 | 0 | 6 | 10 |
| minuet 12, A minor 3/4 | V 10 | i 10 | 8 | 0 | 0 | 1 | 6 |
| minuet 12, A minor 3/4, modulate | V 10 | i 10 | 10 | 3 | 0 | 0 | 7 |
| minuet 12, D minor 3/4 | V 10 | i 10 | 7 | 0 | 0 | 1 | 3 |
| minuet 12, D minor 3/4, modulate | V 10 | i 10 | 10 | 0 | 0 | 0 | 3 |
| minuet 12, G major 3/4 | V 10 | I 10 | 10 | 1 | 0 | 3 | 4 |
| minuet 12, G major 3/4, modulate | V 10 | I 10 | 10 | 7 | 0 | 7 | 7 |
| trio 24, C major 3/4 | V 10 | I 10 | 10 | 4 | 4 | 3 | 7 |
| trio 24, C major 3/4, modulate | V 8, V7 2 | I 10 | 5 | 3 | 0 | 3 | 5 |
| trio 24, A minor 3/4 | V 10 | i 10 | 10 | 6 | 9 | 4 | 5 |
| trio 24, A minor 3/4, modulate | V 10 | i 10 | 10 | 7 | 10 | 10 | 10 |
| minuet-and-trio 24, C major 3/4 (minuet) | V 9, V7 1 | I 10 | 10 | 9 | 3 | 3 | 7 |
| minuet-and-trio 24, C major 3/4 (trio) | V 10 | i 5, I 5 | 10 | 5 | 5 | 5 | 7 |
| minuet-and-trio 24, A minor 3/4 (minuet) | V 10 | i 10 | 10 | 9 | 9 | 3 | 6 |
| minuet-and-trio 24, A minor 3/4 (trio) | V 10 | I 10 | 10 | 8 | 4 | 4 | 7 |

The trio key drawn, over the 10 seeds of each minuet and trio row: C major, A minor 3, C minor 2 and F major 5; A minor, A major 2, C major 5 and F major 3. Each is a key of the region `trio`, as heard at the first row of the trio (`key_heard`), never the home key.

Every row composes 10 of 10. 12 bars have sections of 4 bars: no return is heard and the longest chord is a bar, as for the 12-bar ternary ([small-ternary.md](small-ternary.md)). With `modulate`, 24 bars in A minor and D minor and every 12-bar row need another seed in 8 of 10 requests: the seed retry passes over the runs whose changed keys do not compose. Minuet 24 in G major composed 0 of 10 and F# major was the same before the bass operator's fallback (R-491, [grow-harmony.md](grow-harmony.md)): from G3 every line into the dominant's only D leaps or outlines a seventh. The ternary had the same limit in those two keys, in 3/4 and 4/4, and now composes 4 of 4 seeds in all 24 keys.

## Tests

| Test | What it checks |
| --- | --- |
| `test_form_spec` | The three rows of the built-in table (sizes, meters, tempo, repeats, parts) and their parse asserts: sections, keys, pedal, lead-in and connective, the minuet's sentence against the trio's period, the parts of the compound form. |
| `test_form_plans_templates` | The plans of the minuet and the trio at both sizes in C major and A minor 3/4 (a form with meters is recorded in its first meter; a compound form is not hashed, its parts are). |
| `test_form_templates_minuet*`, `test_form_templates_trio*` | The structure table, the smoke run in C major 3/4, and the compose and heard baselines (35, 105, 30 and 36, 108, 45; `slow`). |
| `test_api_forms` | The minuet's `meters`, `tempo`, `repeats`, `performed`, B's `leads_in` and `connective`, the ternary's empty ones and the minuet and trio's `parts`; the forms list has 10 names in registry order; the minuet, the trio and the minuet and trio entries. |
| `test_api_piece` | The rows' `part`, `repeat_of`, `upbeat`, `connective` and `performed.bars` (minuet 48, minuet and trio 120, ternary all null). A minuet request (version 2, C major 3/4, seed 1) answers 6 rows over 48 bars with tempo 112 in the brief and the MIDI file; in 4/4 it is refused at `/request/meter`; a `minuet-and-trio` request answers 15 rows over 120 bars, the trio's first row not in the home key; regenerate of a performed form is refused. |
| `test_piece_keys_minuet`, `test_piece_keys_trio` | The key plans and the composed runs in 3/4 (20 of 20 complete; `slow`). |
