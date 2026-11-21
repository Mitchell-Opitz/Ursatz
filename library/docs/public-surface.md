# Public surface

How library targets expose their public headers and which tests guard it. [Docs index](README.md)

## Public surface

A library target exposes only the headers it names (R-223). `ursatz_public_headers` (`cmake/UrsatzPublicHeaders.cmake`) copies each public header at configure time to `build/public/<target>/<path>` and makes that folder the target's public include directory. The target's own source directories are private. Re-running configure stages an edited header again.

| Target | Group folder | Included as | Staged headers | Example |
|---|---|---|---|---|
| `ursatz_model` | `library/model/` | `model/<area>/<file>.h` | 48 | `model/time/meter_map.h` |
| `ursatz_theory` | `library/theory/` | `theory/<area>/<file>.h` | 137 | `theory/harmony/harmony_chord.h` |
| `ursatz_analysis` | `library/analysis/` | `analysis/<area>/<file>.h` | 78 | `analysis/motive/motive_sketch.h` |
| `ursatz_io` | `library/io/` | `io/<file>.h` | 7 | `io/midi_importer.h` |
| `ursatz_chain` | `library/chain/` | `chain/<file>.h` | 2 | `chain/analysis_chain.h` |

Every group includes headers by their path from `library/`, so a header means one file whichever target includes it. Sources of a group see the whole of `library/`; a consumer outside the group sees only the staged headers.

**The narrowing rule.** A group stages the include closure of the library headers that something outside the group includes, and nothing else: an application's sources, API headers or tests, or a library file of another group (a library test does not count, it sees the library folders). A header only its own group includes stays private. Nothing named `*_internal.h` is staged. The staged set is 272 headers (model 48, theory 137, analysis 78, io 7, chain 2), against 254 before the groups.

The layer table holds the rule (`cmake/layer-table.txt`, `cmake/check_layer_table.py`, test `layer_table`). A `target` row names the modules one CMake target builds. An include between two library modules of different targets, or from an application, must name a staged header; an include within one target is free. A second check, with the staged manifest, reports every staged header that no include from outside its target reaches, directly or through another staged header: `layer-table: <header> is staged but nothing outside <target> includes it`. A change that makes a header public or private edits the group's `HEADERS` list in its `CMakeLists.txt` in the same commit.

Headers no consumer includes are not staged, among them `io/smf_reader.h`, `chain/form_bridge.h` and `chain/kernel_owners.h`. The chain's own sources and tests reach them through the library folders.

The accessors only tests read are not in the public headers. The struct of such a type is in a `*_internal.h` header beside it (never staged), and the tests read it through `tests/support/*_test_access.h` (chain, interpretation, relationship, semantics, structure), which `ursatz_test_includes` puts on every test's include path. An application cannot include either.

A test sees the internals of what it tests (`ursatz_test_includes()`, which puts `library/`, `tests/support` and the test folder's parent on the path); an app does not.

Two tests and the layer table guard this: `architecture_public_headers` (a public header includes only public headers of its own or a linked target), `public_headers_compile` (a generated file per target includes every public header and compiles with only the staged folders of the target and its links), and the layer table (test `layer_table`, the narrowing rule above).
