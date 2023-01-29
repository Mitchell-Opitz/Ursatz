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
  Fully-diminished7 is deliberately excluded -- it requires the raised
  leading tone of harmonic minor, which this pass does not cover (see
  below).
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
- Fully-diminished7 specifically (needs harmonic minor's raised leading
  tone -- planned alongside harmonic minor support, not before)
- Modal interchange / borrowed chords
- Secondary dominants and other tonicization (including chromatic
  seventh chords such as secondary-dominant sevenths)
- Harmonic minor and melodic minor (natural minor only, this pass)
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

## Known gap: non-chord tones

`triad_identification.c` and `seventh_identification.c` match by subset:
a triad or seventh matches when its degrees or intervals are all present,
and any extra degrees are ignored as non-chord tones. Ties go to the first
match in scan order and are logged to stderr. This tolerates one extra tone
per beat. Real melody-plus-accompaniment textures often carry two or more,
and then nothing matches (Field Nocturne No. 1 resolves 61 of 394 beats).
The fix is weighting by duration and beat strength, which is open work.

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