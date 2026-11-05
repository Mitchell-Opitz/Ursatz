# common-practice-minimal weighted identification

Weighted chord identification and non-chord tones of `frameworks/common-practice-minimal/`, including chords in context. [Docs index](../README.md)

Framework overview: [frameworks/common-practice-minimal/README.md](../../frameworks/common-practice-minimal/README.md).

## Weighted identification and non-chord tones

`triad_identification.c` and `seventh_identification.c` match by subset:
a triad or seventh matches when its degrees or intervals are all present,
and any extra degrees are ignored as non-chord tones. Ties go to the first
match in scan order and are logged to stderr. This tolerates one extra tone
per beat; with two or more, nothing matches (Field Nocturne No. 1 resolved
61 of 394 fixed-grid beats). Those functions are unchanged.

`weighted_identification.h`, `non_chord_tones.h` and
`weighted_chord_identification.h` are the weighted replacement, a new
function beside them. A segment's members (plain structs: pitch, held or
attacked, duration, the caller-computed `BeatStrength`, voice) are weighted by
duration times a beat-strength or held factor, with the bass pitch class
boosted. The 14
diatonic candidates of a major key (seven triads, seven sevenths) are scored
by chord-tone share, minus a penalty for an absent third and plus a bonus for
a chord-tone bass; ties go to root position, then the triad, then the lower
degree. The best chord, a runner-up on a different root, both scores, and an
uncertainty (runner-up over best, higher is less certain) are reported, and a
best score under the minimum coverage is no match. Leftover tones are labeled
with the shared `NonChordTone` enum (passing, neighbor, appoggiatura, escape
tone, suspension, anticipation, pedal) by approach and departure, using the
same voice or else the nearest pitch in adjacent segments; anything unclear
is left unclassified. The defaults are strong 1.0, medium 0.7, weak 0.4, held
0.6, bass x1.1, missing third 0.15, chord-tone bass 0.05, minimum coverage
0.5, all configurable. This framework includes no other framework: the caller
supplies each member's `BeatStrength`. Limits: diatonic candidates only and no
voice separation yet. See docs/modules/analysis.md.

### Chords in context

The sequence function reads a segment against its neighbours after pass 1.
A segment is incomplete when it has no match, or fewer than three of its best
chord's pitch classes sound (two with the root in the bass count as complete:
a triad without its fifth; a single note never does).

- **Arpeggios.** A run of two or more consecutive segments inside one beat
  (`CommonPracticeSegment.beat`, set by the caller; 0 is unknown), none
  complete, is read as one chord when the union of their members is complete.
- **Neighbours.** Any other incomplete segment whose pitch classes all lie in
  a neighbouring chord takes that chord: the previous one (as resolved, so a
  run of sparse segments carries one chord on), else the next one's own
  complete reading.

The result's `context` records the evidence (`PREVIOUS`, `NEXT`, `ARPEGGIO`,
or `NONE` for the segment's own reading). Upstream, adaptive segmentation's
`merge_unchanged_harmony` (default on) joins a melody or passing note over
held harmony into the chord it sounds over; outer-voice extraction turns it
off to keep one segment per melody note.

The bass weight applies to every member on the bass pitch class. Its default,
1.1, was chosen on the accuracy harness and the Framework v1 verification:
1.0 gave the best chorale root agreement (84.1%) but lost Field 5's home key
(B-flat read as D minor); 1.5 and above lowered chorale root agreement (82.5%
at 1.5, 80.3% at 3.0). At 1.1 the chorales give 83.8% root agreement (82.8%
before). Field Nocturne No. 1 stays at 78.7% (48 of 61), below the 90%
target: its misses are a vii or iii read where V7 or I is marked, on beats
whose bass sounded in an earlier beat and is not held into them.

`outer_scale_degrees.h` takes its degree numbers from `standard_tonality_degree_of_semitone`, the one scale degree owner (R-285), and labels the soprano and bass of each segment from
`analysis/outer_voice.h` as `ScaleDegree` Interpretations relative to the
caller's key, with a required `FrameworkReference` and confidence 1/1. Limits:
no voice-crossing awareness, no inner voices, one line per extreme; absent
segments get no label. See docs/modules/analysis.md.

`key_regions.h` finds LOCAL key regions: windowed Krumhansl-Kessler key scoring, a dynamic-programming
path with a change penalty, `KeyChangeExtent` labels, pivot chords, and key signature events as authoritative
boundaries. It is a new function beside the whole-piece estimator, which is unchanged. It takes plain structs
(segment time windows, `CommonPracticeWeights`, members, signature events) and includes no other framework.
Three optional inputs make it harmony aware, all plain data and all off when not given:

- `CommonPracticeKeySegment.chord` (root and a pitch-class mask; `common_practice_key_chord_of` reads it from a
  weighted identification result). A window adds `evidence_weight` (default 0.8) times its functional evidence per
  chord to each key's score: a dominant to tonic motion into a tonic chord with the key's own third (1), a leading
  tone resolving up (0.5), and in minor a raised seventh after a chord holding the minor third or sixth (0.5). A
  dominant into a major tonic therefore favours the major key and not its parallel minor.
- `phrase_boundaries` in the config (times in whole notes). Each edge the path drew moves to the nearest boundary
  measure within `snap_reach_measures` (default 1), never onto or past a neighbouring edge, and never an edge at a
  key signature event. The caller runs the function twice (first pass without boundaries, then chords, cadences
  and phrase ends from it, then a second pass with them) and re-runs everything after it once; it does not iterate
  further. `tests/accuracy/chorale_chain.c` and the Framework v1 chain do this; the chorale chain takes the phrase
  ends from the cadences' arrival ends, which is what the kernel's phrase boundary detection does for a cadence.
- The home key holds: a region whose key is the dominant, subdominant or relative of the key before it is folded
  back into that key when all of its chords are rooted on that key's IV, V or ii and made of its tones and no
  pitch class outside the key carries more than `hold_tolerance` (default 0.05) of its weight. A switch needs a
  non-diatonic pitch or a chord outside IV, V and ii; a region with no chord data is never folded back.

The defaults `evidence_weight` and `snap_reach_measures` were chosen from the accuracy harness, not from the
reference pieces. On the ten chorales the `key` metric is 62.0% before, 64.1% with the boundaries alone, 66.7% at
weight 0.3 to 0.5, 69.1% from 0.8 to 1.6 and 67.7% from 2.0, so 0.8 is the lowest value on the plateau. At weight
0.8 a reach of 0 gives 67.9%, 1 gives 69.1%, 2 gives 64.4% and 4 gives 62.2%: a boundary the first pass missed
pulls an edge to the wrong phrase end, so the reach is one measure and not the window. The hold rule changed no measured result on the chorales or the five
reference pieces; it is exercised by the unit tests. The chorale harness reads `key` 69.1%, Roman numeral in the
chain's key 52.0% (47.1% before), chord root 84.5% (83.8%).
Limits: no cadence confirmation of a key change (the extent is read from region lengths alone), profile bias in
chromatic music, natural-minor pivots, a phrase boundary the first pass did not find cannot place an edge, and the
evidence needs identified chords, so a wrong chord is wrong evidence. See docs/modules/analysis.md.

`key_identification.c` matches by one-directional containment: a key
matches when all seven of its pitch classes are present, and pitch classes
outside the key do not disqualify it. That lets a chromatic piece match the
wrong tonic. It is the last-resort fallback in both consumer apps, so it is
rarely reached, but it is still wrong when it is.
