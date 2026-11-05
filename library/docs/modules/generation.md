# generation

[Docs index](../README.md): the Ursatz docs by topic.

Layer L8 (`music-generation`). Path `src/generation/`, tests `tests/generation/`. One of three mutually-restricted L8 siblings (with `transform` and `analysis`); see `docs/modules/l8-notes.md` for the sibling rule and shared L8 notes.

## Provides

`Intent`, `IntentCompiler`, `ConstraintExtraction`, `ConflictDetection`, `GenerationPlan`, `Generator`, `Candidate`/`CandidateSet`, `CandidateEvaluator`/`CandidateEvaluation`, `GenerationRecord`, `GeneratorRegistry`, `generator_dispatch`.

## Notes

- Natural language must not directly manipulate the IR (TDD §154). `Intent` is the only type a natural-language request compiles into, via `IntentCompiler` (a contract-only interface, no NLP implementation in this pass), and every downstream type (`ConstraintExtraction`, `GenerationPlan`, `Generator`) consumes `Intent`, never raw text.
- `ConflictDetection` never silently drops a conflict (TDD §156). It requires at least two conflicting ids per record (a conflict cannot be reported about a single requirement) and keeps `priority`/`severity`/`weight` as three distinct fields, not synonyms (TDD §157). A rank, a fixed-vocabulary category, and an exact-rational magnitude, each independently settable.
- `generation/generator.h` forward-declares `CandidateSet` as an opaque struct; `generation/candidate.h` supplies the real definition under the identical struct tag. `tests/generation/test_candidate.c` exercises both headers together to confirm they resolve to one type, not two conflicting ones.
- `CandidateEvaluator` reuses `Interpretation`, not `AnalysisResult` (TDD §159). `CandidateEvaluation` holds the identical evaluations/violations-as-`Interpretation`-arrays shape `AnalysisResult` gives its own findings, but is defined independently in `generation/` rather than importing `analysis/analyzer.h`; the L8 sideways restriction forbids that import regardless of the shape being identical.
- `GenerationRecord` is distinct from `CandidateSet`'s and `Generator`'s own seed/version fields (TDD §163). `generation_record.h` is the durable, standalone log entry that a generation run happened. `CandidateSet` carries the same reproducibility bundle (seed, `generator_version`, `configuration`, `input_ids`, `framework_version`) scoped to the one `generate()` call that produced it, and `Generator`'s own `version` field is a declared capability, present whether or not the generator was ever invoked.
- `framework_version` is a known gap, not fabricated. TDD §163 requires it on `CandidateSet` and `GenerationRecord`, but `Framework` (L7) carries no version field of its own yet, the same unresolved-dependency treatment `theory`'s notes give `Interpretation`'s missing `framework` field. Both types keep it as an optional, opaque string (`NULL` when not available) rather than block on that gap.

## Generator contract for consumer apps

A concrete Generator is a module that implements `GeneratorFn` and exposes it, with its declared metadata, as a `Generator` value (`generation/generator.h`). It is not part of this library: `src/` ships no concrete Generator. A consumer app (Composer, a GUI, a script) registers the Generators it has and runs them by name through `generation/generator_registry.h` and `generation/generator_dispatch.h`.

**Registration.**
- `generator_registry_create`/`_destroy`, `generator_registry_register`, `generator_registry_lookup` (by exact, case-sensitive name), `generator_registry_count` and `generator_registry_at` (registration order). There is no unregister.
- The registry borrows: it never copies or frees a `Generator`, so each one, and every string it points at, must outlive the registry.
- Registration rejects a Generator whose name or version is empty, whose `generate` is NULL, or any of whose four descriptor strings (`supported_inputs`, `supported_outputs`, `configuration_description`, `constraints_description`) is NULL (`GENERATOR_REGISTRY_ERR_INVALID_GENERATOR`), and a name that is already registered (`ERR_DUPLICATE_NAME`, nothing changes).
- Not thread-safe: build the registry, then read it.

**Dispatch.** `generator_dispatch(registry, name, plan, random_seed, configuration, &set, &record)` runs the named Generator once, with the plan, seed and configuration exactly as given (configuration is optional), and returns the `CandidateSet` and a `GenerationRecord` together. Both are the caller's to destroy.
- On any failure both outputs are untouched and there is no record: `ERR_NULL_ARGUMENT`, `ERR_UNKNOWN_GENERATOR`, `ERR_GENERATION_FAILED` (the Generator returned a failure, or OK without a set), `ERR_ALLOC`, `ERR_CONTRACT_VIOLATION`.
- `ERR_CONTRACT_VIOLATION`: the returned set records another `random_seed` or `generator_version` than the run used, which would break reproducibility; the set is destroyed.
- The plan is never modified, by dispatch or by a Generator (TDD §140).

**Seed and reproducibility.**
- The seed is required on every call. A deterministic Generator (`deterministic == true`) may ignore it, but must return an identical `CandidateSet` for the same plan, seed and configuration. A stochastic Generator (`deterministic == false`) must consume the seed and reproduce the same set from it, so a different seed gives a different set.
- Dispatch does not call `generate()` twice to check: determinism is a contract each Generator's own tests enforce (`tests/generation/test_generator_reproducibility.c` shows how, including that the check catches a Generator that declares determinism it does not have).

**The record** (`generation_record.h`, TDD §163), one per successful run:
- `random_seed`: the seed passed in. `generator_version`: the Generator's version.
- `configuration`: an owned copy of the configuration passed in, or NULL.
- `framework_version`: NULL. A `GenerationPlan` carries no framework and `Framework` has no version field yet (the known gap above); dispatch will read it from the plan's frameworks when both exist. The record says "not available" rather than inventing a version.
- `provenance`: present, referring to the plan's id.
- `id`: `generation-record:<plan id>:<generator name>:<version>:<seed>:<configuration hash>`, the hash being FNV-1a 64 over the configuration's type string, a zero byte and its content bytes, in hex (`none` with no configuration). The same plan, Generator, version, seed and configuration give the same id, so a record names a run, not an occasion.
- Re-running with the record's seed and configuration against the same Generator version reproduces the set.

**What dispatch does not do.** It does not evaluate or rank candidates (`CandidateEvaluator`, L8; Constraint and Preference are not consulted), choose a Generator, retry, persist the record, or compile an Intent. It borrows the registry's Generators and the caller's plan and configuration for the duration of the call only.

## Deferred

`ConstraintSolver`, `Search`/`Optimizer` (Registry non-normative, reference concept only). Hierarchical Generators (`ThemeGenerator`, `PhraseGenerator`, `BasicIdeaGenerator`, Registry Phase 13, not 12). `PracticeRequest`/`PracticeGenerator`, `DifficultyModel` (belong to L9 Applications, not L8).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), all four L6 modules, and both L7 modules only. Never imports `transform` or `analysis`.
