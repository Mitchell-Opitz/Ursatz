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

The backend exposes one route today: `GET /api/health/status`, which
calls through the persistence service to confirm `persistence_open`
succeeds against a test database path, and returns JSON.

## Frontend

Plain static HTML/JS/CSS served by the backend from `frontend/public/`.
No framework or build pipeline at this stage. Theming lives entirely in
`style.css` as CSS custom properties on `:root`, so a future theme
picker can swap colors without touching markup or `app.js`. Frontend
logic (`app.js`) makes no styling decisions.

## Building

```
cmake -B build
cmake --build build
ctest --test-dir build
```

By default the build fetches Ursatz from git. To develop against a local
Ursatz checkout instead:

```
cmake -B build -DURSATZ_SOURCE_DIR=/path/to/ursatz
```

## Out of scope for this scaffold

Real add-MIDI/analyze/list-saved-pieces flows, native webview packaging,
Generator tool code, and authentication are explicitly not part of this
task.
