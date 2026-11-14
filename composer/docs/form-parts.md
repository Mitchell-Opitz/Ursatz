# The parts of larger forms

The section kinds a larger form is made of, after Schoenberg ch. XVIII: the transition, the subordinate theme, the retransition and the coda (R-503). Back to the [docs index](README.md); the form side is in [forms.md](forms.md), the constraint in [plan-constraints.md](plan-constraints.md).

## The four parts

A form document names a part as the role of a section (`transition`, `subordinate`, `retransition`, `coda`), and `form_build_plan` gives the section a `section_function` constraint with the function, `independent` and `lyric` (all three always written). Any form may use them. The built-in [main-and-subordinate](main-and-subordinate.md) form uses all four (R-509); no other built-in form does, so every other output is unchanged.

| Part | In the form document | In the plan | Grown |
| --- | --- | --- | --- |
| Transition | A theme-like section: `beginning` `presentation` or `compound_basic_idea`, `ending` `continuation`, `cadence` `half`; 8 bars when it modulates (`keys.end` is not its start), else 4 or 8, or 3 to 16 with a `construction`. No `returns`, `variation`, `figure`, `mode` or `pedal`. `independent` (default false) gives it a motive of its own (R-504), so it has no `motive`. | `archetype`, `theme_pairing`, `section_function` (`transition`) | As a theme, with its continuation liquidated to the upbeat chord (below); an independent one is made from a motive of its own (below) |
| Subordinate theme | Any pairing, cadence, keys and construction a theme-like section takes. `lyric` (default false): a period whose motive is m0 augmented (R-506); a lyric theme has no `motive` or `returns`, and the reader sets its motive to `augment` with an open ratio. | `archetype`, `theme_pairing`, `section_function` (`subordinate`), and for a lyric theme a `motive_ref` of `augment` | As a theme today; the open ratio is drawn as any section's motive is (the first usual ratio the meter writes, else an interval change) |
| Retransition | 4 bars, `cadence` `half`, nothing else: no `beginning` or `ending` and none of the members of a theme. | `section_function` (`retransition`), no `archetype` or `theme_pairing` | By the liquidation operator's second layout (R-505, below) |
| Coda | 8 bars, `cadence` `perfect-authentic`, nothing else. | `section_function` (`coda`), no `archetype` or `theme_pairing` | By its own operator (R-507, below) |

A retransition and a coda count as theme sections in a compose run and are skipped when the form is heard (they have no theme shape to be heard as), and the style leaves their form alone, as it does a liquidation's.

## What is composed and what is not heard

The parts are composed to the plan and recorded in it; the hearing does not find them (R-408). The checks read the plan: the function of each section, its key and its cadence. B2 to B6 add what each part's section is made of and what was measured; the form that uses them, measured in the eight rows, is [main-and-subordinate.md](main-and-subordinate.md) (B7).

The test form is [parts-of-forms.json](../tests/fixtures/forms/parts-of-forms.json): 48 bars with the parts in the order of a sonata-like ternary (A, T, S, a codetta, R, A return, K). `test_form_spec` reads it and checks the plan it builds in C major 4/4; `test_plan_json` writes and reads the plan back byte for byte.

## The transition

A transition is a sentence ending in a half cadence (R-504). Grow-theme reads its `section_function` and sets `continuation.liquidate` for that section only, on the request copy: the shared context and the preferences are untouched (R-405), so no other section and no existing output changes. Its continuation then liquidates: `fragment 2` is a residue of a segment of m0 made by liquidation steps and the cadential idea goes at least as far ([grow-continuation.md](grow-continuation.md)).

A transition that modulates is 8 bars and its second half is in the new key (`keys.end`), so its half cadence is that key's V, the upbeat chord. The second half opens on a pivot. From a major key into its dominant the pivots are the new key's IV, ii and vi, all predominants, and a predominant pivot alone leaves too few chords before the V for four bars; the harmony stage then deals the predominant pivots going on to the new key's V7 and I, then the predominant and the V (R-512, [piece.md](piece.md#keys-and-cadences)). Into III of a minor key the pivots are tonics and need no second deal. Modulating in stages is two transition sections, home to a region and that region to another: data, no new code.

Only the liquidation is the transition's own: the pivot rule serves any half-cadence phrase that opens a key.

### An independent transition

`"independent": true` makes the transition from a motive of its own, invented as m0 is (R-504); only such a section changes. `form_build_plan` holds one bar more of material for each and adds an open motive node `independent <section name>` to the material's idea, one bar at the first bar after m0's layout and the extensions, with an `INVENT_MOTIVE` melody span that the invent stage writes as it writes m0. The section's `MOTIVE_REF` names that node with no step (it is not m0 made over). Grow-theme reads a reference with no step to a motive of the material other than m0 as the motive the section is made from as it is: the ideas, the fragments and the liquidation come from it, and the compose run's basic motive, which is m0's, is not used for its chords, features or segments. A form with no independent transition builds the plan it built before.

### What was measured

