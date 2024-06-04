// Frontend logic only — no styling decisions here, no markup beyond what
// index.html already defines. Theming lives entirely in style.css.

const TOOLS = [
  { id: "library", label: "Library", active: true, render: renderLibraryView },
  { id: "corpus", label: "Collections", active: false, render: renderCorpusView },
  { id: "generator", label: "Generator", active: false, comingSoon: true },
];

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

function selectTool(toolId) {
  for (const tool of TOOLS) {
    tool.active = tool.id === toolId;
  }
  renderToolList();
  const selected = TOOLS.find((tool) => tool.id === toolId);
  if (selected && selected.render) {
    selected.render();
  }
}

function getContentEl() {
  return document.getElementById("content");
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

async function renderLibraryView() {
  const content = getContentEl();
  content.innerHTML = `
    <h1 class="view-title">Library</h1>
    <div class="upload-box">
      <div class="upload-box-text">
        <div class="upload-box-heading">Drop a MIDI or MusicXML file to analyze</div>
        <div class="upload-box-subheading">.mid · .midi · .musicxml</div>
      </div>
      <div class="upload-box-controls">
        <input type="file" id="library-upload-file" accept=".mid,.midi,.musicxml" />
        <button id="library-upload-button" type="button">Upload MIDI</button>
        <button type="button" disabled title="Not yet available">Import from URL</button>
      </div>
    </div>
    <div id="library-upload-status" class="upload-status"></div>
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

  wireUploadControl("library-upload", async () => {
    await loadLibraryPieces();
  });

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
      actionsCell.appendChild(buildActionButton({ icon: "↻", label: "Re-run analysis", disabled: true }));
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

async function handleUpload(idPrefix, onUploaded) {
  const fileInput = document.getElementById(`${idPrefix}-file`);
  const uploadButton = document.getElementById(`${idPrefix}-button`);
  const statusEl = document.getElementById(`${idPrefix}-status`);

  const file = fileInput.files[0];
  if (!file) {
    statusEl.textContent = "Select a file first.";
    statusEl.classList.add("error");
    return;
  }

  statusEl.textContent = "";
  statusEl.classList.remove("error");
  uploadButton.disabled = true;
  statusEl.textContent = "Uploading and analyzing...";

  try {
    const bytes = await file.arrayBuffer();
    const res = await fetch("/api/analyzer/pieces", {
      method: "POST",
      headers: { "X-Filename": encodeURIComponent(file.name) },
      body: bytes,
    });
    const data = await res.json();

    uploadButton.disabled = false;
    if (!data.ok) {
      statusEl.textContent = data.message || "Upload failed.";
      statusEl.classList.add("error");
      return;
    }

    statusEl.textContent = `Uploaded ${formatPieceLabel(data.id, data.filename)}.`;
    fileInput.value = "";
    await onUploaded(data);
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
    uploadButton.disabled = false;
  }
}

const PROVENANCE_LABELS = {
  present: "Hand-entered",
  absent: "Analyzed",
};

/* Combines a piece's autogenerated id with its original upload filename
 * (when known) so multiple loaded pieces can be told apart -- the id
 * alone gives no clue which uploaded file it came from. */
function formatPieceLabel(id, filename) {
  return filename ? `${filename} (${id})` : id;
}

/* Analysis view tabs, each mapped to the real interpretation `type`
 * values GET /api/analyzer/pieces/<id> returns (see analysis_service.c's
 * kIndexType* constants) -- Form folds in thematic_return (Thematic
 * Return Detection shares Form Structure Detection's Relationship-typed
 * lineage) and Periods/Sentences folds in both period and sentence,
 * each pair sharing one tab and one count badge. */
const ANALYSIS_TABS = [
  { id: "chords", label: "Chords", types: ["chord"] },
  { id: "keys", label: "Keys", types: ["key"] },
  { id: "cadences", label: "Cadences", types: ["cadence"] },
  { id: "motifs", label: "Motifs", types: ["motif"] },
  { id: "phrases", label: "Phrases", types: ["phrase"] },
  { id: "form", label: "Form", types: ["form", "thematic_return"] },
  { id: "periods-sentences", label: "Periods/Sentences", types: ["period", "sentence"] },
];

/* No analyzer-registry endpoint exists yet -- this mirrors the static
 * name/version list shown in the mockup rather than fetching real
 * analyzer identity. */
const ANALYZERS = [
  { name: "harmony", version: "2.4.1" },
  { name: "keyfinder", version: "1.8.0" },
  { name: "phrasing", version: "0.6.3" },
  { name: "form", version: "0.3.0" },
];

function renderAnalysisBackLink() {
  const back = document.createElement("a");
  back.textContent = "← Library";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => selectTool("library"));
  return back;
}

function renderAnalyzersList() {
  const listEl = document.getElementById("analysis-analyzers");
  listEl.innerHTML = "";
  for (const analyzer of ANALYZERS) {
    const li = document.createElement("li");
    li.textContent = `${analyzer.name} ${analyzer.version}`;
    listEl.appendChild(li);
  }
}

/* Buckets interpretations by ANALYSIS_TABS entry; an interpretation
 * whose type matches no tab (none do today) is silently dropped. */
function groupClaimsByTab(interpretations) {
  const groups = new Map();
  for (const tab of ANALYSIS_TABS) {
    groups.set(tab.id, []);
  }
  for (const interpretation of interpretations) {
    const tab = ANALYSIS_TABS.find((candidate) => candidate.types.includes(interpretation.type));
    if (tab) {
      groups.get(tab.id).push(interpretation);
    }
  }
  return groups;
}

function showAnalysisView(id) {
  const content = getContentEl();
  content.innerHTML = `
    <div id="analysis-back-container"></div>
    <h1 class="view-title" id="analysis-title">Loading...</h1>
    <div class="analysis-layout">
      <aside class="analysis-sidebar">
        <div class="sidebar-label">Analyzers</div>
        <ul id="analysis-analyzers" class="analyzers-list"></ul>
      </aside>
      <div class="analysis-main">
        <div id="analysis-status" class="piece-list-status">Loading piece ${id}...</div>
        <div id="analysis-tabs" class="analysis-tabs" hidden></div>
        <div id="analysis-claims" class="analysis-claims"></div>
        <div id="analysis-detail" class="analysis-detail" hidden></div>
      </div>
    </div>
  `;

  document.getElementById("analysis-back-container").appendChild(renderAnalysisBackLink());
  renderAnalyzersList();

  fetch(`/api/analyzer/pieces/${id}`)
    .then((res) => res.json())
    .then((data) => {
      const statusEl = document.getElementById("analysis-status");
      if (!data.ok) {
        statusEl.textContent = data.message === "piece not found" ? "Piece not found" : data.message || "Piece not found";
        statusEl.classList.add("error");
        return;
      }

      statusEl.textContent = "";
      document.getElementById("analysis-title").textContent = data.filename || data.id;
      renderAnalysisTabs(data);
    })
    .catch((err) => {
      const statusEl = document.getElementById("analysis-status");
      statusEl.textContent = `Could not reach backend: ${err}`;
      statusEl.classList.add("error");
    });
}

function renderAnalysisTabs(data) {
  const tabsEl = document.getElementById("analysis-tabs");
  const claimsEl = document.getElementById("analysis-claims");
  const detailEl = document.getElementById("analysis-detail");
  const groups = groupClaimsByTab(data.interpretations || []);

  tabsEl.innerHTML = "";
  tabsEl.hidden = false;

  function selectTab(tabId) {
    for (const button of tabsEl.querySelectorAll("button")) {
      button.classList.toggle("active", button.dataset.tabId === tabId);
    }
    detailEl.hidden = true;
    renderClaimList(claimsEl, groups.get(tabId) || [], detailEl);
  }

  for (const tab of ANALYSIS_TABS) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.tabId = tab.id;
    button.textContent = `${tab.label} ${groups.get(tab.id).length}`;
    button.addEventListener("click", () => selectTab(tab.id));
    tabsEl.appendChild(button);
  }

  selectTab(ANALYSIS_TABS[0].id);
}

function renderClaimList(claimsEl, claims, detailEl) {
  claimsEl.innerHTML = "";

  if (claims.length === 0) {
    const empty = document.createElement("div");
    empty.classList.add("piece-list-status");
    empty.textContent = "No claims in this category.";
    claimsEl.appendChild(empty);
    return;
  }

  const list = document.createElement("ul");
  list.classList.add("claim-list");
  for (const claim of claims) {
    const li = document.createElement("li");
    li.classList.add("clickable");

    li.textContent = `${claim.label} — `;
    const flag = document.createElement("span");
    flag.classList.add("provenance-flag");
    flag.textContent = PROVENANCE_LABELS[claim.provenance_state] || claim.provenance_state;
    li.appendChild(flag);

    li.addEventListener("click", () => renderClaimDetail(detailEl, claim));
    list.appendChild(li);
  }
  claimsEl.appendChild(list);
}

/* Evidence is always empty (no Evidence data exists behind this
 * endpoint -- Registry marks Evidence Not Started) and Provenance is
 * limited to produced_by.author/method and provenance_state (no full
 * Provenance record -- see UrsatzGuiInterpretationSummary in
 * analysis_service.h). Rendered explicitly rather than omitted so the
 * empty state is visible, not silently missing. */
function renderClaimDetail(detailEl, claim) {
  detailEl.hidden = false;
  detailEl.innerHTML = "";

  const summary = document.createElement("div");
  summary.classList.add("claim-detail-summary");
  summary.textContent = `${claim.type}: ${claim.label}`;
  detailEl.appendChild(summary);

  const evidenceHeading = document.createElement("div");
  evidenceHeading.classList.add("sidebar-label");
  evidenceHeading.textContent = "Evidence";
  detailEl.appendChild(evidenceHeading);

  const evidenceBody = document.createElement("div");
  evidenceBody.classList.add("claim-detail-empty");
  evidenceBody.textContent = "No evidence data returned by the analyzer.";
  detailEl.appendChild(evidenceBody);

  const provenanceHeading = document.createElement("div");
  provenanceHeading.classList.add("sidebar-label");
  provenanceHeading.textContent = "Provenance";
  detailEl.appendChild(provenanceHeading);

  const provenanceBody = document.createElement("div");
  provenanceBody.textContent = `${claim.produced_by.author} / ${claim.produced_by.method} (${claim.provenance_state})`;
  detailEl.appendChild(provenanceBody);

  const actions = document.createElement("div");
  actions.classList.add("claim-detail-actions");

  const acceptButton = document.createElement("button");
  acceptButton.type = "button";
  acceptButton.textContent = "Accept";
  acceptButton.disabled = true;
  acceptButton.title = "Not yet available";
  actions.appendChild(acceptButton);

  const disputeButton = document.createElement("button");
  disputeButton.type = "button";
  disputeButton.textContent = "Dispute";
  disputeButton.disabled = true;
  disputeButton.title = "Not yet available";
  actions.appendChild(disputeButton);

  detailEl.appendChild(actions);
}

async function renderCorpusView() {
  const content = getContentEl();
  content.innerHTML = `
    <div class="collections-header">
      <h1 class="view-title">Collections</h1>
      <button id="corpus-new-button" type="button">New Collection</button>
    </div>
    <div id="corpus-create-form" class="upload-controls" hidden>
      <input type="text" id="corpus-id-input" placeholder="id" />
      <input type="text" id="corpus-name-input" placeholder="name" />
      <button id="corpus-create-button" type="button">Create</button>
    </div>
    <div id="corpus-create-status" class="upload-status"></div>
    <div id="corpus-list-status" class="piece-list-status">Loading collections...</div>
    <div id="corpus-grid" class="collections-grid"></div>
  `;

  document.getElementById("corpus-new-button").addEventListener("click", () => {
    document.getElementById("corpus-create-form").hidden = false;
    document.getElementById("corpus-id-input").focus();
  });
  document.getElementById("corpus-create-button").addEventListener("click", handleCreateCorpus);

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
  const idInput = document.getElementById("corpus-id-input");
  const nameInput = document.getElementById("corpus-name-input");
  const createButton = document.getElementById("corpus-create-button");
  const statusEl = document.getElementById("corpus-create-status");

  const id = idInput.value.trim();
  const name = nameInput.value.trim();
  if (!id || !name) {
    statusEl.textContent = "Enter both an id and a name.";
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
      body: JSON.stringify({ id, name }),
    });
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = data.message || "Create failed.";
      statusEl.classList.add("error");
      createButton.disabled = false;
      return;
    }

    showCorpusDetail(id, name);
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
      <input type="text" id="corpus-piece-input" placeholder="piece id" />
      <button id="corpus-add-piece-button" type="button">Add</button>
    </div>
    <div id="corpus-piece-add-status" class="upload-status"></div>
  `;

  document.getElementById("corpus-back-container").appendChild(renderCorpusBackLink());
  document.getElementById("corpus-add-piece-button").addEventListener("click", () => handleAddPiece(id, name));

  loadCorpusPieces(id, name);
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

    if (data.pieces.length === 0) {
      statusEl.textContent = "No piece refs yet.";
      return;
    }

    const filenamesById = new Map();
    if (libraryData.ok) {
      for (const piece of libraryData.pieces) {
        filenamesById.set(piece.id, piece.filename);
      }
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
  return buildPieceRow(piece, (row, actionsCell, rowPiece) => {
    actionsCell.appendChild(
      buildActionButton({
        icon: "🗑",
        label: "Remove from collection",
        onClick: () => handleRemovePiece(corpusId, corpusName, rowPiece.id),
      }),
    );
  });
}

async function handleAddPiece(corpusId, corpusName) {
  const pieceInput = document.getElementById("corpus-piece-input");
  const addButton = document.getElementById("corpus-add-piece-button");
  const statusEl = document.getElementById("corpus-piece-add-status");

  const pieceId = pieceInput.value.trim();
  if (!pieceId) {
    statusEl.textContent = "Enter a piece id.";
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

    pieceInput.value = "";
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

renderToolList();
checkHealth();
