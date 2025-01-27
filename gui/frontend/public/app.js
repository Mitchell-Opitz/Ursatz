// Frontend logic only — no styling decisions here, no markup beyond what
// index.html already defines. Theming lives entirely in style.css.

const TOOLS = [
  { id: "library", label: "Library", active: true, render: renderLibraryView },
  { id: "analysis", label: "Analysis", active: false, render: renderAnalysisTool },
  { id: "corpus", label: "Collections", active: false, render: renderCorpusView },
  { id: "composer", label: "Composer", active: false, render: () => renderComposerView() },
  { id: "health", label: "Health", active: false, render: renderHealthView },
];

/* The piece id last opened in the Analysis view (via a Library filename
 * click, or by returning to this tab), so the top-level Analysis nav
 * entry can reopen it instead of showing a blank page. null until a
 * piece has been opened at least once this session. */
let lastAnalysisPieceId = null;

function renderToolList() {
  const list = document.getElementById("tool-list");
  list.innerHTML = "";
  for (const tool of TOOLS) {
    const li = document.createElement("li");
    li.textContent = tool.comingSoon ? `${tool.label} (soon)` : tool.label;
    if (tool.active) {
      li.classList.add("active");
    }
    if (tool.render) {
      li.classList.add("clickable");
      li.addEventListener("click", () => selectTool(tool.id));
    }
    list.appendChild(li);
  }
}

function setActiveTool(toolId) {
  for (const tool of TOOLS) {
    tool.active = tool.id === toolId;
  }
  renderToolList();
}

function selectTool(toolId) {
  leaveWorkspace();
  setActiveTool(toolId);
  const selected = TOOLS.find((tool) => tool.id === toolId);
  if (selected && selected.render) {
    selected.render();
  }
}

function getContentEl() {
  return document.getElementById("content");
}

/* Sends a request and resolves to {ok, status, data}; data is null for an
 * answer that is not JSON. ok is false for an error status or a body with an
 * "error" field. A network failure resolves to {ok:false, status:0,
 * data:{error}}. Shared by the views that talk to the Composer routes. */
async function fetchJsonResult(url, options) {
  try {
    const res = await fetch(url, options);
    let data = null;
    try {
      data = await res.json();
    } catch (err) {
      data = null;
    }
    return { ok: res.ok && !(data && data.error), status: res.status, data };
  } catch (err) {
    return { ok: false, status: 0, data: { error: `Could not reach backend: ${err}` } };
  }
}