`tests/test_form_parts.c` case `transition` (`slow`, about 25 s): a 32-bar form built in the test (A theme 8 bars, PAC, closed; T transition 8 bars, HC, `keys.end` `subordinate`; S subordinate 8 bars, antecedent and consequent, PAC, in `subordinate`; C codetta 8 bars, presentation and continuation, PAC, in `subordinate`; region major [5], minor [3]), `modulate` on, seeds 1 to 10, eight rows. At least 9 of 10 must compose in each row.

| Row | Composed |
|---|---|
| C major 4/4 | 10 of 10 |
| A minor 4/4 | 10 of 10 |
| C major 3/4 | 10 of 10 |
| A minor 3/4 | 10 of 10 |
| G major 2/4 | 10 of 10 |
| D minor 2/4 | 10 of 10 |
| C major 6/8 | 10 of 10 |
| D minor 6/8 | 10 of 10 |

In all 80 runs T carries the transition function, its second half is in the region's key (a key lane span starts at the half: V in major, III in minor), its last chord is V or V7, its `fragment 2` idea is a liquidation by 1 step or more on a segment, its cadential idea has at least as many steps, and the codetta's `fragment 2` has none. The liquidation is read from the plan, not counted in notes: a residue is regrown to fill its bar, so `fragment 2` often holds as many melody notes as `fragment 1` or more. Without the pivot rule the four major rows composed 0 of 10 (stuck at harmony); `tests/test_harmony_phrase.c` `test_pivot_into_the_dominant` holds the rule on its own.

### The independent transition

`tests/test_form_parts.c` case `independent`: the same 32-bar form with T `independent`, `modulate` on, seeds 1 to 10, eight rows. At least 9 of 10 must compose in each row.

| Row | Composed |
|---|---|
| C major 4/4 | 10 of 10 |
| A minor 4/4 | 10 of 10 |
| C major 3/4 | 10 of 10 |
| A minor 3/4 | 10 of 10 |
| G major 2/4 | 10 of 10 |
| D minor 2/4 | 10 of 10 |
| C major 6/8 | 10 of 10 |
| D minor 6/8 | 10 of 10 |

In all 80 runs the material holds two invented motives (m0 and `independent T`, two notes or more each), T's `MOTIVE_REF` names the second with no step, T's `basic idea` melody differs from A's, and the composed plan round-trips through plan JSON byte for byte.

## The retransition

