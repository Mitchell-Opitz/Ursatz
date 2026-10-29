# Frameworks

Theory Framework extension modules. Each one lives under `frameworks/<name>/`, outside
`src/`, and builds on the kernel's `Framework`, `Rule` and `Constraint` types
(`src/theory/`, `src/rules/`) without changing them. The kernel never includes a framework
header. The one exception is `src/analysis/chord_identification.c` and `key_estimation.c`,
which are compiled into `common_practice_minimal`, not into `ursatz` (see
`docs/modules/analysis.md`).

Each framework's own `README.md` holds its full scope and exclusions. This page is the
catalog and the shared conventions.

## Catalog

| Framework | Covers | Depends on |
|---|---|---|
| `common-practice-minimal` | 7 diatonic triads, 4 diatonic seventh qualities, major and natural-minor keys. **The only framework the analyzers use today** | ursatz |
| `standard-notation` | Staff, clefs, ledger lines, stems, accidentals, half and whole steps | ursatz |
| `standard-markings` | Dynamics order, articulation classes, tempo words, repeat and D.C./D.S. playback order | ursatz |
| `western-periods` | The six conventional stylistic periods | ursatz |
| `standard-rhythm` | Note values, dots, ties, rests, tuplets | ursatz |
| `standard-meter` | Simple and compound meter, beaming, anacrusis, counting, hypermeter, syncopation | ursatz |
| `standard-tonality` | Scales, modes, solfege, key signatures, circle of fifths, intervals, triads, sevenths, chord symbols, Roman numerals | ursatz |
| `standard-texture` | Monophony, heterophony, homophony, polyphony | ursatz |
| `standard-voicing` | Strict SATB voicing of one chord | ursatz |
| `standard-counterpoint` | Species counterpoint, first through fifth | ursatz |
| `sixteenth-century-style` | Palestrina-style imitative polyphony | standard-counterpoint, ursatz |
| `high-baroque-fugue` | Fugal exposition | standard-counterpoint, ursatz |
| `baroque-ground-bass` | Ground bass, chaconne, passacaglia | standard-counterpoint, ursatz |
| `galant-schemas` | Gjerdingen's Galant schemas | standard-counterpoint, ursatz |
| `scale-harmonization` | Rule of the octave, sequences over a bass scale | galant-schemas, standard-counterpoint, ursatz |
| `standard-harmony` | Harmonic function, phrase model, cadences (two competing readings where the music has two: V to I6 as evaded cadence or prolongation, a dominant before a section start as half or authentic cadence, each with a share in thousandths), six-four chords, prolongation, tonicization, modulation | ursatz |
| `standard-form` | Form levels, motives, sentence, period, hybrid forms, phrase expansion and contraction | standard-harmony, ursatz |

Most of the frameworks after `common-practice-minimal` follow Open Music Theory, a free
textbook; each README names the chapters it covers.

## Shared conventions

- **Rule libraries, not analyzers.** Every framework except `common-practice-minimal` takes
  caller-supplied plain data (scale degrees, spellings, inversions, flags) and returns `bool`
  results and hard/soft violation bitmasks. Only parts of `standard-tonality` and
  `standard-texture` build an `Interpretation`. Nothing yet reads MIDI or `NoteEvent`s into
  these inputs. That bridge is the next piece of work (see the project roadmap).
- **Framework singleton.** Each exposes one lazily built `Framework` instance plus one
  `Rule`/`Constraint` pair per rule. Rules are checked with the framework's own
  `*_rule_holds()` functions. standard-harmony and standard-counterpoint also register an evaluation
  callback on each Constraint, so `constraint_evaluate()` runs the same check; the other
  frameworks never call it and their Constraints return `CONSTRAINT_EVAL_NOT_IMPLEMENTED`. They also
  build a `Preference` per soft rule, scored through `preference_rank`; the other frameworks have none.
- **Hard vs. soft.** Rules phrased as "must" are hard. Rules phrased as "limit", "avoid" or
  "prefer" are soft.
- **Dependency ceiling.** L0 to L6 plus `theory/framework.h`, `rules/rule.h` and
  `rules/constraint.h`, plus any framework listed in its row above. Each framework's
  `tests/test_architecture_<name>.c` enforces this from the root build; a new include edge
  updates that allowlist in the same commit. Input from L8 analysis is passed in by the
  caller as plain data, never included.
- **Vocabulary enums.** Names a framework needs (for example `CadenceType`, `VoicePart`) live
  as plain enums in `src/semantics/` and carry no framework claim. What each name means is the
  framework's convention. See `docs/modules/semantics.md`.

## Claims and rule pairs

A framework builds its durable claims through the kernel helpers in `interpretation/claim_interpretation.h`. Each `analysis/*claim.c` keeps its public names and is a one-line call with the framework's id and short name: `claim_interpretation_create` for the Interpretation, `claim_interpretation_note_group` for the note shell, `claim_interpretation_relationship` for the Relationship. A framework builds its rule and constraint pairs through `rules/rule_pair.h`: its `rule_pair.h` has `typedef RulePair <Framework>RulePair;` and a static inline `<framework>_build_rule_pair` that passes its framework and id prefix to `rule_pair_build` (or `rule_pair_build_typed` for hard and soft rules). A framework needs no claim or rule pair code of its own.

## Building and testing

The root build compiles every framework library and adds each framework's `tests/` directory, so
root `ctest` and CI run every framework test, its architecture test included (since PR #99). A
framework built as the top-level project still runs its own tests.
