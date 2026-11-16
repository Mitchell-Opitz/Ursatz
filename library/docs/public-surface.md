# Public surface

How library targets expose their public headers and which tests guard it. [Docs index](README.md)

## Public surface

A library target exposes only the headers it names (R-223). `ursatz_public_headers` (`cmake/UrsatzPublicHeaders.cmake`) copies each public header at configure time to `build/public/<target>/<prefix>/<path>` and makes that folder the target's public include directory. The target's own source directories are private. Re-running configure stages an edited header again.

| Target | Included as | Example |
|---|---|---|
| `ursatz` (kernel) | its module folder, no prefix | `time/meter_map.h`, `analysis/motive_sketch.h` |
| a framework | its folder name, then the path in the framework | `standard-form/analysis/analysis_motive.h`, `common-practice-minimal/key_regions.h` |
| `ursatz_chain` | `ursatz_chain/` | `ursatz_chain/analysis_chain.h` |

A framework or chain source that includes another framework's header uses that path, never the bare name, so the four names that exist in more than one framework (`analysis_claim.h`, `analysis_input.h`, `analysis_phrase.h`, `framework.h`) mean one header each. A sibling or a header of the same framework stays bare. standard-counterpoint's `rule_pair.h` stays private.

The public set of a target is: every header the apps include, the owners named on this page, every header a framework or the chain takes from another target, every header those include, and nothing named `*_internal.h`. `internal_expansion_technique.h` is a plain enum and is public. A framework with nothing in the set stages nothing.

Three headers no consumer includes are not staged: `io/smf_reader.h` (kernel), `form_bridge.h` and `kernel_owners.h` (chain). The chain's own sources and tests still reach them through their source directory. A clean stage holds 97 kernel headers and 2 chain headers (`analysis_chain.h`, `chain_inputs.h`).

The accessors only tests read are not in the public headers. The struct of such a type is in a `*_internal.h` header beside it (never staged), and the tests read it through `tests/support/*_test_access.h` (chain, framework, interpretation, preference, relationship, semantics, structure), which `ursatz_test_includes` puts on every test's include path. An application cannot include either.

A test sees the internals of what it tests (`ursatz_test_includes()`); an app does not.

Two tests and the layer table guard this: `architecture_public_headers` (a public header includes only public headers of its own or a linked target), `public_headers_compile` (a generated file per target includes every public header and compiles with only the staged folders of the target and its links), the layer table (test `layer_table`) allows no bare include of another framework's header.