A retransition is 4 bars at home over a dominant pedal, ending in a half cadence (R-505). `grow_liquidate.c` grows a Constrained, childless section with the `retransition` function as its second layout: five ideas of 1, 1, 1, 1/2 and 1/2 bars over one prolongation of V with the pedal. The `basic idea` is the residue (m0 liquidated, 1 to half its notes dropped), the `repetition` the residue one note further, `fragment 1` a scale line of eighths filling a bar, `fragment 2` a broken chord of V in half a bar, and the `cadential` idea the main theme head (m0's leading notes that fit half a bar, written as they are) on the half cadence. The layout is in [grow-liquidation.md](grow-liquidation.md#the-retransition).

It opens the home key straight from any region: the key before it may be the subordinate region's, and the prolongation of V is the arrival, so there is no pivot. The return home is at the retransition's start, never inside a section (a key change back home at a section's half failed in minor at main, R-505). The liquidation operator accepts 4/4 for both layouts (a half bar of 4 eighths); the scherzo's liquidation in 3/4, 2/4 and 6/8 is unchanged, byte for byte. Not built, Later: a retransition of 8 bars, one over a tonic or another pedal, one that quotes more than the head.

### The retransition measure

`tests/test_form_parts.c` case `retransition` (`slow`): [parts-of-forms.json](../tests/fixtures/forms/parts-of-forms.json) with the coda K replaced by a codetta of 8 (presentation and continuation, PAC, closed), so no coda is grown yet; 48 bars (A, T, S lyric, C codetta of 4, R, A return exact, K), `modulate` on, seeds 1 to 10, eight rows. At least 9 of 10 must compose in each row.

| Row | Composed |
|---|---|
| C major 4/4 | 10 of 10 |
| A minor 4/4 | 10 of 10 |
| C major 3/4 | 10 of 10 |
| A minor 3/4 | 10 of 10 |
| G major 2/4 | 10 of 10 |
| D minor 2/4 | 10 of 10 |
| C major 6/8 | 10 of 10 |
| D minor 6/8 | 10 of 10 |

In all 80 runs R carries the retransition function and no archetype, is in the home key, has the dominant in the bass throughout and a last chord V or V7, its `basic idea` asks for a residue with fewer notes than m0, and its `cadential` melody keeps the intervals of the main theme head, which are the first intervals of m0. `test_grow_liquidate` composes the liquidation fixture in C major 4/4, 10 of 10 (the other rows unchanged).

Two things the measure found. A broken chord of four eighths over V (5, 7, 2, 4) ended on the seventh, so in four eighths it is 5, 7, 2, 5. A melody target for the head was made again under the cadence's rules and did not keep m0's intervals (seed 2 in C major 4/4), so the head is written as it is.

## The coda

A coda is 8 bars at home in three phrases of 4, 2 and 2 bars, each ending in a perfect authentic cadence (R-507). `grow_coda.c` grows a Constrained, childless section with the `coda` function, no `archetype`, `theme_pairing`, `return` or `hold`, a PAC, 8 whole bars from a barline in one meter span and one key span with no change, when m0 has 2 notes or more, is at most a bar long and lies in MIDI 58 to 84. The main theme is quoted, then liquidated, then only its head and a held tonic are left:

| Phrase | Bars | Harmony shape | Ideas |
|---|---|---|---|
| 1 | 1 to 4 | phrase model, tonic to tonic | `basic idea` (bar 1) and `repetition` (bar 2): m0 exact; `cadential` (bars 3 and 4): m0 augmented 2:1, node `m1` (m0 exact when the augmentation is longer than 2 bars or the meter cannot write it) |
| 2 | 5 and 6 | cadential | `fragment 1` (bar 5): m0 liquidated, 1 to half its notes dropped, node `m2`; `cadential` (bar 6): m0 liquidated one note further, node `m3` |
| 3 | 7 and 8 | cadential | `fragment 2` (bar 7): the main theme head as the retransition makes it (written as it is, node `main theme head`); `cadential` (bar 8): the tonic, a bar long, in the octave nearest the head's last note (node `final tone`) |

Each phrase has the PAC and its own chord targets, and its last idea is a melody target with the cadence. The motive nodes take 5 bars of the material (6 when the augmentation fills 2 bars). The judge's hard rules: every idea fits its span, `fragment 1` has fewer notes than m0 and phrase 2's `cadential` fewer than `fragment 1`; its soft rules: the share of phrases as long as or shorter than the one before (4, 2, 2: 2 of 2) and of ideas inside phrases 2 and 3 with fewer notes than the one before. The label is `coda <drop> dropped`. A coda of other lengths, or one that quotes the subordinate theme, is Later.

### The coda measure

`tests/test_form_parts.c` case `coda` (`slow`): [parts-of-forms.json](../tests/fixtures/forms/parts-of-forms.json) as it is (48 bars, K the coda), `modulate` on, seeds 1 to 10, eight rows. At least 9 of 10 must compose in each row.

| Row | Composed |
|---|---|
| C major 4/4 | 10 of 10 |
| A minor 4/4 | 10 of 10 |
| C major 3/4 | 10 of 10 |
| A minor 3/4 | 10 of 10 |
| G major 2/4 | 10 of 10 |
| D minor 2/4 | 10 of 10 |
| C major 6/8 | 10 of 10 |
| D minor 6/8 | 10 of 10 |

In all 80 runs K carries the coda function and no archetype, has three phrases of 4, 2 and 2 bars, each phrase ends on V or V7 to a root-position tonic with the dominant and then the tonic in the bass, the last melody note is the tonic, and the plan round-trips through plan JSON byte for byte. `grow_coda_applies` is true on K and false for a section that is no coda, a coda of 4 bars, a coda that follows another region (a start change) and a source of 2 bars. m0 is checked only once it is made: a plan before its motive is composed applies, as the liquidation's does.

## The game loop

A section of a game loop request may be a transition or a coda (R-508, B6): `PieceSection.function`, `PIECE_FUNCTION_TRANSITION` on any body or intro section that is a theme section with a half cadence, `PIECE_FUNCTION_CODA` on the ending's 8 bars with a perfect authentic cadence. The rules are in [piece-structure.md](piece-structure.md). A request without `function` builds the same plan as before. The retransition and the subordinate theme are not offered to a game loop.

`tests/test_piece_structure.c` case `functions`: a loop in C major 4/4 and in A minor 3/4 (intro 4 bars, body A 8 bars, T 8 bars a transition, B 8 bars with a motive of its own, turnaround 4 bars, ending K 8 bars a coda), seeds 1 to 10. The loop composed 10 of 10 and the ending (built from the composed loop, seed + 20) 10 of 10 in both rows. In every run T ends on V or V7 and its `fragment 2` is a liquidation (the motive node its melody span names has `steps` 1 or more and a segment, R-512), and K has three phrases of 4, 2 and 2 bars and no archetype. Each refusal is `PIECE_ERR_BAD_REQUEST`.

## The rows and the API

API 1.23 (R-510) names the parts in the result. Each row of a piece, in a form with sizes and in a game loop, has `function`: `null` for a section that is none of the parts, else `{"kind", "independent", "lyric", "text"}`, read from the section's `section_function` constraint (a performed row takes it from the section it performs). A transition says whether it modulates (`, modulating to G major`, the plan's key at three quarters of the section against its start), a lyric subordinate theme says so, a coda says its three phrases. The forms entry's sections carry `independent` and `lyric`. The vocabulary lists `piece_functions` (transition on `intro` and `body`, coda on `ending`) and a game loop request section takes `function` `"transition"` or `"coda"`; any other value is refused at `<section>/function`. A retransition and a subordinate theme are not game loop functions: they come from a form document only. Every composed output is unchanged. `tests/test_api_parts.c` composes the `main-and-subordinate` piece (C major 4/4, 57 bars, seed 1) and a game loop with a transition and a coda and reads the rows; `test_api_forms` and `test_api_vocabulary` read the entry and the list.
