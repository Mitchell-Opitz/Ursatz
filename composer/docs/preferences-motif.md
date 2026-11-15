# Preferences: the motif group

Part of the settings document ([preferences.md](preferences.md)). Back to the [docs index](README.md).

The settings of the segments motive generator ([cli-random-motive.md](cli-random-motive.md#the-segments-strategy)), stored in `MotiveRandomPreferences` and read through `MotiveRandomOptions.preferences` (NULL for the defaults). The drawn strategy keeps its constants as the comparison baseline and reads none of them (R-304). A weighted list is an object of whole numbers 0 to 20 under the names below, with a sum above 0; a name left out keeps its default. A list whose weights sum to 0 where the meter leaves an entry out (a three-way shape list in a meter with no halves) draws its first entry.

| Key | Default | Limit | What it shapes | Replaces |
| --- | --- | --- | --- | --- |
| `bars` | `one` 1, `two` 1 | weighted list | A motive of one bar or of two. One bar only in a meter of an even number of beats, four or more. | `SEGMENT_BAR_ONE_BAR_WEIGHT`, `SEGMENT_BAR_TWO_BARS_WEIGHT` |
| `pickup_one_in` | 4 | 0 to 64 | A pickup (half a beat or a beat) in one motive of this many; 0 never draws one and draws no random number for it. | `SEGMENT_PICKUP_ONE_IN` |
| `shapes_one_bar` | `repeat` 3, `contrast` 2 | weighted list | The shape of a one-bar motive: a a' or a b. | `SHAPE_WEIGHTS_ONE_BAR` |
| `shapes_two_bars` | `repeat` 3, `contrast` 2, `repeat_contrast` 2 | weighted list | The shape of a two-bar motive; `repeat_contrast` (a a' b) only with an even meter. | `SHAPE_WEIGHTS_TWO_BARS` |
| `plans_two` | `I-V` 3, `I-IV` 1, `I-I` 1 | weighted list | The chords of a two-segment motive. | the weights of `PLANS_TWO` |
| `plans_three` | `I-V-I` 2, `I-IV-I` 1 | weighted list | The chords of a three-segment motive. | the weights of `PLANS_THREE` |
| `features` | `dotted` 1, `leap` 1, `repeat` 1 | weighted list | The distinctive feature of segment a. A feature that cannot fit its cell gives way to the next one with a weight. | `FEATURE_KINDS` (equal) |
| `leap.low` | 5 | 3 to 12 | The smallest leap, in semitones, a leap feature may be. | `SEGMENT_LEAP_LOW` |
| `leap.high` | 9 | `leap.low` to 12 | The largest. | `SEGMENT_LEAP_HIGH` |
| `rung_moves` | `step` 6, `skip` 2 | weighted list | A walk on the chord tones moves one rung or two. | the `moves` table of `next_rung` |
| `decorations` | `chord_tone` 1, `passing` 1, `neighbor` 1, `appoggiatura` 1, `escape` 1 | weighted list | What a note between two frame notes is: another chord tone or a non-chord tone of the type. | `DECORATION_KINDS` (equal) |
| `durations` | `half_beat` 2, `beat` 4, `two_beats` 1, `quarter_beats` 0, `long_short` 0, `short_long` 0, `triplet` 0, `triplet_two_beats` 0 | weighted list | The note lengths a cell is tiled with; a length that does not fit its place is left out. The five after `two_beats` are off by default (R-422): two notes of a quarter beat, a dotted-beat figure and its reverse, and the two triplets (simple meters only). | `MOTIVE_RANDOM_DURATION_WEIGHTS` (the drawn strategy still reads it) |
| `accented_one_in` | 0 | 0 to 64 | One eligible note in this many becomes an accented non-chord tone: a beat note whose neighbors are frame notes and which resolves to the next note. 0 never and draws no random number. Off by default; R-422. | new (R-425) |
| `accented_types` | `appoggiatura` 1, `passing` 1, `suspension` 1 | weighted list | Which accented figure is placed; a suspension only where the chord changes. Off by default (it matters only with `accented_one_in`); R-422. | new |
| `avoid_repeats` | `false` | true or false | No repeated note: a decoration that finds no figure tries the other kinds before a chord tone, and an adjacent repeat is refused unless the feature or a suspension makes it. Off by default; R-422. | new |
| `leaps_per_motif` | 11 | 0 to 11 | The most intervals of 5 semitones or more in a motive; 11 never rejects. Off by default; R-422. | new |
| `leap_recovery` | `false` | true or false | A leap is followed by a move of 1 or 2 semitones the other way. Off by default; R-422. | new |
| `pickup_from_accent` | `false` | true or false | A pickup is one chord tone below segment a's first note and no longer than it, and a motive that opens with an implied upbeat but has no pickup is refused. Off by default; R-422. | new |
| `contrast_fidelity_min` | `"0/1"` | a ratio `n/d` as text, 0 <= n <= d, 1 <= d <= 1000 | The least recognizability of a contrasting segment against segment a; 0 never rejects. Off by default; R-422. | new |
| `long_on_weak` | 0 | 0 to 20 | The two-beat note is this much more likely on a weak beat (its weight times 1 plus this); 0 changes nothing. Off by default; R-422. | new |
| `final_long` | `false` | true or false | The last note of the last segment is longer than every other note in it. Off by default; R-422. | new |
| `compound_beats` | `false` | true or false | In 6/8, 9/8 and 12/8 the beat is the dotted quarter: cells are tiled in beats of three eighths. No effect in any other meter. Off by default; R-422. | new (R-439) |
| `endings` | `free` 1, `closed` 0, `open` 0, `lead_in` 0 | weighted list | How the last segment ends: `free` as drawn, `closed` (a long last note on the strongest beat available, the root or third of the chord), `open` (the last note is degree 2, 5 or 7 of the key) or `lead_in` (two short last notes, the last off the beat). A motive that ends on IV cannot end open; `final_long` never passes with a lead-in. Off by default; R-422. | new (R-440) |
| `long_nct_one_in` | 0 | 0 to 64 | A segment gets a long non-chord tone (on a beat, a beat or longer, resolving to the next note) one time in this many. 0 never and draws no random number. Off by default; R-422. | new (R-441) |
| `long_nct_types` | `appoggiatura` 1, `suspension` 1, `retardation` 1, `passing` 1, `neighbor` 1 | weighted list | Which long tone is placed; a suspension or retardation only where the chord changes. Matters only with `long_nct_one_in`; R-422. | new |
| `long_nct_intervals` | `second` 1, `fourth` 1, `sixth` 1, `seventh` 1 | weighted list | The tone's interval above the chord root. Matters only with `long_nct_one_in`; R-422. | new |
| `long_nct_beats` | `downbeat` 1, `middle` 1, `other` 1 | weighted list | Where the tone sits: the downbeat, the middle of the bar, another beat. Matters only with `long_nct_one_in`; R-422. | new |
| `long_nct_lengths` | `beat` 1, `dotted_beat` 1, `two_beats` 1 | weighted list | How long the tone is: a beat, a dotted beat, two beats or more. Matters only with `long_nct_one_in`; R-422. | new |
| `syncopation_one_in` | 0 | 0 to 64 | A cell, and a segment boundary, gets a syncopation one time in this many; 0 never and draws no random number. Simple meters only (not 6, 9 or 12 beats a bar), never on the motive's last note. Off by default; R-422. | new (R-454) |
| `syncopation_types` | `off_beat` 1, `tie_beat` 1, `tie_bar` 1, `anticipation` 1 | weighted list | Which figure: an off-beat note held across the beat, a 3+3+2 tie across the beat, the last note before a barline held across it, or a segment's first note arriving early on a chord change. Matters only with `syncopation_one_in`; R-422. | new |
| `syncopation_levels` | `eighth` 1, `sixteenth` 1 | weighted list | The short note of the figure is a half beat or a quarter beat. Matters only with `syncopation_one_in`; R-422. | new |
| `skips_per_motif` | 11 | 0 to 11 | The most intervals of 3 or 4 semitones (a skip) in a motive, counted beside `leaps_per_motif` on every adjacent interval; 11 never rejects. Off by default; R-422. | new (R-455) |
| `plan` | absent | 1 to 4 chords | A chord plan as written instead of drawn from `plans_two` and `plans_three`: Roman numerals `I` to `VII` in any case, an optional `o` or `°` (ignored), and a place `@bar` or `@bar:beat` (bar 1 or 2, beat 1 to 12; `@2` is `@2:1`). Either no chord has a place (at most 3, one to a segment) or every chord after the first has one, rising; the first is at 1:1 and may be written `@1`. Written back upper case with the place only when set (`"II"`, `"V@1:3"`); not written when absent. A plan that does not fit the meter, bars, shapes or endings allowed is refused by the run at `/preferences/motif/plan`. Off by default; R-422. | new (R-456 to R-461) |

With every default each draw makes the random-number calls it always made, so a seed gives the motives it always gave (the `random-motive*` snapshots are unchanged). The plan degrees (I-V, I-IV, I-I, I-V-I, I-IV-I) stay in `motive_random_segments.c`; the preferences set only how often each is drawn.

```json
{
  "motif": {
    "bars": {"one": 1, "two": 1},
    "pickup_one_in": 4,
    "shapes_one_bar": {"repeat": 3, "contrast": 2},
    "shapes_two_bars": {"repeat": 3, "contrast": 2, "repeat_contrast": 2},
    "plans_two": {"I-V": 3, "I-IV": 1, "I-I": 1},
    "plans_three": {"I-V-I": 2, "I-IV-I": 1},
    "features": {"dotted": 1, "leap": 1, "repeat": 1},
    "leap": {"low": 5, "high": 9},
    "rung_moves": {"step": 6, "skip": 2},
    "decorations": {"chord_tone": 1, "passing": 1, "neighbor": 1, "appoggiatura": 1, "escape": 1},
    "durations": {"half_beat": 2, "beat": 4, "two_beats": 1, "quarter_beats": 0, "long_short": 0, "short_long": 0,
                  "triplet": 0, "triplet_two_beats": 0},
    "accented_one_in": 0,
    "accented_types": {"appoggiatura": 1, "passing": 1, "suspension": 1},
    "avoid_repeats": false,
    "leaps_per_motif": 11,
    "leap_recovery": false,
    "pickup_from_accent": false,
    "contrast_fidelity_min": "0/1",
    "long_on_weak": 0,
    "final_long": false,
    "compound_beats": false,
    "endings": {"free": 1, "closed": 0, "open": 0, "lead_in": 0},
    "long_nct_one_in": 0,
    "long_nct_types": {"appoggiatura": 1, "suspension": 1, "retardation": 1, "passing": 1, "neighbor": 1},
    "long_nct_intervals": {"second": 1, "fourth": 1, "sixth": 1, "seventh": 1},
    "long_nct_beats": {"downbeat": 1, "middle": 1, "other": 1},
    "long_nct_lengths": {"beat": 1, "dotted_beat": 1, "two_beats": 1},
    "syncopation_one_in": 0,
    "syncopation_types": {"off_beat": 1, "tie_beat": 1, "tie_bar": 1, "anticipation": 1},
    "syncopation_levels": {"eighth": 1, "sixteenth": 1},
    "skips_per_motif": 11
  }
}
```

The default document has no `plan` member; a plan reads like `"plan": ["ii", "V@1:3"]`.
