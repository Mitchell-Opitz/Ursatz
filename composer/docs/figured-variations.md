# The figured variations

The built-in `figured-variations` form after Schoenberg, *Fundamentals of Musical Composition*, ch. XVII (R-407, R-497 to R-500, P50 B4). Back to the [docs index](README.md). The form is data (`forms/figured-variations.json`, read like any form, [forms.md](forms.md)); B1 to B3 added what it uses: the return variation `figurate` and its `figure`, the skeleton and the figures ([variation-figure.md](variation-figure.md)), the reharmonized figure with a root change, the climax, and a variation in the parallel mode. The older `variations` form (P44) is unchanged.

## The form

A theme of eight bars and variations that keep its course: each variation is a return of the theme (R-070), so its phrases, ideas, harmony, bass and proportions are the theme's, and the melody is rewritten from a skeleton of the theme's, each cell circumscribed by the figure of the variation.

| Form | Sizes | What it is |
| --- | --- | --- |
| `figured-variations` | 40 (count 3), **56** (count 5) | 2/4, 3/4, 4/4 and 6/8 (2/4 first), no tempo, `closing`, no regions. Theme, the variations, and a coda of two codettas: eight sections at 56 bars, six at 40, under `FORM_SPEC_MAX_SECTIONS` (9). |

The sections at 56 bars, and what each is for:

| Section | Role | Bars | Figure | What it is |
| --- | --- | --- | --- | --- |
| T | `theme` | 8 | none | The theme: a period (antecedent + consequent), PAC, closed, in the tonic. |
| V1 | `variation` | 8 | `neighbor`, eighths | The first variation: the skeleton circumscribed by neighbor tones, one cell a figure. The last bar of each half keeps the theme's notes: the figure gives way at the cadences. |
| V2 | `variation` | 8 | `neighbor`, sixteenths | The same figure in a shorter rhythm. V1 and V2 are a group: one idea elaborated in steps, as Schoenberg's grouped variations are. |
| V3 | `variation` | 8 | `scale`, eighths, `"mode": "parallel"` | The minore (or maggiore): the theme in the home tonic's other mode, mapped degree by degree (3 and 6 lowered into minor, raised into major) and reharmonized there. A change of mode on one tonic is not a modulation: no pivot, and the key check does not classify it. A scale line toward the next skeleton tone. |
| V4 | `variation` | 8 | `scale`, sixteenths, `reharmonized`, `change_at` 3 | The harmony varied: the figure sits over a reharmonized copy, and a chord that starts in bar 3 should differ from the theme's (Brahms's third bar; asked in the first half of the attempts). |
| V5 | `variation` | 8 | `broken-chord`, sixteenths, `climax` | The build to the climax: a broken chord of each cell's chord in the shortest rhythm, an octave higher when every note stays inside the melody range. |
| C1, C2 | `codetta` | 4 each | none | The coda: two codettas as the scherzo's (antecedent + cadential, C1 closed; compound basic idea + cadential), ending the piece in a PAC. |

At 40 bars (count 3) the set is the theme, V1 (neighbor, eighths), V2 (the parallel mode, scale, eighths) and V3 (the climax, broken chord, sixteenths), then C1 and C2. The planned key of each section is the key lane's span under it: the tonic everywhere, and the tonic's other mode under the parallel variation (so V3 of a piece in C major is C minor, and in A minor it is A major).

## Composed, not heard

The variations are composed to the plan, not heard back (R-408): `key_heard` is what the Analyzer hears, and the planning measure found the parallel variation heard in the home mode in 9 of 10 runs (R-493: heard keys stay unasserted). The tests check what the plan says: the keys of the sections, each figure against its kind (`form_figure_check`), the names and lengths of the phrases and ideas against the theme's, and the pieces compose.

## What was measured