function postJsonResult(url, body) {
  return fetchJsonResult(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
}

/* Fetches a MusicXML file and draws it with OSMD (vendored under
 * vendor/opensheetmusicdisplay/, loaded globally as
 * window.opensheetmusicdisplay -- see index.html) into containerEl; progress
 * and errors go to statusEl. Resolves to {osmd, xmlText}, or null when it
 * could not be drawn. zoom is optional (small previews). */
async function drawMusicXmlScore(url, containerEl, statusEl, zoom) {
  statusEl.textContent = "Loading score...";
  statusEl.classList.remove("error");
  containerEl.innerHTML = "";
  try {
    const res = await fetch(url);
    const xmlText = await res.text();
    const problem = ScoreNotes.scoreResponseProblem(res.status, res.headers.get("Content-Type") || "", xmlText);
    if (problem) {
      statusEl.textContent = `Could not load score: ${problem}`;
      statusEl.classList.add("error");
      return null;
    }
    const osmd = new opensheetmusicdisplay.OpenSheetMusicDisplay(containerEl);
    await osmd.load(xmlText);
    if (zoom) {
      osmd.zoom = zoom;
    }
    osmd.render();
    statusEl.textContent = "";
    return { osmd, xmlText };
  } catch (err) {
    statusEl.textContent = `Could not load score: ${err}`;
    statusEl.classList.add("error");
    return null;
  }
}

async function checkHealth() {
  const el = document.getElementById("health-status");
  try {
    const res = await fetch("/api/health/status");
    const data = await res.json();
    el.textContent = data.message;
    el.classList.add(data.ok ? "ok" : "error");
  } catch (err) {
    el.textContent = `Could not reach backend: ${err}`;
    el.classList.add("error");
  }
}

function renderHealthView() {
  const content = getContentEl();
  content.innerHTML = `
    <section class="panel">
      <h1>Backend Health</h1>
      <div id="health-status" class="health-status">Checking...</div>
    </section>
  `;
  checkHealth();
}

async function renderLibraryView() {
  const content = getContentEl();
  content.innerHTML = `
    <h1 class="view-title">Library</h1>
    <div class="upload-box">
      <div class="upload-box-text">
        <div class="upload-box-heading">Drop MIDI or MusicXML files to analyze</div>
        <div class="upload-box-subheading">.mid · .midi · .musicxml · .xml · .mxl</div>
      </div>
      <div class="upload-box-controls">
        <input type="file" id="library-upload-file" accept=".mid,.midi,.musicxml,.xml,.mxl" multiple />
        <button id="library-upload-button" type="button">Upload files</button>
        <button type="button" disabled title="Not yet available">Import from URL</button>
      </div>
    </div>
    <div id="library-upload-status" class="upload-status"></div>
    <ul id="library-upload-list" class="upload-list"></ul>
    <div class="filter-row">
      <label class="filter-field">
        Filter
        <select disabled>
          <option>All composers</option>
        </select>
      </label>
      <select disabled>
        <option>All statuses</option>
      </select>
      <label class="filter-field">
        Sort
        <select disabled>
          <option>Date added</option>
        </select>
      </label>
    </div>
    <section class="panel library-panel">
      <div id="library-status" class="piece-list-status">Loading pieces...</div>
      <table id="library-table" class="library-table" hidden>
        <thead>
          <tr>
            <th>Title</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="library-table-body"></tbody>
      </table>
    </section>
  `;

  const onUploaded = async () => {
    await loadLibraryPieces();
  };
  wireUploadControl("library-upload", onUploaded);
  wireUploadDrop("library-upload", onUploaded);

  await loadLibraryPieces();
}

async function loadLibraryPieces() {
  const statusEl = document.getElementById("library-status");
  const tableEl = document.getElementById("library-table");
  const bodyEl = document.getElementById("library-table-body");

  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");
  tableEl.hidden = true;
  bodyEl.innerHTML = "";

  try {
    const res = await fetch("/api/analyzer/pieces");
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = `Failed to load pieces: ${data.message || "unknown error"}`;
      statusEl.classList.add("error");
      return;
    }

    if (data.pieces.length === 0) {
      statusEl.textContent = "No pieces yet.";
      return;
    }

    statusEl.textContent = "";
    tableEl.hidden = false;
    for (const piece of data.pieces) {
      bodyEl.appendChild(buildLibraryRow(piece));
    }
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

/* Icon-only action button. icon is a single glyph (no icon font/CDN --
 * plain Unicode, consistent with this app's no-external-assets style);
 * label is the accessible name, shown as both aria-label and a hover
 * tooltip so the icon-only button stays identifiable. */
function buildActionButton({ icon, label, disabled, onClick }) {
  const button = document.createElement("button");
  button.type = "button";
  button.classList.add("icon-button");
  button.textContent = icon;
  button.setAttribute("aria-label", label);
  button.title = disabled ? "Not yet available" : label;
  if (disabled) {
    button.disabled = true;
  } else if (onClick) {
    button.addEventListener("click", onClick);
  }
  return button;
}

/* Shared piece-row visual style (filename cell + actions cell), reused by
 * any view that lists pieces in this table layout. Each caller supplies
 * its own action buttons via buildActions(row, actionsCell, piece) --
 * this function owns only the markup/styling, not what a view can do to
 * a piece. */
function buildPieceRow(piece, buildActions, onFilenameClick) {
  const row = document.createElement("tr");

  const filenameCell = document.createElement("td");
  const filenameEl = renderPieceFilenameCell(piece);
  if (onFilenameClick) {
    filenameEl.classList.add("clickable");
    filenameEl.addEventListener("click", () => onFilenameClick(piece));
  }
  filenameCell.appendChild(filenameEl);
  row.appendChild(filenameCell);

  const actionsCell = document.createElement("td");
  actionsCell.classList.add("library-actions");
  buildActions(row, actionsCell, piece);
  row.appendChild(actionsCell);

  return row;
}

function renderPieceFilenameCell(piece) {
  const span = document.createElement("span");
  span.textContent = piece.filename || piece.id;
  return span;
}

function buildLibraryRow(piece) {
  return buildPieceRow(
    piece,
    (row, actionsCell, rowPiece) => {
      actionsCell.appendChild(buildActionButton({ icon: "▶", label: "Play", disabled: true }));
      actionsCell.appendChild(
        buildActionButton({ icon: "✎", label: "Rename", onClick: () => startLibraryRename(row, rowPiece) }),
      );
      actionsCell.appendChild(
        buildActionButton({
          icon: "↻",
          label: "Re-run analysis",
          onClick: () => handleLibraryReanalyze(rowPiece.id),
        }),
      );
      actionsCell.appendChild(buildActionButton({ icon: "🔗", label: "Import from URL", disabled: true }));
      actionsCell.appendChild(buildActionButton({ icon: "⌫", label: "Delete analysis", disabled: true }));
      actionsCell.appendChild(
        buildActionButton({
          icon: "🗑",
          label: "Delete piece",
          onClick: () => handleDeleteLibraryPiece(rowPiece.id, row),
        }),
      );
    },
    (rowPiece) => showAnalysisView(rowPiece.id),
  );
}

function startLibraryRename(row, piece) {
  const filenameCell = row.querySelector("td");
  filenameCell.innerHTML = "";

  const input = document.createElement("input");
  input.type = "text";
  input.value = piece.filename || "";
  filenameCell.appendChild(input);
  input.focus();

  const saveButton = document.createElement("button");
  saveButton.type = "button";
  saveButton.textContent = "Save";
  saveButton.addEventListener("click", () => handleLibraryRename(row, piece, input.value.trim()));
  filenameCell.appendChild(saveButton);

  const cancelButton = document.createElement("button");
  cancelButton.type = "button";
  cancelButton.textContent = "Cancel";
  cancelButton.addEventListener("click", () => {
    filenameCell.innerHTML = "";
    filenameCell.appendChild(renderPieceFilenameCell(piece));
  });
  filenameCell.appendChild(cancelButton);
}

async function handleLibraryRename(row, piece, newFilename) {
  const statusEl = document.getElementById("library-status");

  if (!newFilename) {
    statusEl.textContent = "Enter a filename.";
    statusEl.classList.add("error");
    return;
  }

  try {
    const res = await fetch(`/api/analyzer/pieces/${piece.id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ filename: newFilename }),
    });
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = data.message || "Rename failed.";
      statusEl.classList.add("error");
      return;
    }

    statusEl.textContent = "";
    statusEl.classList.remove("error");
    piece.filename = data.filename;
    const filenameCell = row.querySelector("td");
    filenameCell.innerHTML = "";
    filenameCell.appendChild(renderPieceFilenameCell(piece));
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

/* Runs the Analyzer again on the piece's stored source
 * (POST /api/analyzer/pieces/<id>/reanalyze). Resolves to an error message,
 * or null on success. */
async function reanalyzePiece(id) {
  try {
    const res = await fetch(`/api/analyzer/pieces/${id}/reanalyze`, { method: "POST" });
    const data = await res.json();
    if (res.status === 404) {
      return "Piece not found.";
    }
    if (res.status === 422) {
      return "The stored file could not be analyzed.";
    }
    if (!data.ok) {
      return data.message || "Re-run failed.";
    }
    return null;
  } catch (err) {
    return `Could not reach backend: ${err}`;
  }
}

async function handleLibraryReanalyze(id) {
  const statusEl = document.getElementById("library-status");
  statusEl.textContent = "Re-running analysis...";
  statusEl.classList.remove("error");
  const error = await reanalyzePiece(id);
  if (error) {
    statusEl.textContent = error;
    statusEl.classList.add("error");
    return;
  }
  statusEl.textContent = "Analysis updated.";
}

async function handleDeleteLibraryPiece(id, rowEl) {
  if (!window.confirm("Delete this piece? This removes the file and all of its analyses.")) {
    return;
  }

  const statusEl = document.getElementById("library-status");

  try {
    const res = await fetch(`/api/analyzer/pieces/${id}`, { method: "DELETE" });
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = data.message || "Delete failed.";
      statusEl.classList.add("error");
      return;
    }

    rowEl.remove();
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

/* Wires the "<idPrefix>-file"/"<idPrefix>-button"/"<idPrefix>-status"
 * upload control markup already in the DOM (see renderLibraryView's
 * .upload-box) to the shared
 * upload handler below. onUploaded(data) runs after a successful
 * upload, once, with the parsed response body -- each view decides what
 * "uploaded" means for it (jump to the new piece's detail, or just
 * refresh a list) without duplicating the fetch/status-message logic. */
function wireUploadControl(idPrefix, onUploaded) {
  document
    .getElementById(`${idPrefix}-button`)
    .addEventListener("click", () => handleUpload(idPrefix, onUploaded));
}

/* Dropping files on the .upload-box runs the same queue as the picker.
 * Names the picker would not offer stay in the list as failed entries. */
function wireUploadDrop(idPrefix, onUploaded) {
  const box = document
    .getElementById(`${idPrefix}-button`)
    .closest(".upload-box");
  box.addEventListener("dragover", (event) => {
    event.preventDefault();
    box.classList.add("is-dragover");
  });
  box.addEventListener("dragleave", () => box.classList.remove("is-dragover"));
  box.addEventListener("drop", (event) => {
    event.preventDefault();
    box.classList.remove("is-dragover");
    const files = Array.from(event.dataTransfer.files);
    if (files.length === 0) return;
    if (document.getElementById(`${idPrefix}-button`).disabled) return;
    runUploadQueue(idPrefix, files, onUploaded);
  });
}

function handleUpload(idPrefix, onUploaded) {
  const fileInput = document.getElementById(`${idPrefix}-file`);
  const statusEl = document.getElementById(`${idPrefix}-status`);

  const files = Array.from(fileInput.files);
  if (files.length === 0) {
    statusEl.textContent = "Select a file first.";
    statusEl.classList.add("error");
    return Promise.resolve();
  }
  fileInput.value = "";
  return runUploadQueue(idPrefix, files, onUploaded);
}

/* Uploads the files one after another with the existing POST (analysis is
 * CPU bound, so no parallel requests). The list under the box shows one
 * line per file, the status line the summary. onUploaded(data) runs after
 * each success so the table fills in as files finish. */
async function runUploadQueue(idPrefix, files, onUploaded) {
  const uploadButton = document.getElementById(`${idPrefix}-button`);
  const statusEl = document.getElementById(`${idPrefix}-status`);
  const listEl = document.getElementById(`${idPrefix}-list`);

  let queue = UploadQueue.createQueue(files);
  const show = () => {
    listEl.innerHTML = "";
    for (const entry of queue) {
      const item = document.createElement("li");
      item.classList.add(`upload-${entry.state}`);
      item.textContent = uploadEntryText(entry);
      listEl.appendChild(item);
    }
    statusEl.textContent = UploadQueue.queueSummary(queue);
    statusEl.classList.toggle(
      "error",
      queue.some((entry) => entry.state === "failed")
    );
  };

  uploadButton.disabled = true;
  show();
  for (let i = 0; i < files.length; i++) {
    if (!UploadQueue.acceptedFile(files[i].name)) {
      queue = UploadQueue.markFailed(queue, i, "not a MIDI or MusicXML file");
      show();
      continue;
    }
    queue = UploadQueue.markAnalyzing(queue, i);
    show();
    try {
      const bytes = await files[i].arrayBuffer();
      const res = await fetch("/api/analyzer/pieces", {
        method: "POST",
        headers: { "X-Filename": encodeURIComponent(files[i].name) },
        body: bytes,
      });
      const data = await res.json();
      if (!data.ok) {
        queue = UploadQueue.markFailed(queue, i, data.message || "Upload failed.");
        show();
        continue;
      }
      queue = UploadQueue.markDone(queue, i, formatPieceLabel(data.id, data.filename));
      show();
      await onUploaded(data);
    } catch (err) {
      queue = UploadQueue.markFailed(queue, i, `Could not reach backend: ${err}`);
      show();
    }
  }
  uploadButton.disabled = false;
}

function uploadEntryText(entry) {
  switch (entry.state) {
    case "analyzing":
      return `${entry.name}: analyzing...`;
    case "done":
      return `${entry.name}: uploaded as ${entry.label}`;
    case "failed":
      return `${entry.name}: failed: ${entry.message}`;
    default:
      return `${entry.name}: queued`;
  }
}

/* Combines a piece's autogenerated id with its original upload filename
 * (when known) so multiple loaded pieces can be told apart -- the id
 * alone gives no clue which uploaded file it came from. */
function formatPieceLabel(id, filename) {
  return filename ? `${filename} (${id})` : id;
}

/* The tabs and their rows come from analysis-tabs.js (ANALYSIS_TABS,
 * buildAllTabStates), loaded before this file. */

function renderAnalysisBackLink() {
  const back = document.createElement("a");
  back.textContent = "← Library";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => selectTool("library"));
  return back;
}

/* Entry point for the top-level Analysis nav tab: reopens whichever
 * piece was last viewed (via a Library filename click, or a prior visit
 * to this tab) so the tab isn't blank on return. Before any piece has
 * been opened this session, there is nothing to reopen -- shows a
 * prompt pointing at Library instead of guessing a piece. */
function renderAnalysisTool() {
  if (lastAnalysisPieceId) {
    showAnalysisView(lastAnalysisPieceId);
    return;
  }

  const content = getContentEl();
  content.innerHTML = `
    <h1 class="view-title">Analysis</h1>
    <div id="analysis-empty-container"></div>
    <div class="piece-list-status">No piece selected yet. Open one from the Library to see its analysis here.</div>
  `;
  document.getElementById("analysis-empty-container").appendChild(renderAnalysisBackLink());
}

/* The workspace mounted in the content area while a piece is open (see
 * showAnalysisView). */
let analysisWorkspace = null;

/* Closes the open workspace, if any, and gives the content area back its
 * padding. */
function leaveWorkspace() {
  if (analysisWorkspace) {
    analysisWorkspace.destroy();
    analysisWorkspace = null;
  }
  getContentEl().classList.remove("content--workspace");
}

function showAnalysisView(id) {
  lastAnalysisPieceId = id;
  setActiveTool("analysis");
  leaveWorkspace();

  const content = getContentEl();
  content.classList.add("content--workspace");
  content.innerHTML = `<div id="analysis-status" class="piece-list-status ws-loading">Loading piece ${id}...</div>`;

  fetch(`/api/analyzer/pieces/${id}`)
    .then((res) => res.json())
    .then((data) => {
      if (lastAnalysisPieceId !== id || content.querySelector(".ws")) {
        return;
      }
      if (!data.ok) {
        showAnalysisProblem(data.message === "piece not found" ? "Piece not found" : data.message || "Piece not found");
        return;
      }
      mountAnalysisWorkspace(content, id, data);
    })
    .catch((err) => {
      showAnalysisProblem(`Could not reach backend: ${err}`);
    });
}

function showAnalysisProblem(message) {
  const statusEl = document.getElementById("analysis-status");
  if (!statusEl) {
    return;
  }
  statusEl.textContent = message;
  statusEl.classList.add("error");
  const back = renderAnalysisBackLink();
  back.classList.add("ws-loading");
  statusEl.after(back);
}

/* Format, measure count and note count under the title
 * (buildAnalysisHeader in analysis-tabs.js). */
function analysisSubtitle(header) {
  return [
    header.format,
    header.measureCount === null ? "" : `${header.measureCount} measures`,
    `${header.noteCount} notes`,
  ]
    .filter((part) => part !== "")
    .join(" · ");
}

/* Mounts the score workspace for a piece. Score mode is the default; Data
 * mode holds the Provenance list and the claim tabs. */
function mountAnalysisWorkspace(content, id, data) {
  const header = buildAnalysisHeader(data);
  const workspace = ScoreWorkspace.mount(content, {
    title: header.filename,
    subtitle: analysisSubtitle(header),
    musicxmlUrl: `/api/analyzer/pieces/${id}/musicxml`,
    metaUrl: `/api/analyzer/pieces/${id}/musicxml/meta`,
    playbackUrl: `/api/analyzer/pieces/${id}/playback`,
    document: data.document,
    noteXmlIds: data.note_xml_ids,
    onBack: () => selectTool("library"),
    actions: [
      {
        label: "Re-run analysis",
        onClick: async (event) => {
          const button = event.currentTarget;
          button.disabled = true;
          workspace.setStatus("Re-running analysis...", false);
          const error = await reanalyzePiece(id);
          if (error) {
            button.disabled = false;
            workspace.setStatus(error, true);
            return;
          }
          showAnalysisView(id);
        },
      },
    ],
  });
  analysisWorkspace = workspace;

  renderDataMode(workspace.dataEl(), data);
}

/* Data mode: the Provenance list, then the claim tabs. */
function renderDataMode(dataEl, data) {
  dataEl.innerHTML = `
    <div class="sidebar-label">Provenance</div>
    <dl id="analysis-provenance" class="provenance-list"></dl>
    <div id="analysis-tabs" class="analysis-tabs"></div>
    <div id="analysis-claims" class="analysis-claims"></div>
  `;
  renderProvenancePanel(data.document);
  renderAnalysisTabs(data);
}

/* The metadata fields of the analysis document (buildProvenanceFields). */
function renderProvenancePanel(analysisDocument) {
  const listEl = document.getElementById("analysis-provenance");
  listEl.innerHTML = "";
  for (const [label, value] of buildProvenanceFields(analysisDocument)) {
    const term = document.createElement("dt");
    term.textContent = label;
    const detail = document.createElement("dd");
    detail.textContent = value;
    listEl.appendChild(term);
    listEl.appendChild(detail);
  }
}

function renderAnalysisTabs(data) {
  const tabsEl = document.getElementById("analysis-tabs");
  const claimsEl = document.getElementById("analysis-claims");
  const states = buildAllTabStates(data.document);

  function selectTab(tabId) {
    for (const button of tabsEl.querySelectorAll("button")) {
      button.classList.toggle("active", button.dataset.tabId === tabId);
    }
    renderTabBlocks(claimsEl, states.find((state) => state.tab.id === tabId), data.note_xml_ids);
  }

  for (const state of states) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.tabId = state.tab.id;
    if (state.empty) {
      button.textContent = `${state.tab.label} (none)`;
      button.disabled = true;
    } else {
      button.textContent = `${state.tab.label} ${state.rowCount}`;
      button.addEventListener("click", () => selectTab(state.tab.id));
    }
    tabsEl.appendChild(button);
  }

  const firstFilled = states.find((state) => !state.empty);
  if (firstFilled) {
    selectTab(firstFilled.tab.id);
  } else {
    claimsEl.textContent = "This analysis has no claims.";
  }
}

/* A Data mode claim row was chosen: show it on the score. */
function showClaimOnScore() {
  if (analysisWorkspace) {
    analysisWorkspace.setMode("score");
  }
}

/* One tab: for each document section, a heading, the section's JSON block
 * when it is an object, and its rows. A row is a claim: label, where it
 * is, and a toggle for its remaining fields. Choosing a row shows its
 * claim on the score (showClaimOnScore). */
function renderTabBlocks(claimsEl, state, noteXmlIds) {
  claimsEl.innerHTML = "";

  for (const block of state.blocks) {
    if (block.kind === "empty") {
      continue;
    }

    const heading = document.createElement("div");
    heading.classList.add("sidebar-label");
    heading.textContent = block.name;
    claimsEl.appendChild(heading);

    if (block.kind === "object") {
      const pre = document.createElement("pre");
      pre.classList.add("json-block");
      pre.textContent = block.text;
      claimsEl.appendChild(pre);
    }

    if (block.rows.length > 0) {
      claimsEl.appendChild(buildClaimRows(block.rows, noteXmlIds));
    }
  }
}

function buildClaimRows(rows, noteXmlIds) {
  const list = document.createElement("ul");
  list.classList.add("claim-list");
  for (const row of rows) {
    const li = document.createElement("li");
    li.classList.add("clickable", "claim-row");

    const line = document.createElement("div");
    line.classList.add("claim-row-line");
    const label = document.createElement("span");
    label.classList.add("claim-row-label");
    label.textContent = row.label;
    const where = document.createElement("span");
    where.classList.add("claim-row-where");
    where.textContent = row.where;
    const toggle = document.createElement("button");
    toggle.type = "button";
    toggle.classList.add("claim-row-toggle");
    toggle.textContent = "Fields";
    line.appendChild(label);
    line.appendChild(where);
    line.appendChild(toggle);
    li.appendChild(line);

    const json = document.createElement("pre");
    json.classList.add("json-block");
    json.textContent = row.detail;
    json.hidden = true;
    li.appendChild(json);

    toggle.addEventListener("click", (event) => {
      event.stopPropagation();
      json.hidden = !json.hidden;
    });
    li.addEventListener("click", () => {
      showClaimOnScore();
    });
    list.appendChild(li);
  }
  return list;
}

async function renderCorpusView() {
  const content = getContentEl();
  content.innerHTML = `
    <div class="collections-header">
      <h1 class="view-title">Collections</h1>
      <div class="upload-controls">
        <input type="text" id="corpus-name-input" placeholder="New collection name" />
        <button id="corpus-create-button" type="button">Create</button>
      </div>
    </div>
    <div id="corpus-create-status" class="upload-status"></div>
    <div id="corpus-list-status" class="piece-list-status">Loading collections...</div>
    <div id="corpus-grid" class="collections-grid"></div>
  `;

  document.getElementById("corpus-create-button").addEventListener("click", handleCreateCorpus);
  document.getElementById("corpus-name-input").addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      handleCreateCorpus();
    }
  });

  const statusEl = document.getElementById("corpus-list-status");
  const gridEl = document.getElementById("corpus-grid");

  try {
    const res = await fetch("/api/corpus");
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = `Failed to load collections: ${data.message || "unknown error"}`;
      statusEl.classList.add("error");
      return;
    }

    if (data.corpora.length === 0) {
      statusEl.textContent = "No collections yet.";
      return;
    }

    statusEl.textContent = "";
    for (const corpus of data.corpora) {
      gridEl.appendChild(buildCollectionCard(corpus));
    }
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

function buildCollectionCard(corpus) {
  const card = document.createElement("div");
  card.classList.add("collection-card", "clickable");
  card.addEventListener("click", () => showCorpusDetail(corpus.id, corpus.name));

  const cover = document.createElement("div");
  cover.classList.add("collection-card-cover");
  card.appendChild(cover);

  const name = document.createElement("div");
  name.classList.add("collection-card-name");
  name.textContent = corpus.name;
  card.appendChild(name);

  return card;
}

async function handleCreateCorpus() {
  const nameInput = document.getElementById("corpus-name-input");
  const createButton = document.getElementById("corpus-create-button");
  const statusEl = document.getElementById("corpus-create-status");

  const name = nameInput.value.trim();
  if (!name) {
    statusEl.textContent = "Enter a name.";
    statusEl.classList.add("error");
    return;
  }

  statusEl.textContent = "";
  statusEl.classList.remove("error");
  createButton.disabled = true;

  try {
    const res = await fetch("/api/corpus", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name }),
    });
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = data.message || "Create failed.";
      statusEl.classList.add("error");
      createButton.disabled = false;
      return;
    }

    showCorpusDetail(data.id, data.name);
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
    createButton.disabled = false;
  }
}

function renderCorpusBackLink() {
  const back = document.createElement("a");
  back.textContent = "Back to list";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => renderCorpusView());
  return back;
}

function showCorpusDetail(id, name) {
  const content = getContentEl();
  content.innerHTML = `
    <div id="corpus-back-container"></div>
    <h1 class="view-title">${name}</h1>
    <section class="panel library-panel">
      <div id="corpus-piece-status" class="piece-list-status">Loading pieces...</div>
      <table id="corpus-piece-table" class="library-table" hidden>
        <thead>
          <tr>
            <th>Title</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="corpus-piece-table-body"></tbody>
      </table>
    </section>
    <div class="upload-controls">
      <select id="corpus-piece-select" disabled></select>
      <button id="corpus-add-piece-button" type="button" disabled>Add</button>
    </div>
    <div id="corpus-piece-add-status" class="upload-status"></div>
  `;

  document.getElementById("corpus-back-container").appendChild(renderCorpusBackLink());
  document.getElementById("corpus-add-piece-button").addEventListener("click", () => handleAddPiece(id, name));

  loadCorpusPieces(id, name);
}

/* Library pieces not already in the collection, labeled by filename with
 * the bare id as fallback (same as the row label), sorted by label. */
function collectionAddOptions(libraryPieces, memberIds) {
  const members = new Set(memberIds);
  return libraryPieces
    .filter((piece) => !members.has(piece.id))
    .map((piece) => ({ id: piece.id, label: piece.filename || piece.id }))
    .sort((a, b) => a.label.localeCompare(b.label));
}

function populateCorpusPieceSelect(libraryPieces, memberIds) {
  const selectEl = document.getElementById("corpus-piece-select");
  const addButton = document.getElementById("corpus-add-piece-button");
  if (!selectEl || !addButton) {
    return;
  }

  const options = collectionAddOptions(libraryPieces, memberIds);
  selectEl.innerHTML = "";
  if (options.length === 0) {
    const empty = document.createElement("option");
    empty.value = "";
    empty.textContent = "All Library pieces are in this collection";
    selectEl.appendChild(empty);
  }
  for (const option of options) {
    const optionEl = document.createElement("option");
    optionEl.value = option.id;
    optionEl.textContent = option.label;
    selectEl.appendChild(optionEl);
  }
  selectEl.disabled = options.length === 0;
  addButton.disabled = options.length === 0;
}

/* Corpus piece refs only carry an id (see corpus_service.h) -- this joins
 * them against /api/analyzer/pieces so the shared row component can show
 * the same filename Library shows, falling back to the bare id exactly
 * as Library's row does when a piece isn't found (e.g. deleted from the
 * Library since being added here). */
async function loadCorpusPieces(id, name) {
  const statusEl = document.getElementById("corpus-piece-status");
  const tableEl = document.getElementById("corpus-piece-table");
  const bodyEl = document.getElementById("corpus-piece-table-body");
  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");
  tableEl.hidden = true;
  bodyEl.innerHTML = "";

  try {
    const [corpusRes, libraryRes] = await Promise.all([
      fetch(`/api/corpus/${id}/pieces`),
      fetch("/api/analyzer/pieces"),
    ]);
    const data = await corpusRes.json();
    const libraryData = await libraryRes.json();

    if (!data.ok) {
      statusEl.textContent = `Failed to load pieces: ${data.message || "unknown error"}`;
      statusEl.classList.add("error");
      return;
    }

    const libraryPieces = libraryData.ok ? libraryData.pieces : [];
    populateCorpusPieceSelect(
      libraryPieces,
      data.pieces.map((piece) => piece.id),
    );

    if (data.pieces.length === 0) {
      statusEl.textContent = "No piece refs yet.";
      return;
    }

    const filenamesById = new Map();
    for (const piece of libraryPieces) {
      filenamesById.set(piece.id, piece.filename);
    }

    statusEl.textContent = "";
    tableEl.hidden = false;
    for (const piece of data.pieces) {
      const enrichedPiece = { id: piece.id, filename: filenamesById.get(piece.id) };
      bodyEl.appendChild(buildCollectionPieceRow(enrichedPiece, id, name));
    }
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

function buildCollectionPieceRow(piece, corpusId, corpusName) {
  return buildPieceRow(
    piece,
    (row, actionsCell, rowPiece) => {
      actionsCell.appendChild(
        buildActionButton({
          icon: "🗑",
          label: "Remove from collection",
          onClick: () => handleRemovePiece(corpusId, corpusName, rowPiece.id),
        }),
      );
    },
    (rowPiece) => showAnalysisView(rowPiece.id),
  );
}

async function handleAddPiece(corpusId, corpusName) {
  const pieceSelect = document.getElementById("corpus-piece-select");
  const addButton = document.getElementById("corpus-add-piece-button");
  const statusEl = document.getElementById("corpus-piece-add-status");

  const pieceId = pieceSelect.value;
  if (!pieceId) {
    statusEl.textContent = "Pick a piece to add.";
    statusEl.classList.add("error");
    return;
  }

  statusEl.textContent = "";
  statusEl.classList.remove("error");
  addButton.disabled = true;

  try {
    const res = await fetch(`/api/corpus/${corpusId}/pieces`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ piece_id: pieceId }),
    });
    const data = await res.json();

    addButton.disabled = false;
    if (!data.ok) {
      statusEl.textContent = data.message || "Add failed.";
      statusEl.classList.add("error");
      return;
    }

    loadCorpusPieces(corpusId, corpusName);
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
    addButton.disabled = false;
  }
}

async function handleRemovePiece(corpusId, corpusName, pieceId) {
  try {
    const res = await fetch(`/api/corpus/${corpusId}/pieces/${pieceId}`, { method: "DELETE" });
    const data = await res.json();

    if (!data.ok) {
      const statusEl = document.getElementById("corpus-piece-status");
      statusEl.textContent = data.message || "Remove failed.";
      statusEl.classList.add("error");
      return;
    }

    loadCorpusPieces(corpusId, corpusName);
  } catch (err) {
    const statusEl = document.getElementById("corpus-piece-status");
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

selectTool("library");
