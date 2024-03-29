# Ursatz GUI

A local web UI for the Ursatz platform. This repo is a host/shell around
independent tool modules, not a monolithic application.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`) — the only code
  allowed to call into Ursatz's persistence/query functions. Every tool
  goes through this layer; nothing calls Ursatz directly from route code.
  This boundary is the one thing this scaffold is strict about, because
  it is expensive to untangle later.
- **Tool modules** (`backend/src/routes/`) — independent GUI features
  (Analysis/Corpus now, Generator later). Each tool registers its own
  HTTP routes under an `/api/<tool_name>/...` prefix and never calls
  another tool directly. See `backend/include/ursatz_gui/tool.h` for the
  registration convention, and `backend/src/routes/health.c` as the
  reference tool.
- **Host/shell** (`backend/src/server.c`, `frontend/public/`) — starts
  the HTTP server, wires tool routes together, and serves the static
  frontend shell (top bar, tool sidebar, content area) that tool UIs
  render into.

New tool code goes in `backend/src/routes/`. New shared persistence or
query logic goes in `backend/src/service/`. If you find yourself calling
an Ursatz function from inside `routes/`, it belongs in `service/`
instead.

## Backend

C HTTP/JSON-RPC-style server using [civetweb](https://github.com/civetweb/civetweb)
(MIT licensed), fetched via CMake `FetchContent`. Civetweb was chosen over
hand-rolled sockets or heavier alternatives (e.g. libmicrohttpd, mongoose)
because it's a small, permissively licensed, single-purpose embedded HTTP
library with a simple C API and no GPL/commercial dual-licensing
complications.

The backend exposes:

- `GET /api/health/status`: calls through the persistence service to
  confirm `persistence_open` succeeds against a test database path,
  and returns JSON.
- `POST /api/analyzer/pieces`: accepts a raw MIDI file as the request
  body, runs it through Ursatz's `MIDIImporter`, `ChordIdentification`,
  and `KeyEstimation` (against the `common-practice-minimal`
  framework) via the analysis service, saves the resulting NoteEvent
  sequence and Interpretations, and returns the new piece's id.
- `GET /api/analyzer/pieces`: lists saved piece ids.

Ursatz's persistence layer has no "piece" concept (one `note_events`
table, one `interpretations` table, no grouping/filename metadata): the
Analyzer tool works around this by saving each piece to its own SQLite
file under `pieces/`, using the filename as the piece id. Listing scans
that directory rather than querying a tracking table.

## Frontend

Plain static HTML/JS/CSS served by the backend from `frontend/public/`.
No framework or build pipeline at this stage. Theming lives entirely in
`style.css` as CSS custom properties on `:root`, so a future theme
picker can swap colors without touching markup or `app.js`. Frontend
logic (`app.js`) makes no styling decisions.

## Building

```
git submodule update --init --recursive
cmake -B build
cmake --build build
ctest --test-dir build
```

Ursatz is vendored as a git submodule at `external/ursatz`, same pattern
as `ursatz-analyzer`. `git submodule update --init --recursive` (or
`git clone --recurse-submodules` up front) is required before the first
build.

## Out of scope for this scaffold

Native webview packaging, Generator tool code, re-running analysis on
an already-saved piece, and authentication are explicitly not part of
this task.
