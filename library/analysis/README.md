# Analysis

The analysis group (`library/analysis/`, target `ursatz_analysis`): the neutral analysis types and segmentation, the
kernel's analyses, similarity scoring, and the analysis adapter of every Theory Framework (what was `analysis/` inside
each framework folder, with common-practice-minimal's perception and all of standard-texture). The group depends on
`theory` and `model`; the chain (`chain/`) joins it with the inputs a run needs. Headers are included by their path from
`library/` (`analysis/harmony/analysis_cadence.h`). The theory side of each framework is in
[theory/README.md](../theory/README.md).

## Areas and the former framework adapters each holds

| Area | Holds | Former framework adapters |
|---|---|---|
| `notes/` | `AnalysisNote`, `AnalysisSegment`, `SpelledKey`, `AnalysisVoiceGrid`, sonority segmentation, contour and rhythm patterns | kernel `semantics` |
| `motive/` | Motive sketches, relations, links, tone roles, variant recognition, window matching | kernel `analysis` |
| `voices/` | Outer voices, voice separation | kernel `analysis` |
| `harmony/` | Chord segments to functions, cadences, six-fours, prolongation; weighted chord identification, key regions, non-chord tones, harmonic spans, Roman numeral and applied-chord adapters; progression patterns; chord readings of sonorities | standard-harmony, standard-tonality, common-practice-minimal |
| `form/` | Phrase, idea, repeat, similarity, theme, expansion and motive analysis; form structure, phrase boundaries, thematic returns | standard-form |
| `meter/` | Meter position, syncopation, hemiola, hypermeter, density, checks, tuplets; beat grid length | standard-meter |
| `texture/` | Texture rules, classification and features | standard-texture |
| `counterpoint/` | Melody and motion analysis, perfects, grid | standard-counterpoint |
| `galant/` | Schema matching, sequence detection | galant-schemas, scale-harmonization |
| `similarity/` | Similarity model and scoring over sequences | kernel `query` |

Each former framework's README is in the area that holds most of its code, with its scope line naming both halves:
common-practice-minimal's is [harmony/README.md](harmony/README.md), standard-texture's is
[texture/README.md](texture/README.md); the others are in [theory/](../theory/README.md).

## Conventions

- **Neutral input.** An adapter accepts the neutral types of `notes/` through mapping functions that copy field for
  field, so a caller writes no converter.
- **Results.** An adapter's results are plain typed structs (a claim struct holds the fields of one finding, with an
  uncertainty); no claim objects are built (P62 B11). The stages that hand a group of notes on build a `Structure` shell
  with `notes/note_group.h`. What a reader says about a realization is a reading (`library/reading`).
- **Owner injection.** An adapter that needs another analysis takes it as an injected callback and never re-derives it
  ([analytic-job-owners.md](../docs/analytic-job-owners.md)).
- **Building and testing.** The root build compiles `ursatz_analysis` and adds `library/analysis/tests/`, one folder
  per area. The chain-level tests are in `library/chain/tests/`.
- **Sonorities and chord readings.** The sonority is the base harmonic object and a chord is one reading of it
  (`harmony/chord_reading.h`). `SonorityPitches` holds the pitch classes sounding over a span and the lowest one;
  an aggregate no chord explains is a valid sonority. `ChordReading` adds a root, the chord type the knowledge
  snapshot gives for the pitch classes above it, the members by role, the bass, and the `borrowed_from`, `role`
  (functional or embellishing) and `expected` fields. `chord_reading_of_sonority` reads a sonority on a root; the
  chain gives each of its chords as a reading with `analysis_chain_chord_reading` (a cadential six-four is
  embellishing). No mixture, common-tone or elision reading fills the other fields yet, and nothing writes readings
  until the reading layer of P62.
