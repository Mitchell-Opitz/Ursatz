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

The subset-match chord analyzer and the key collection analyzer that were once compiled into this
target now live in the analysis chain (`chain/ursatz_chain/`); this framework compiles no kernel
source.

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
  `chain/ursatz_chain/subset_chord_analyzer.c` already documents).

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

Weighted identification and chords in context are documented in [common-practice-minimal-weighted.md](../../docs/modules/common-practice-minimal-weighted.md).

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

## Theory owners

`common_practice_home_key_region` (`key_regions.h`) gives the home key of a piece from its regions: the key with the longest total duration, the earliest on a tie, summed on checked `Rational`. `common_practice_ranked_chord_quality_name` and `common_practice_ranked_chord_inversion` (`weighted_identification.h`) name a ranked chord's quality and place its bass among its tones. `common_practice_scale_degree_of` (`outer_scale_degrees.h`) gives a pitch class's scale degree (1 to 7) and alteration (-1, 0, 1) as numbers; `common_practice_scale_degree_label` is built over it.

## Segment bass, harmonic spans (R-370 to R-373, R-376)

`CommonPracticeSegment` can carry its own bass (`has_bass`, `bass_pitch`), `CommonPracticeWeightedConfig.require_bass_chord_tone` limits the candidates to chords holding it, and `seventh_penalty` (default 0) is subtracted from every seventh candidate's score (R-376). `harmonic_spans.h` reads each segment's bass and groups segments into spans, one chord each. The chain sets all of these for spans. The rules are in [common-practice-minimal-weighted.md](../../docs/modules/common-practice-minimal-weighted.md).

## Neutral analysis input

`common_practice_member_from_analysis_note` (`weighted_identification.h`) makes a `CommonPracticeMember` from an `AnalysisNote` and the segment's span: `held` when the note starts before the segment, `duration` the overlap and `onset` the note start, in whole notes as `double` (`rational_to_double`), `pitch` the note's `midi`. `key_regions.h` maps a region's key to an `AnalysisKey` (`common_practice_key_region_analysis_key`) and an `AnalysisKeyEvent` to a `CommonPracticeKeySignatureEvent` (`common_practice_key_signature_event_from_analysis`).

Leftover tones are labelled by `common_practice_label_leftover_tones`, the fallback when no classifier is injected. `CommonPracticeWeightedConfig.classifier` takes a `NonChordToneClassifier` (`semantics/tone_line.h`; the owner is `motive_tone_roles_line`): the leftover members that have a `voice_id` are labelled per voice, as a line of the voice's members with the chord mask of the segment each sounds in; members without a voice and a classifier that refuses keep the framework rule.
