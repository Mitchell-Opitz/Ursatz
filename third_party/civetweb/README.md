# civetweb (vendored)

civetweb is an embeddable C HTTP server. It is vendored here for the GUI backend (`gui/backend/src/server.c`), which serves the frontend and the routes on it.

- Project: civetweb, https://github.com/civetweb/civetweb
- Copy: tag v1.16, with one upstream fix applied (see Notes for maintainers)
- License: MIT, in `LICENSE.md` (it must stay next to the code)

| file | SHA-256 |
|---|---|
| `include/civetweb.h` | `e3a12c0a659b3bc6f75aa67799232ca343572dcb3add8b2475666633fa4afad3` |
| `src/civetweb.c` | `6c219878e931b730aa940ae2d063fbb85e29e614fc2def2854d226df23724d3a` |
| `LICENSE.md` | `bc24af9ffb777f4842f8556ac752efd5ea212c55d2eccfae58cc2c2a24ea1f03` |

## How it is built

`CMakeLists.txt` builds it as the static target `civetweb-c-library` with `NO_SSL` (the GUI's server needs none), `USE_IPV6`, `USE_STACK_SIZE=33554432` (the Composer API runs on the request threads and some entries need more than the default stack) and `-w` (warnings off for this target only).

## Notes for maintainers

- Upstream tag v1.16 is `d7ba35bbb649209c66e582d5a0244ba988a15159`. Every other file is byte-identical to it.
- One patch is applied to `src/civetweb.c`: the upstream fix for CVE-2025-55763 (commit `76e222bcb77ba8452e5da4e82ae6cecd499c25e0`, "Fix heap overflow in directory URI slash redirection", with its style follow-up `d5321963b1d0bc953101de91f8588bf83db73bf5`, PR #1347). It replaces the `strcat` calls in `handle_request` step 12 (directory URIs get a trailing slash) with bounded appends. v1.16 has no release with this fix.
- The server is bound to `127.0.0.1` only (`gui/backend/src/server.c`). No `[::1]` entry: v1.16 fails `mg_start` on a host without IPv6 when one is listed.
- Do not edit these files otherwise. If they must change, mark the change plainly in the file and in this README.
