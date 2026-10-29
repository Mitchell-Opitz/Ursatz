# Analysis chain

`ursatz_chain` is the library that joins the kernel (`src/`) and the Theory Frameworks (`frameworks/`) for analysis. A framework never compiles kernel sources (R-241), so anything that needs both lives here. Headers are included as `ursatz_chain/<name>.h`.

## Layer

An extension above the numbered stack. It may include kernel L0 to L8 and the framework headers listed in `tests/test_architecture_chain.c`. It never includes L9 (`io`, `export`, `persistence`, `corpus`, `corpus_stats`, `query`). `architecture_chain` enforces this.

## Contents

- `subset_chord_analyzer.h/.c`: `chain_subset_chord_analyze` and `CHAIN_SUBSET_CHORD_ANALYZER`, the subset-match chord analyzer (not the chord identification owner; kept for the subset coverage reports).
- `key_collection_analyzer.h/.c`: `chain_key_collection_analyze` and `CHAIN_KEY_COLLECTION_ANALYZER`, the pitch-class-collection key analyzer (not the key identification owner).
- `form_bridge.h/.c`: `chain_form_boundary_detect` (standard-form's boundary detector, over the kernel's Phrase Boundary Detection) and `chain_form_match_windows` (standard-form's window matcher, over the kernel's Motif Window Matching).

## Targets

`ursatz_chain` links `ursatz` and the frameworks the chain uses. `architecture_framework_sources` (`cmake/check_framework_sources.cmake`) fails when a framework or this folder names a source under `src/`.