At Composer `720ffba` plus B4, a scratch program outside the repository (`piece_build_plan` and `compose_run`, an invented motive, request seeds 1 to 10, each tried up to 16 times from the next seeds as the piece entry does; seconds are `compose_run` only, over all the tries of the ten seeds). Checked in every composed plan: the theme, the home variations and the codettas are in the home key and the parallel variation in the parallel key; `form_figure_check` holds for every variation; every variation's phrases and ideas have the theme's names and lengths at the same offsets; the highest melody note of the last variation against the rest of the piece when it was raised (a run counts as raised when its last, kept note is an octave over the theme's); the runs in which V4's bar 3 has a chord that is not the theme's (56 bars); and the variations that have a return record against the theme (`return_heard_find`).

| Size, key, meter | Composed | First try | Keys | Figures | Shapes | Climax raised | V4 bar 3 changed | Returns found | Mean seconds a piece |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 56, C major 2/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | 0 | 50 of 50 | 0.4 |
| 56, A minor 2/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | 1 | 50 of 50 | 0.7 |
| 56, G major 2/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | 0 | 50 of 50 | 0.3 |
| 56, D minor 2/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | 0 | 50 of 50 | 0.4 |
| 56, C major 3/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | 0 | 50 of 50 | 0.5 |
| 56, C major 4/4 | 10 of 10 | 7 | 10 | 10 | 10 | 0 | 0 | 50 of 50 | 2.3 |
| 56, C major 6/8 | 10 of 10 | 8 | 10 | 10 | 10 | 0 | 0 | 50 of 50 | 2.4 |
| 56, A minor 6/8 | 10 of 10 | 4 | 10 | 10 | 10 | 0 | 2 | 50 of 50 | 2.8 |
| 40, C major 2/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | n/a | 30 of 30 | 0.3 |
| 40, A minor 2/4 | 10 of 10 | 10 | 10 | 10 | 10 | 0 | n/a | 27 of 30 | 0.2 |

Every row composes 10 of 10. The climax was raised in none of the 100 composed pieces: the raise is all or nothing (every note an octave up must stay in the melody range), and in the runs inspected (40 bars, C major 2/4) the theme's top note, 79 to 84, was the last variation's too, so it had no higher one. The cause was not investigated further. The condition that its highest note is the highest of the piece whenever it was raised therefore holds without being exercised. V4's bar 3 changed in 3 of 80 runs (the fit check is in the first half of the attempts and the theme's harmony is one chord a bar in 2/4). Both are recorded, not asserted; the follow-up is in the project's notes.

The tests: `test_form_templates figured-variations` structure (both sizes in four scenarios: the sections, the figures, the parallel key of V3, refused sizes), `compose` (39 of 40 runs complete over seeds 1 to 5 in C major 4/4 and A minor 3/4 at both sizes, the one stuck at the returns stage; 272 sections heard, 126 heard as requested; the baselines) and `smoke`; `test_piece_keys figured-variations` (20 of 20 runs complete, the parallel span related to home, no pivot, not counted as a change); the 24 new lines of `template-plans.txt` (2/4 in C major and A minor, both sizes); the form's table and sections in `test_form_spec` and `test_api_forms`; and a request at 56 and at 40 in `test_api_piece`, whose responses `test_api_schema` validates.

## The rows (API 1.22)

A piece row of the form gains `figure` (R-501): `null` on the theme and the codettas, and on a variation `{"kind", "rhythm", "harmony", "change_at", "climax", "raised", "mode", "skeleton", "text"}`. `skeleton` lists the skeleton's principal tones, one per beat cell of the composed variation (16 in 8 bars of 2/4), as note names with octave (C4 middle C); `raised` says whether the climax octave was applied (V5 asks for it; at the melody range's top it stays down, as measured above); `mode` is `"parallel"` for V3; `text` is the sentence for a person, for example `neighbor tones in sixteenths over the skeleton E5 D5 C5 ...`. The row's `key` of V3 is `C minor` in a C major piece. A figurate variation's `changes.text` says `melody: figured over the skeleton` in place of the notes added. The forms entry serves the same figure per section (`kind`, `rhythm`, `harmony`, `change_at`, `climax`, no skeleton) and `mode`; every other form's sections have both `null`. See [api/theme-and-piece.md](api/theme-and-piece.md) and [api/vocabulary-and-validation.md](api/vocabulary-and-validation.md).

## Later

Not built (R-497 to R-502): more than one skeleton per variation; a figure longer than a cell (Schoenberg allows up to two bars); changing the figure inside a variation other than at the cadences; contrapuntal variations (cantus firmus, ostinato, canon, fughetta); substitute sequence progressions and chromatic movement; the parallel mode for a section grown afresh, another key for a written-out repeat and Op. 34's keys and meters; a bridge between variations (P51 builds transitions); a coda with extra variations; changes of meter and tempo within the set; a theme of 16 bars or a binary or ternary theme.
