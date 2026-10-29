# common-practice-minimal

The first populated Theory Framework instance in Ursatz: `common-practice-minimal-v1`.

Path `frameworks/common-practice-minimal/`, tests
`frameworks/common-practice-minimal/tests/`. Built as a Framework
extension module physically separate from `src/theory/`, per
`docs/modules/theory.md`'s documented boundary ("built as extensions on
top of Framework/Rule/Constraint after this module, never as changes to
the L7 kernel itself"). This is the first module in that pattern;
future extensions (jazz harmony, species counterpoint, etc.) are
expected to follow the same `frameworks/<name>/` shape.

## Scope

Basic diatonic triad identification and major/natural-minor key naming
for Western common-practice tonal harmony. Specifically, this pass
covers exactly:

- **Seven diatonic triads**, relative to a declared key: I (major), ii
  (minor), iii (minor), IV (major), V (major, also usable as dominant
  function), vi (minor), vii&deg; (diminished). Each resolves to a real
  `Chord` (`interpretation/chord.h`) labeled with its roman numeral, and
  each has a backing `Rule`/`Constraint` pair referencing this
  Framework.
- **Four diatonic seventh-chord qualities**: dominant7, major7, minor7,
  half-diminished7 -- the seventh chords that occur naturally within a
  major or natural-minor key. Unlike the triads above, seventh-chord
  identification (`seventh_identification.h`) is quality-keyed rather
  than degree-keyed: matching is against each chord tone's root-relative
  semitone interval (0-11), not scale-degree position, because which
  degree a seventh chord sits on does not by itself determine its
  quality (e.g. scale degree 1 is major7 in a major key but minor7 in
  natural minor). Each quality resolves to a real `Chord` labeled with
  its quality name (e.g. "dominant7"), with its own backing
  `Rule`/`Constraint` pair (`chord_vocabulary_seventh.h`), mirroring the
  triads' construction shape but as a separate table/code path.
  The subset matcher scans these four only (and the seven degree-keyed
  triads); the diminished7 and the augmented triad below belong to the
  weighted minor vocabulary.
- **Minor-key chord vocabulary for weighted identification** (R-030). The
  weighted identifier (`weighted_identification.h`) scores the region's
  own mode: the 14 diatonic chords in a major key, and in a minor key the
  26 diatonic triads and sevenths of natural, harmonic and melodic minor
  together, including the augmented III (`COMMON_PRACTICE_TRIAD_AUGMENTED`)
  and the fully-diminished vii-dim-7
  (`COMMON_PRACTICE_SEVENTH_DIMINISHED7`, root-relative {0,3,6,9}). The
  minor-major and augmented-major sevenths are excluded. A minor triad is
  labeled by its quality ("major", "minor", "diminished", "augmented"),
  since a degree numeral depends on which form of the scale it comes from;
  the Roman numeral is standard-tonality's work (RomanText spelling, R-031).
  When a segment fits the raised and the natural form equally, the raised
  form wins for V, V7, vii-dim and vii-dim-7 and the natural form wins
  everywhere else. Pivot chords whose old key is minor are read in this
  vocabulary too (`key_regions.h`).
- **Major and natural minor keys only.** Each resolves to a real `Key`
  (`interpretation/key.h`) labeled with its name (e.g. "C major", "A
  minor").
- **Deterministic, single-answer triad identification**, given
  caller-supplied scale-degree data for a Sonority already understood
  to be in a declared key (see `triad_identification.h` for why scale
  degrees rather than a raw `Sonority` -- `PitchIdentity` carries no
  numeric semantics yet, so no numeric pitch-class analysis is possible
  at this layer; this mirrors the same limitation
  `analysis/chord_identification.c` already documents).

## Deliberately out of scope (not omitted by oversight)

- Ninths and other extensions beyond sevenths, and altered chords
- Modal interchange / borrowed chords
- Secondary dominants and other tonicization (including chromatic
  seventh chords such as secondary-dominant sevenths)
- Harmonic and melodic minor in the subset matcher and in `Key` (natural
  minor only there); weighted identification reads all three forms
- Chord inversion identification (a triad or seventh chord in any
  inversion is matched to its root-position label; which tone is in the
  bass is not reported)
- True multi-candidate / confidence-weighted analysis: this pass
  returns a single best match by the documented tie-break rule
  (unordered scale-degree-set membership) rather than a weighted
  candidate set. Genuinely ambiguous or partial sonorities (e.g. fewer
  than three distinct scale degrees, or a set matching more than one
  reading under a different framework) are reported as no match, not
  guessed.
- Modulation and key-change detection
- Cadence, voice-leading, and other harmonic-function analysis beyond
  bare triad/key naming

All of the above are planned for later passes, either within this
Framework's own versioning (`common-practice-minimal-v2`+) or as
separate `frameworks/<name>/` extensions.

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

`outer_scale_degrees.h` labels the soprano and bass of each segment from
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

## Boundary with `rules/constraint_evaluate`

`rules/constraint.h`'s `constraint_evaluate` remains an untouched stub,
exactly as documented in `docs/modules/rules.md`: real evaluation logic
is reserved for a future L8 `CandidateEvaluator`. This module's own
triad-matching logic (`triad_identification.c`) is a separate,
self-contained implementation that never calls `constraint_evaluate` --
it uses `Rule`/`Constraint`/`Chord`/`Key` only for their existing
shape and metadata.

## Dependencies

Same ceiling as `Framework` itself: `identity` (L0), `core` (L1),
`time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5),
`context`/`semantics`/`relationship`/`interpretation` (L6) -- plus
`theory/framework.h`, `rules/rule.h`, and `rules/constraint.h`
specifically. Enforced by
`tests/test_architecture_common_practice_minimal.c`.

## Neutral analysis input

`common_practice_member_from_analysis_note` (`weighted_identification.h`) makes a `CommonPracticeMember` from an `AnalysisNote` and the segment's span: `held` when the note starts before the segment, `duration` the overlap and `onset` the note start, in whole notes as `double` (`rational_to_double`), `pitch` the note's `midi`. `key_regions.h` maps a region's key to an `AnalysisKey` (`common_practice_key_region_analysis_key`) and an `AnalysisKeyEvent` to a `CommonPracticeKeySignatureEvent` (`common_practice_key_signature_event_from_analysis`).
