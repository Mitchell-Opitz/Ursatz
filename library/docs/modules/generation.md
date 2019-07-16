# generation

Layer L8 (`music-generation`). Path `src/generation/`, tests `tests/generation/`. One of three mutually-restricted L8 siblings (with `transform` and `analysis`); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

`Intent`, `IntentCompiler`, `ConstraintExtraction`, `ConflictDetection`, `GenerationPlan`, `Generator`, `Candidate`/`CandidateSet`, `CandidateEvaluator`/`CandidateEvaluation`, `GenerationRecord`.

## Notes

- Natural language must not directly manipulate the IR (TDD §154). `Intent` is the only type a natural-language request compiles into, via `IntentCompiler` (a contract-only interface, no NLP implementation in this pass), and every downstream type (`ConstraintExtraction`, `GenerationPlan`, `Generator`) consumes `Intent`, never raw text.
- `ConflictDetection` never silently drops a conflict (TDD §156). It requires at least two conflicting ids per record (a conflict cannot be reported about a single requirement) and keeps `priority`/`severity`/`weight` as three distinct fields, not synonyms (TDD §157). A rank, a fixed-vocabulary category, and an exact-rational magnitude, each independently settable.
- `generation/generator.h` forward-declares `CandidateSet` as an opaque struct; `generation/candidate.h` supplies the real definition under the identical struct tag. `tests/generation/test_candidate.c` exercises both headers together to confirm they resolve to one type, not two conflicting ones.
- `CandidateEvaluator` reuses `Interpretation`, not `AnalysisResult` (TDD §159). `CandidateEvaluation` holds the identical evaluations/violations-as-`Interpretation`-arrays shape `AnalysisResult` gives its own findings, but is defined independently in `generation/` rather than importing `analysis/analyzer.h`; the L8 sideways restriction forbids that import regardless of the shape being identical.
- `GenerationRecord` is distinct from `CandidateSet`'s and `Generator`'s own seed/version fields (TDD §163). `generation_record.h` is the durable, standalone log entry that a generation run happened. `CandidateSet` carries the same reproducibility bundle (seed, `generator_version`, `configuration`, `input_ids`, `framework_version`) scoped to the one `generate()` call that produced it, and `Generator`'s own `version` field is a declared capability, present whether or not the generator was ever invoked.
- `framework_version` is a known gap, not fabricated. TDD §163 requires it on `CandidateSet` and `GenerationRecord`, but `Framework` (L7) carries no version field of its own yet, the same unresolved-dependency treatment `theory`'s notes give `Interpretation`'s missing `framework` field. Both types keep it as an optional, opaque string (`NULL` when not available) rather than block on that gap.

## Deferred

`ConstraintSolver`, `Search`/`Optimizer` (Registry non-normative, reference concept only). Hierarchical Generators (`ThemeGenerator`, `PhraseGenerator`, `BasicIdeaGenerator`, Registry Phase 13, not 12). `PracticeRequest`/`PracticeGenerator`, `DifficultyModel` (belong to L9 Applications, not L8).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and both L7 modules only. Never imports `transform` or `analysis`.
