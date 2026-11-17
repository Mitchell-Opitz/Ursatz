# containers

[Docs index](../README.md): the Ursatz docs by topic.

Layer L4 (`music-containers`). Path `src/containers/`, tests `tests/containers/`. Depends on `identity`, `core`, `time`, `pitch`, and `events` only. Must not depend on anything L5+.

## Provides

`Section` (`containers/section.h`): groups phrase refs. Schema `{id, phrase_ids}`, the array optional.

`id_array` (`containers/id_array.h`): the shared copy and destroy of an `EntityID` array, and `container_shell_create`, which checks and copies the ids an L5 shell holds.

## Notes

- `Section` has no mutator. A revision is a new construction, not an in-place field change.
- Its identity is its `EntityID` only, with the referenced ids excluded (they are content, not identity).
- Every reference field (`phrase_ids`) is an `EntityID`, held by reference only -- never an embedded/resolved copy of the referenced object. Resolving a reference to its actual object is a registry/repository concern (L9), out of scope here, same treatment `VoiceReference` gets.
- An empty `phrase_ids` array (count `0`, `NULL`) is valid, not an error -- the same "found nothing is not an error" treatment the Composer's `CandidateSet` gives its own arrays. This is a deliberate difference from `structure`'s `Structure`, which requires a non-empty `event_ids`.
- `FormStructureDetection` (`analysis/form_structure_detection.h`) builds `Section`s directly with `section_create`.

## Known Gaps

- **`Phrase` naming**: `structure`'s `PhraseOccurrence` is a situated instance of an abstract phrase, named by a phrase id; the kernel has no phrase container of its own. A `Section` groups those phrase ids.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), and `events` (L3) only. The layer table enforces the ceiling.
