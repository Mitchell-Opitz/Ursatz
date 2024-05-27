// Frontend logic only — no styling decisions here, no markup beyond what
// index.html already defines. Theming lives entirely in style.css.

const TOOLS = [
  { id: "analysis", label: "Analysis", active: true, render: renderAnalysisView },
  { id: "library", label: "Library", active: false, render: renderLibraryView },
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

async function renderAnalysisView() {
  const content = getContentEl();
  content.innerHTML = `
    <section class="panel">
      <h1>Analysis</h1>
      <div id="piece-list-status" class="piece-list-status">Loading pieces...</div>
      <ul id="piece-list" class="piece-list"></ul>
      <div class="upload-controls">
        <input type="file" id="upload-file" accept=".mid,.midi" />
        <button id="upload-button" type="button">Upload</button>
      </div>
      <div id="upload-status" class="upload-status"></div>
    </section>
  `;

  wireUploadControl("upload", (data) => showPieceDetail(data.id));

  const statusEl = document.getElementById("piece-list-status");
  const listEl = document.getElementById("piece-list");

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
    for (const piece of data.pieces) {
      const li = document.createElement("li");

      const label = document.createElement("span");
      label.textContent = formatPieceLabel(piece.id, piece.filename);
      label.classList.add("clickable");
      label.addEventListener("click", () => showPieceDetail(piece.id));
      li.appendChild(label);

      li.appendChild(document.createTextNode(" "));

      const deleteButton = document.createElement("button");
      deleteButton.type = "button";
      deleteButton.textContent = "Delete";
      deleteButton.addEventListener("click", () => handleDeletePiece(piece.id, li));
      li.appendChild(deleteButton);

      listEl.appendChild(li);
    }
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

async function handleDeletePiece(id, liEl) {
  const statusEl = document.getElementById("piece-list-status");

  try {
    const res = await fetch(`/api/analyzer/pieces/${id}`, { method: "DELETE" });
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = data.message || "Delete failed.";
      statusEl.classList.add("error");
      return;
    }

    liEl.remove();
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
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
function buildPieceRow(piece, buildActions) {
  const row = document.createElement("tr");

  const filenameCell = document.createElement("td");
  filenameCell.appendChild(renderPieceFilenameCell(piece));
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
  return buildPieceRow(piece, (row, actionsCell, rowPiece) => {
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
  });
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
 * upload control markup already in the DOM (see renderAnalysisView's
 * .upload-controls and renderLibraryView's .upload-box) to the shared
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

function renderBackLink() {
  const back = document.createElement("a");
  back.textContent = "Back to list";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => renderAnalysisView());
  return back;
}

function showPieceDetail(id) {
  const content = getContentEl();
  content.innerHTML = `
    <section class="panel">
      <h1>Piece Detail</h1>
      <div id="piece-detail-status" class="piece-detail-status">Loading piece ${id}...</div>
      <div id="piece-detail-body"></div>
    </section>
  `;

  const statusEl = document.getElementById("piece-detail-status");
  const bodyEl = document.getElementById("piece-detail-body");

  fetch(`/api/analyzer/pieces/${id}`)
    .then((res) => res.json())
    .then((data) => {
      if (!data.ok) {
        statusEl.textContent = data.message === "piece not found" ? "Piece not found" : data.message || "Piece not found";
        statusEl.classList.add("error");
        bodyEl.appendChild(renderBackLink());
        return;
      }

      statusEl.textContent = "";

      const heading = bodyEl.parentElement.querySelector("h1");
      if (heading) {
        heading.textContent = `Piece Detail: ${formatPieceLabel(data.id, data.filename)}`;
      }

      const noteCount = document.createElement("div");
      noteCount.textContent = `Note count: ${data.note_event_count}`;
      bodyEl.appendChild(noteCount);

      const list = document.createElement("ul");
      list.classList.add("interpretation-list");
      if (data.interpretations.length === 0) {
        const li = document.createElement("li");
        li.textContent = "No interpretations yet.";
        list.appendChild(li);
      } else {
        for (const interpretation of data.interpretations) {
          const li = document.createElement("li");
          const provenanceLabel = PROVENANCE_LABELS[interpretation.provenance_state] || interpretation.provenance_state;
          li.textContent = `${interpretation.type}: ${interpretation.label} — `;
          const flag = document.createElement("span");
          flag.classList.add("provenance-flag");
          flag.textContent = provenanceLabel;
          li.appendChild(flag);
          list.appendChild(li);
        }
      }
      bodyEl.appendChild(list);

      const relationshipList = document.createElement("ul");
      relationshipList.classList.add("relationship-list");
      if (!data.relationships || data.relationships.length === 0) {
        const li = document.createElement("li");
        li.textContent = "No relationships yet.";
        relationshipList.appendChild(li);
      } else {
        for (const relationship of data.relationships) {
          const li = document.createElement("li");
          li.textContent = `${relationship.source_id} —${relationship.predicate_name}→ ${relationship.target_id}`;
          relationshipList.appendChild(li);
        }
      }
      bodyEl.appendChild(relationshipList);

      bodyEl.appendChild(renderBackLink());
    })
    .catch((err) => {
      statusEl.textContent = `Could not reach backend: ${err}`;
      statusEl.classList.add("error");
      bodyEl.appendChild(renderBackLink());
    });
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
