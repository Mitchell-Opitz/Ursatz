# Public surface

How library targets expose their public headers and which tests guard it. [Docs index](README.md)

## Public surface

A library target exposes only the headers it names (R-223). `ursatz_public_headers` (`cmake/UrsatzPublicHeaders.cmake`) copies each public header at configure time to `build/public/<target>/<prefix>/<path>` and makes that folder the target's public include directory. The target's own source directories are private. Re-running configure stages an edited header again.

| Target | Included as | Example |
|---|---|---|
| `ursatz` (kernel) | its module folder, no prefix | `time/meter_map.h`, `analysis/motive_sketch.h` |
| a framework | its folder name, then the path in the framework | `standard-form/analysis/analysis_motive.h`, `common-practice-minimal/key_regions.h` |
| `ursatz_chain` | `ursatz_chain/` | `ursatz_chain/analysis_chain.h` |

A framework or chain source that includes another framework's header uses that path, never the bare name, so the eight names that exist in more than one framework (`analysis_claim.h`, `analysis_input.h`, `analysis_phrase.h`, `framework.h`, `hierarchy_rules.h`, `key_signature_rules.h`, `rule_pair.h`, `stem_rules.h`) mean one header each. A sibling or a header of the same framework stays bare. `rule_pair.h` stays private in each framework.

The public set of a target is: every header the apps include (`docs/public-headers-baseline.txt`, made by `scripts/app_includes.sh <app checkout>...`), the owners named on this page, every header a framework or the chain takes from another target, every header those include, and nothing named `*_internal.h`. `internal_expansion_technique.h` is a plain enum and is public. A framework with nothing in the set stages nothing.

A test sees the internals of what it tests (`ursatz_test_includes()`); an app does not.

Three tests guard this: `architecture_public_headers` (a public header includes only public headers of its own or a linked target), `public_headers_compile` (a generated file per target includes every public header and compiles with only the staged folders of the target and its links), `architecture_header_paths` (no bare include of another framework's header).
