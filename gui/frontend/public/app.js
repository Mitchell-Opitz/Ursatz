// Frontend logic only — no styling decisions here, no markup beyond what
// index.html already defines. Theming lives entirely in style.css.

const TOOLS = [
  { id: "library", label: "Library", active: true, render: renderLibraryView },
  { id: "analysis", label: "Analysis", active: false, render: renderAnalysisTool },
  { id: "corpus", label: "Collections", active: false, render: renderCorpusView },
  { id: "motifs", label: "Motifs", active: false, render: () => renderMaterialsView("motif") },
  { id: "themes", label: "Themes", active: false, render: () => renderMaterialsView("theme") },
  { id: "pieces", label: "Pieces", active: false, render: () => renderMaterialsView("piece") },
  { id: "subjects", label: "Subjects", active: false, render: renderSubjectsView },
  { id: "motive-links", label: "Motive links", active: false, render: renderMotiveLinksView },
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
  if (toolId !== "library" && tagsPanel) {
    tagsPanel.setActive(null);
  }
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

const UNASSIGNED = "__unassigned__";
const UNTAGGED = "__untagged__";

function emptyLibraryFilter() {
  return { composer: "", source: "", tag: "", unassigned: [], untagged: false, search: "", sort: "added" };
}

/* The Library's current filter: composer, source, tag, unassigned (list of
 * "composer"/"source"), untagged, plus client-side search and sort. */
let libraryFilter = emptyLibraryFilter();

/* The pieces currently shown, after search and sort. */
let libraryShownPieces = [];

async function renderLibraryView(filter) {
  libraryFilter = { ...emptyLibraryFilter(), ...(filter || {}) };
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
      <input type="search" id="library-search" placeholder="Search" aria-label="Search" />
      <label class="filter-field">Composer <select id="library-filter-composer"></select></label>
      <label class="filter-field">Source <select id="library-filter-source"></select></label>
      <label class="filter-field">Tag <select id="library-filter-tag"></select></label>
      <label class="filter-field">
        Sort
        <select id="library-sort">
          <option value="added">Date added</option>
          <option value="title">Title</option>
          <option value="composer">Composer</option>
        </select>
      </label>
      <button type="button" id="library-clear-filters">Clear filters</button>
    </div>
    <div id="library-count" class="piece-list-status"></div>
    <div id="library-save-row" class="bulk-bar" hidden></div>
    <div id="library-style-prompt"></div>
    <div id="library-style-result" class="style-result" role="status"></div>
    <div id="library-bulk" class="bulk-bar" hidden></div>
    <section class="panel library-panel">
      <div id="library-status" class="piece-list-status">Loading pieces...</div>
      <div id="library-table-container" hidden></div>
    </section>
  `;

  const onUploaded = async () => {
    await refreshLibrary();
    notifyMetadataChanged();
  };
  wireUploadControl("library-upload", onUploaded);
  wireUploadDrop("library-upload", onUploaded);
  wireLibraryFilterRow();

  await loadLibraryGroups();
  populateLibraryFilterSelects();
  await loadLibraryPieces();
}

/* Reloads the group counts, the filter selects and the piece list. */
async function refreshLibrary() {
  await loadLibraryGroups();
  populateLibraryFilterSelects();
  await loadLibraryPieces();
}

function isLibraryFilterOn() {
  const f = libraryFilter;
  return Boolean(f.composer || f.source || f.tag || f.unassigned.length || f.untagged || f.search.trim());
}

function fillFilterSelect(select, anyLabel, specialValue, specialLabel, special, groupList, current) {
  select.innerHTML = "";
  const add = (value, text) => {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = text;
    select.appendChild(option);
  };
  add("", "Any");
  add(specialValue, `${specialLabel} (${special})`);
  for (const group of groupList) {
    add(group.value, `${group.value} (${group.count})`);
  }
  select.value = current;
  if (select.value !== current) {
    select.value = "";
  }
}

function populateLibraryFilterSelects() {
  const f = libraryFilter;
  const g = libraryGroups;
  fillFilterSelect(
    document.getElementById("library-filter-composer"),
    "Any",
    UNASSIGNED,
    "Unassigned",
    g.unassigned.composer,
    g.composers,
    f.unassigned.includes("composer") ? UNASSIGNED : f.composer,
  );
  fillFilterSelect(
    document.getElementById("library-filter-source"),
    "Any",
    UNASSIGNED,
    "Unassigned",
    g.unassigned.source,
    g.sources,
    f.unassigned.includes("source") ? UNASSIGNED : f.source,
  );
  fillFilterSelect(
    document.getElementById("library-filter-tag"),
    "Any",
    UNTAGGED,
    "Untagged",
    g.untagged,
    g.tags,
    f.untagged ? UNTAGGED : f.tag,
  );
  document.getElementById("library-search").value = f.search;
  document.getElementById("library-sort").value = f.sort;
}

function wireLibraryFilterRow() {
  const setUnassigned = (which, on) => {
    libraryFilter.unassigned = libraryFilter.unassigned.filter((name) => name !== which);
    if (on) {
      libraryFilter.unassigned.push(which);
    }
  };
  document.getElementById("library-filter-composer").addEventListener("change", (event) => {
    const unassigned = event.target.value === UNASSIGNED;
    setUnassigned("composer", unassigned);
    libraryFilter.composer = unassigned ? "" : event.target.value;
    loadLibraryPieces();
  });
  document.getElementById("library-filter-source").addEventListener("change", (event) => {
    const unassigned = event.target.value === UNASSIGNED;
    setUnassigned("source", unassigned);
    libraryFilter.source = unassigned ? "" : event.target.value;
    loadLibraryPieces();
  });
  document.getElementById("library-filter-tag").addEventListener("change", (event) => {
    libraryFilter.untagged = event.target.value === UNTAGGED;
    libraryFilter.tag = libraryFilter.untagged ? "" : event.target.value;
    loadLibraryPieces();
  });
  document.getElementById("library-search").addEventListener("input", (event) => {
    libraryFilter.search = event.target.value;
    showLibraryPieces(libraryFetchedPieces);
  });
  document.getElementById("library-sort").addEventListener("change", (event) => {
    libraryFilter.sort = event.target.value;
    showLibraryPieces(libraryFetchedPieces);
  });
  document.getElementById("library-clear-filters").addEventListener("click", () => {
    libraryFilter = emptyLibraryFilter();
    populateLibraryFilterSelects();
    loadLibraryPieces();
  });
}

/* The last list fetched from the backend, before search and sort. */
let libraryFetchedPieces = [];

async function loadLibraryPieces() {
  const statusEl = document.getElementById("library-status");
  const tableEl = document.getElementById("library-table-container");

  if (tagsPanel) {
    tagsPanel.setActive(libraryFilter);
  }
  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");
  tableEl.hidden = true;
  tableEl.innerHTML = "";
  libraryTable = null;

  try {
    const query = PieceMetadata.filterQuery(libraryFilter);
    const res = await fetch(query ? `/api/analyzer/pieces?${query}` : "/api/analyzer/pieces");
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = `Failed to load pieces: ${data.message || "unknown error"}`;
      statusEl.classList.add("error");
      return;
    }

    libraryFetchedPieces = data.pieces;
    showLibraryPieces(data.pieces);
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

/* Applies the client-side search and sort to the fetched pieces and draws
 * the table and the count line. */
function showLibraryPieces(fetched) {
  const statusEl = document.getElementById("library-status");
  const tableEl = document.getElementById("library-table-container");
  const countEl = document.getElementById("library-count");

  libraryShownPieces = PieceMetadata.sortPieces(
    PieceMetadata.searchPieces(fetched, libraryFilter.search),
    libraryFilter.sort,
  );
  libraryTable = null;
  tableEl.innerHTML = "";
  updateLibraryBulkBar();
  updateLibrarySaveRow();
  countEl.textContent = isLibraryFilterOn()
    ? `${libraryShownPieces.length} of ${libraryGroups.total} pieces`
    : "";

  if (libraryShownPieces.length === 0) {
    statusEl.textContent = isLibraryFilterOn() ? "No pieces match." : "No pieces yet.";
    statusEl.classList.remove("error");
    tableEl.hidden = true;
    return;
  }

  statusEl.textContent = "";
  statusEl.classList.remove("error");
  tableEl.hidden = false;
  libraryTable = buildPieceTable(tableEl, {
    pieces: libraryShownPieces,
    columns: ["composer", "source", "tags"],
    selectable: true,
    rowActions: libraryRowActions,
    onOpen: (piece) => showAnalysisView(piece.id),
    onSelectionChange: updateLibraryBulkBar,
  });
}

/* "Save as collection" while a filter is on and pieces match. */
function updateLibrarySaveRow() {
  const row = document.getElementById("library-save-row");
  row.innerHTML = "";
  row.hidden = !(isLibraryFilterOn() && libraryShownPieces.length > 0);
  if (row.hidden) {
    return;
  }
  const button = document.createElement("button");
  button.type = "button";
  button.textContent = "Save as collection";
  button.addEventListener("click", () => saveLibraryAsCollection(button));
  row.appendChild(button);
  const styleButton = document.createElement("button");
  styleButton.type = "button";
  styleButton.textContent = "Build style";
  styleButton.addEventListener("click", () =>
    startStyleBuild({
      source: PieceMetadata.styleSourceForFilter(
        libraryFilter,
        libraryShownPieces.map((piece) => piece.id),
      ),
      host: document.getElementById("library-style-prompt"),
      trigger: styleButton,
      resultEl: document.getElementById("library-style-result"),
    }),
  );
  row.appendChild(styleButton);
}

/* Creates a collection named from the filter and adds every shown piece in
 * one POST, then opens it. The collection is a snapshot: it does not follow
 * later tag changes. */
async function saveLibraryAsCollection(button) {
  const statusEl = document.getElementById("library-status");
  const name = PieceMetadata.collectionNameFromFilter(libraryFilter);
  const ids = libraryShownPieces.map((piece) => piece.id);
  button.disabled = true;
  statusEl.classList.remove("error");
  const created = await postJsonResult("/api/corpus", { name });
  if (!created.ok || !created.data || !created.data.ok) {
    statusEl.textContent = collectionResultError(created, "Could not create the collection.");
    statusEl.classList.add("error");
    button.disabled = false;
    return;
  }
  const added = await postJsonResult(`/api/corpus/${created.data.id}/pieces`, PieceMetadata.addPiecesBody(ids));
  if (!added.ok || !added.data || !added.data.ok) {
    statusEl.textContent = `Created "${created.data.name}" but adding pieces failed: ${collectionResultError(added, "failed")}`;
    statusEl.classList.add("error");
    button.disabled = false;
    return;
  }
  setActiveTool("corpus");
  tagsPanel.setActive(null);
  showCorpusDetail(created.data.id, created.data.name);
}

/* The Library's current shared table (see buildPieceTable), or null. */
let libraryTable = null;

function selectedLibraryIds() {
  return libraryTable ? libraryTable.selectedIds() : [];
}

const BULK_ACTIONS = [
  { action: "set-composer", label: "Set composer", list: "bulk-composers" },
  { action: "set-source", label: "Set source", list: "bulk-sources" },
  { action: "add-tag", label: "Add tag", list: "bulk-tags" },
  { action: "remove-tag", label: "Remove tag", list: "bulk-tags" },
];

/* Shows the bulk bar when rows are checked: the count, one button per
 * bulk action (each opens an inline input with an Apply button) and Clear
 * selection. */
function updateLibraryBulkBar() {
  const bar = document.getElementById("library-bulk");
  const ids = selectedLibraryIds();
  bar.innerHTML = "";
  bar.hidden = ids.length === 0;
  if (ids.length === 0) {
    return;
  }

  const count = document.createElement("span");
  count.textContent = `${ids.length} selected`;
  const inline = document.createElement("span");
  inline.classList.add("bulk-inline");
  bar.append(
    count,
    buildDatalist("bulk-composers", libraryGroups.composers),
    buildDatalist("bulk-sources", libraryGroups.sources),
    buildDatalist("bulk-tags", libraryGroups.tags),
  );
  for (const item of BULK_ACTIONS) {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = item.label;
    button.addEventListener("click", () => openBulkInput(inline, item));
    bar.appendChild(button);
  }
  const clear = document.createElement("button");
  clear.type = "button";
  clear.textContent = "Clear selection";
  clear.addEventListener("click", () => {
    if (libraryTable) {
      libraryTable.clearSelection();
    }
    updateLibraryBulkBar();
  });
  bar.append(clear, inline);
}

function openBulkInput(container, item) {
  container.innerHTML = "";
  const input = document.createElement("input");
  input.type = "text";
  input.setAttribute("list", item.list);
  input.setAttribute("aria-label", item.label);
  const apply = document.createElement("button");
  apply.type = "button";
  apply.textContent = "Apply";
  apply.addEventListener("click", () => applyBulkEdit(item.action, input.value));
  container.append(input, apply);
  input.focus();
}

/* Sends one PATCH per selected piece, in order, and stops at the first
 * failure, naming the piece. Reloads the list and the groups afterwards. */
async function applyBulkEdit(action, rawValue) {
  const isTag = action === "add-tag" || action === "remove-tag";
  const value = isTag ? PieceMetadata.parseTagInput(rawValue) : rawValue.trim();
  const statusEl = document.getElementById("library-status");
  if (isTag && value.length === 0) {
    statusEl.textContent = "Enter a tag.";
    statusEl.classList.add("error");
    return;
  }
  const body = PieceMetadata.bulkPatch(action, value);
  const labels = new Map(libraryShownPieces.map((piece) => [piece.id, piece.filename || piece.id]));
  let failure = "";
  for (const id of selectedLibraryIds()) {
    const result = await fetchJsonResult(`/api/analyzer/pieces/${id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!result.ok || !result.data || !result.data.ok) {
      const reason = (result.data && (result.data.message || result.data.error)) || "failed";
      failure = `Stopped at ${labels.get(id) || id}: ${reason}`;
      break;
    }
  }
  await refreshLibrary();
  notifyMetadataChanged();
  if (failure) {
    const status = document.getElementById("library-status");
    status.textContent = failure;
    status.classList.add("error");
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

/* Text cell for composer or source; an empty value shows a muted dash. */
function buildMetadataTextCell(value) {
  const cell = document.createElement("td");
  if (value) {
    cell.textContent = value;
  } else {
    const dash = document.createElement("span");
    dash.classList.add("muted-dash");
    dash.textContent = "—";
    cell.appendChild(dash);
  }
  return cell;
}

function buildTagChips(tags) {
  const cell = document.createElement("td");
  cell.classList.add("tag-cell");
  for (const tag of tags || []) {
    const chip = document.createElement("span");
    chip.classList.add("tag-chip");
    chip.textContent = tag;
    cell.appendChild(chip);
  }
  return cell;
}

/* Row actions of the Library table. */
function libraryRowActions(row, actionsCell, rowPiece) {
  actionsCell.appendChild(buildActionButton({ icon: "▶", label: "Play", disabled: true }));
  actionsCell.appendChild(
    buildActionButton({ icon: "✎", label: "Edit", onClick: () => startLibraryEdit(row, rowPiece) }),
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
}

const PIECE_TABLE_COLUMN_LABELS = { composer: "Composer", source: "Source", tags: "Tags" };

/* The shared piece table used by the Library, a collection's detail and the
 * add-pieces picker. options: pieces; columns (any of "composer", "source",
 * "tags" after the always-present Title); selectable (a checkbox column and
 * a select-all box); rowActions(row, actionsCell, piece); onOpen(piece) for a
 * title click; onSelectionChange(). Draws into container and returns
 * {table, columnCount, buildRow(piece), selectedIds(), selectAll(checked),
 * clearSelection()}. */
function buildPieceTable(container, options) {
  const columns = options.columns || [];
  const table = document.createElement("table");
  table.classList.add("library-table");
  const headRow = document.createElement("tr");
  const notify = () => {
    if (options.onSelectionChange) {
      options.onSelectionChange();
    }
  };

  let selectAllBox = null;
  if (options.selectable) {
    const th = document.createElement("th");
    th.classList.add("select-cell");
    selectAllBox = document.createElement("input");
    selectAllBox.type = "checkbox";
    selectAllBox.classList.add("select-all");
    selectAllBox.setAttribute("aria-label", "Select all shown pieces");
    th.appendChild(selectAllBox);
    headRow.appendChild(th);
  }
  const labels = ["Title", ...columns.map((name) => PIECE_TABLE_COLUMN_LABELS[name]), "Actions"];
  for (const text of labels) {
    const th = document.createElement("th");
    th.textContent = text;
    headRow.appendChild(th);
  }
  const thead = document.createElement("thead");
  thead.appendChild(headRow);
  const tbody = document.createElement("tbody");
  table.append(thead, tbody);

  function buildRow(piece) {
    const row = buildPieceRow(piece, options.rowActions || (() => {}), options.onOpen);
    if (options.selectable) {
      const selectCell = document.createElement("td");
      selectCell.classList.add("select-cell");
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.classList.add("row-select");
      checkbox.dataset.id = piece.id;
      checkbox.setAttribute("aria-label", `Select ${piece.filename || piece.id}`);
      selectCell.appendChild(checkbox);
      row.insertBefore(selectCell, row.firstChild);
    }
    const extra = columns.map((name) => (name === "tags" ? buildTagChips(piece.tags) : buildMetadataTextCell(piece[name])));
    row.children[options.selectable ? 1 : 0].after(...extra);
    return row;
  }

  for (const piece of options.pieces) {
    tbody.appendChild(buildRow(piece));
  }
  container.innerHTML = "";
  container.appendChild(table);

  const boxes = () => tbody.querySelectorAll(".row-select");
  if (selectAllBox) {
    selectAllBox.addEventListener("change", () => {
      for (const box of boxes()) {
        box.checked = selectAllBox.checked;
      }
      notify();
    });
    tbody.addEventListener("change", (event) => {
      if (event.target.classList.contains("row-select")) {
        selectAllBox.checked = Array.from(boxes()).every((box) => box.checked);
        notify();
      }
    });
  }

  return {
    table,
    columnCount: labels.length + (options.selectable ? 1 : 0),
    buildRow,
    selectedIds() {
      return Array.from(tbody.querySelectorAll(".row-select:checked"), (box) => box.dataset.id);
    },
    selectAll(checked) {
      for (const box of boxes()) {
        box.checked = checked;
      }
      if (selectAllBox) {
        selectAllBox.checked = checked;
      }
      notify();
    },
    clearSelection() {
      for (const box of boxes()) {
        box.checked = false;
      }
      if (selectAllBox) {
        selectAllBox.checked = false;
      }
    },
  };
}

/* Distinct composers, sources and tags with counts (GET
 * /api/analyzer/groups), cached for the Library's datalists and selects.
 * Resolves to the cache; a failed fetch keeps the previous one. */
let libraryGroups = { composers: [], sources: [], tags: [], unassigned: { composer: 0, source: 0 }, untagged: 0, total: 0 };

async function loadLibraryGroups() {
  const result = await fetchJsonResult("/api/analyzer/groups");
  if (result.ok && result.data && result.data.ok) {
    libraryGroups = result.data;
  }
  return libraryGroups;
}

/* Called after any upload, edit, bulk edit or delete in the Library;
 * refreshes the sidebar Tags panel. */
function notifyMetadataChanged() {
  if (tagsPanel) {
    tagsPanel.refresh();
  }
}

function buildDatalist(id, groupList) {
  const list = document.createElement("datalist");
  list.id = id;
  for (const group of groupList) {
    const option = document.createElement("option");
    option.value = group.value;
    list.appendChild(option);
  }
  return list;
}

/* A text box that turns typed text into removable chips on Enter or comma;
 * Backspace on an empty box removes the last chip. getTags() also takes
 * whatever is still typed. */
function buildTagInput(initialTags) {
  const el = document.createElement("div");
  el.classList.add("tag-input");
  const tags = [];
  const input = document.createElement("input");
  input.type = "text";
  input.placeholder = "Add tag";
  input.setAttribute("aria-label", "Tags");

  function render() {
    for (const chip of el.querySelectorAll(".tag-chip")) {
      chip.remove();
    }
    for (const tag of tags) {
      const chip = document.createElement("span");
      chip.classList.add("tag-chip");
      chip.textContent = tag;
      const remove = document.createElement("button");
      remove.type = "button";
      remove.classList.add("chip-remove");
      remove.textContent = "×";
      remove.setAttribute("aria-label", `Remove tag ${tag}`);
      remove.addEventListener("click", () => {
        tags.splice(tags.indexOf(tag), 1);
        render();
      });
      chip.appendChild(remove);
      el.insertBefore(chip, input);
    }
  }

  function commit() {
    for (const tag of PieceMetadata.parseTagInput(input.value)) {
      if (!tags.includes(tag)) {
        tags.push(tag);
      }
    }
    input.value = "";
    render();
  }

  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter" || event.key === ",") {
      event.preventDefault();
      commit();
    } else if (event.key === "Backspace" && input.value === "") {
      tags.pop();
      render();
    }
  });
  input.addEventListener("input", () => {
    if (input.value.includes(",")) {
      commit();
    }
  });

  el.appendChild(input);
  tags.push(...(initialTags || []));
  render();
  return {
    el,
    inputEl: input,
    getTags() {
      commit();
      return tags.slice();
    },
  };
}

function buildEditorField(labelText, inputEl) {
  const label = document.createElement("label");
  label.classList.add("editor-field");
  label.append(labelText, inputEl);
  return label;
}

/* Replaces a Library row with an editor row for the piece's title,
 * composer, source and tags. Save sends one PATCH; Cancel or Escape puts
 * the row back. */
async function startLibraryEdit(row, piece) {
  await loadLibraryGroups();
  const editorRow = document.createElement("tr");
  editorRow.classList.add("editor-row");
  const cell = document.createElement("td");
  cell.colSpan = libraryTable ? libraryTable.columnCount : 6;
  editorRow.appendChild(cell);

  const title = document.createElement("input");
  title.type = "text";
  title.value = piece.filename || "";
  const composer = document.createElement("input");
  composer.type = "text";
  composer.value = piece.composer || "";
  composer.setAttribute("list", "editor-composers");
  const source = document.createElement("input");
  source.type = "text";
  source.value = piece.source || "";
  source.setAttribute("list", "editor-sources");
  const tagInput = buildTagInput(piece.tags);
  const subjectsField = buildSubjectsField("pieces", piece.id, null);
  fetchJsonResult(`/api/analyzer/pieces/${piece.id}?document=summary`).then((detail) => {
    subjectsField.setSubjects(detail.ok && detail.data && Array.isArray(detail.data.subjects) ? detail.data.subjects : []);
  });

  const errorEl = document.createElement("div");
  errorEl.classList.add("piece-list-status", "error");
  const saveButton = document.createElement("button");
  saveButton.type = "button";
  saveButton.textContent = "Save";
  const cancelButton = document.createElement("button");
  cancelButton.type = "button";
  cancelButton.textContent = "Cancel";

  const fields = document.createElement("div");
  fields.classList.add("editor-fields");
  fields.append(
    buildEditorField("Title", title),
    buildEditorField("Composer", composer),
    buildEditorField("Source", source),
    buildEditorField("Tags", tagInput.el),
    buildEditorField("Subjects", subjectsField.el),
  );
  const buttons = document.createElement("div");
  buttons.classList.add("editor-buttons");
  buttons.append(saveButton, cancelButton);
  cell.append(
    buildDatalist("editor-composers", libraryGroups.composers),
    buildDatalist("editor-sources", libraryGroups.sources),
    fields,
    errorEl,
    buttons,
  );

  const cancel = () => editorRow.replaceWith(row);
  cancelButton.addEventListener("click", cancel);
  editorRow.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      cancel();
    }
  });
  saveButton.addEventListener("click", async () => {
    errorEl.textContent = "";
    const body = {
      filename: title.value.trim(),
      composer: composer.value,
      source: source.value,
      tags: tagInput.getTags(),
    };
    const result = await fetchJsonResult(`/api/analyzer/pieces/${piece.id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!result.ok || !result.data || !result.data.ok) {
      errorEl.textContent = (result.data && (result.data.message || result.data.error)) || "Save failed.";
      return;
    }
    const fresh = libraryTable ? libraryTable.buildRow(result.data.piece) : row;
    editorRow.replaceWith(fresh);
    await loadLibraryGroups();
    populateLibraryFilterSelects();
    notifyMetadataChanged();
  });

  row.replaceWith(editorRow);
  title.focus();
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
    await loadLibraryGroups();
    populateLibraryFilterSelects();
    notifyMetadataChanged();
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

/* options.motiveFamily opens the Data mode Motives tab and shows that family
 * on the score. */
function showAnalysisView(id, options) {
  lastAnalysisPieceId = id;
  setActiveTool("analysis");
  leaveWorkspace();

  const content = getContentEl();
  content.classList.add("content--workspace");
  content.innerHTML = `<div id="analysis-status" class="piece-list-status ws-loading">Loading piece ${id}...</div>`;

  loadAnalysisPiece(id)
    .then((data) => {
      if (lastAnalysisPieceId !== id || content.querySelector(".ws")) {
        return;
      }
      if (!data.ok) {
        showAnalysisProblem(data.message === "piece not found" ? "Piece not found" : data.message || "Piece not found");
        return;
      }
      mountAnalysisWorkspace(content, id, data, options);
    })
    .catch((err) => {
      showAnalysisProblem(`Could not reach backend: ${err}`);
    });
}

/* The sections of a piece's analysis document by name, from
 * GET /api/analyzer/pieces/<id>/document. Resolves to the {name: section}
 * object, or null when the request fails. */
async function fetchDocumentSections(id, names) {
  const result = await fetchJsonResult(
    `/api/analyzer/pieces/${encodeURIComponent(id)}/document?sections=${encodeURIComponent(names.join(","))}`,
  );
  return result.ok && result.data && result.data.ok && result.data.sections ? result.data.sections : null;
}

/* The piece detail with the document summary (metadata and meter), then the sections the score draws its layers
 * from. The other sections load when their tab opens (renderAnalysisTabs). The detail's `document` is the cache's
 * sections so far; `documentCache` is the cache. */
async function loadAnalysisPiece(id) {
  const res = await fetch(`/api/analyzer/pieces/${encodeURIComponent(id)}?document=summary`);
  const data = await res.json();
  if (!data.ok) {
    return data;
  }
  const cache = createDocumentCache(data.document, data.document_sections);
  const scoreSections = ScoreLayers.scoreSectionNames(cache.names);
  const loaded = await loadDocumentSections(cache, scoreSections, (names) => fetchDocumentSections(id, names));
  if (!loaded) {
    return { ok: false, message: "Could not load the analysis document" };
  }
  data.document = cache.document();
  data.documentCache = cache;
  return data;
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
function mountAnalysisWorkspace(content, id, data, options) {
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
    headerExtra: buildNotationRow(data),
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

  const family = options && Number.isInteger(options.motiveFamily) ? options.motiveFamily : null;
  renderDataMode(workspace.dataEl(), data, family === null ? null : "motives");
  const motives = data.document && Array.isArray(data.document.motives) ? data.document.motives : [];
  if (family !== null && motives[family]) {
    workspace.selectClaim(motives[family]);
  }
}

/* The Notation row (R-110): the reading the analysis used, the meter
 * check's line, Apply for a better reading, and Edit for the override
 * fields. Saving sends the PATCH and loads the analysis and score again. */
function buildNotationRow(data) {
  const summary = NotationPanel.meterFitSummary(data.document);
  const current = data.notation || null;
  const row = document.createElement("div");
  row.classList.add("notation-row");

  const label = document.createElement("span");
  label.classList.add("notation-label");
  label.textContent = "Notation";
  const reading = document.createElement("span");
  reading.classList.add("notation-reading");
  reading.textContent = summary ? summary.reading : "";
  const line = document.createElement("span");
  line.classList.add("notation-line");
  line.textContent = summary ? summary.line : "";
  const message = document.createElement("span");
  message.classList.add("notation-message");
  message.setAttribute("role", "status");

  async function save(body) {
    message.textContent = "Saving...";
    const result = await fetchJsonResult(`/api/analyzer/pieces/${data.id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!result.ok || !result.data || !result.data.ok) {
      message.textContent = (result.data && (result.data.message || result.data.error)) || "Save failed.";
      return;
    }
    showAnalysisView(data.id);
  }

  function button(text, onClick) {
    const el = document.createElement("button");
    el.type = "button";
    el.classList.add("ws-btn");
    el.textContent = text;
    el.addEventListener("click", onClick);
    return el;
  }

  row.append(label, reading, line);

  if (summary && summary.alternative) {
    const alt = summary.alternative;
    row.appendChild(
      button("Apply", () =>
        save(NotationPanel.notationBody({ meter: alt.meter, value_scale: alt.value_scale, pickup: alt.pickup })),
      ),
    );
  }

  const form = document.createElement("div");
  form.classList.add("notation-form");
  form.hidden = true;
  const meterInput = document.createElement("input");
  meterInput.type = "text";
  meterInput.placeholder = "Meter, 3/4";
  meterInput.size = 6;
  meterInput.setAttribute("aria-label", "Meter");
  meterInput.value = current ? current.meter : "";
  const scaleSelect = document.createElement("select");
  scaleSelect.setAttribute("aria-label", "Value scale");
  for (const [value, text] of [["", "Scale: as written"], ["1/4", "x1/4"], ["1/2", "x1/2 (halved)"], ["1", "x1"], ["2", "x2 (doubled)"], ["4", "x4"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = text;
    scaleSelect.appendChild(option);
  }
  scaleSelect.value = current ? current.value_scale : "";
  const pickupInput = document.createElement("input");
  pickupInput.type = "text";
  pickupInput.placeholder = "Pickup, 1/4";
  pickupInput.size = 8;
  pickupInput.setAttribute("aria-label", "Pickup");
  pickupInput.value = current ? current.pickup : "";
  form.append(
    meterInput,
    scaleSelect,
    pickupInput,
    button("Save", () =>
      save(NotationPanel.notationBody({ meter: meterInput.value, value_scale: scaleSelect.value, pickup: pickupInput.value })),
    ),
    button("Clear", () => save(NotationPanel.notationBody({ clear: true }))),
    button("Cancel", () => {
      form.hidden = true;
      message.textContent = "";
    }),
  );

  row.appendChild(button("Edit", () => {
    form.hidden = !form.hidden;
  }));
  row.append(message, form);
  return row;
}

/* Data mode: the Provenance list, then the claim tabs. */
function renderDataMode(dataEl, data, tabId) {
  dataEl.innerHTML = `
    <div class="sidebar-label">Provenance</div>
    <dl id="analysis-provenance" class="provenance-list"></dl>
    <div id="analysis-tabs" class="analysis-tabs"></div>
    <div id="analysis-claims" class="analysis-claims"></div>
  `;
  renderProvenancePanel(data.document);
  renderAnalysisTabs(data, tabId);
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

function renderAnalysisTabs(data, initialTabId) {
  const tabsEl = document.getElementById("analysis-tabs");
  const claimsEl = document.getElementById("analysis-claims");
  const cache = data.documentCache || createDocumentCache(data.document);
  const fetchSections = (names) => fetchDocumentSections(data.id, names);
  let selectedTabId = null;

  function labelButton(button, state) {
    if (!state) {
      button.textContent = button.dataset.label;
    } else if (state.empty) {
      button.textContent = `${button.dataset.label} (none)`;
    } else {
      button.textContent = `${button.dataset.label} ${state.rowCount}`;
    }
  }

  async function selectTab(tab) {
    selectedTabId = tab.id;
    for (const button of tabsEl.querySelectorAll("button")) {
      button.classList.toggle("active", button.dataset.tabId === tab.id);
    }
    claimsEl.textContent = cache.missing(tab.sections).length > 0 ? "Loading..." : "";
    const state = await loadTabState(cache, tab, fetchSections);
    if (selectedTabId !== tab.id) {
      return;
    }
    if (!state) {
      claimsEl.textContent = "Could not load this tab.";
      return;
    }
    labelButton(tabsEl.querySelector(`button[data-tab-id="${tab.id}"]`), state);
    claimsEl.textContent = "";
    if (state.empty) {
      claimsEl.textContent = "This analysis has no claims in this tab.";
    } else {
      renderTabBlocks(claimsEl, state, data.note_xml_ids, data.id);
    }
    if (tab.id === "rhythm-meter") {
      claimsEl.prepend(buildNotationRow(data));
    }
  }

  for (const tab of ANALYSIS_TABS) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.tabId = tab.id;
    button.dataset.label = tab.label;
    labelButton(button, cache.missing(tab.sections).length === 0 ? buildTabState(tab, cache.document()) : null);
    button.addEventListener("click", () => selectTab(tab));
    tabsEl.appendChild(button);
  }

  const initial = ANALYSIS_TABS.find((tab) => tab.id === initialTabId) || ANALYSIS_TABS[0];
  selectTab(initial);
}

/* A Data mode claim row was chosen: the workspace shows it on the score. */
function showClaimOnScore(claim) {
  if (analysisWorkspace) {
    analysisWorkspace.selectClaim(claim);
  }
}

/* One tab: for each document section, a heading, the section's JSON block
 * when it is an object, and its rows. A row is a claim: label, where it
 * is, and a toggle for its remaining fields. Choosing a row shows its
 * claim on the score (showClaimOnScore). */
function renderTabBlocks(claimsEl, state, noteXmlIds, pieceId) {
  claimsEl.innerHTML = "";
  let pieceLinks = null;
  if (state.tab.id === "motives" && pieceId) {
    pieceLinks = fetchJsonResult(`/api/links/pieces/${pieceId}`).then((answer) =>
      answer.ok && answer.data && Array.isArray(answer.data.links) ? answer.data.links : [],
    );
  }

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
      claimsEl.appendChild(buildClaimRows(block.rows, noteXmlIds, pieceId, pieceLinks));
    }
  }
}

/* A motive family's features and its Linked list (links from the newest run
 * of each scope; pieceLinks resolves to GET /api/links/pieces/<id> entries). */
function buildMotiveExtras(row, pieceId, pieceLinks) {
  const extras = document.createElement("div");
  extras.classList.add("claim-row-extras");
  for (const [name, value] of row.features) {
    const line = document.createElement("div");
    line.textContent = `${name}: ${value}`;
    extras.appendChild(line);
  }
  const linked = document.createElement("div");
  linked.textContent = "Linked: loading...";
  extras.appendChild(linked);
  pieceLinks.then((entries) => {
    const links = MotiveLinksModel.pieceLinkRows(entries, pieceId, row.family);
    linked.textContent = "";
    if (links.length === 0) {
      linked.textContent = "No links yet. Run Motive links over a scope with this piece.";
      return;
    }
    linked.appendChild(document.createTextNode("Linked:"));
    const list = document.createElement("ul");
    for (const link of links) {
      const li = document.createElement("li");
      const open = document.createElement("a");
      open.classList.add("clickable");
      open.textContent = link.title;
      open.addEventListener("click", (event) => {
        event.stopPropagation();
        showAnalysisView(link.piece, { motiveFamily: link.family });
      });
      li.append(open, document.createTextNode(` ${link.measures}, ${link.steps} (${link.distance})`));
      list.appendChild(li);
    }
    linked.appendChild(list);
  });
  return extras;
}

function buildClaimRows(rows, noteXmlIds, pieceId, pieceLinks) {
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
    if (pieceLinks && row.family !== null) {
      li.appendChild(buildMotiveExtras(row, pieceId, pieceLinks));
    }

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
      showClaimOnScore(row.claim);
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
        <input type="text" id="corpus-description-input" placeholder="Description (optional)" />
        <button id="corpus-create-button" type="button">Create</button>
      </div>
    </div>
    <div id="corpus-create-status" class="upload-status"></div>
    <div id="corpus-list-status" class="piece-list-status">Loading collections...</div>
    <div id="corpus-grid" class="collections-grid"></div>
    <h2 class="section-title">Styles</h2>
    <div id="styles-status" class="piece-list-status" role="status"></div>
    <section id="styles-section" class="panel library-panel" hidden></section>
  `;

  document.getElementById("corpus-create-button").addEventListener("click", handleCreateCorpus);
  document.getElementById("corpus-name-input").addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      handleCreateCorpus();
    }
  });

  const statusEl = document.getElementById("corpus-list-status");
  const gridEl = document.getElementById("corpus-grid");

  loadStylesSection();

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
    const summaries = await loadCollectionSummaries(data.corpora);
    for (const corpus of data.corpora) {
      gridEl.appendChild(buildCollectionCard(corpus, summaries.get(corpus.id)));
    }
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

/* Resolves to Map(collection id -> collectionSummary) by joining each
 * collection's piece ids against the Library list. A failed lookup leaves
 * that card without a summary. */
async function loadCollectionSummaries(collections) {
  const summaries = new Map();
  const library = await fetchJsonResult("/api/analyzer/pieces");
  if (!(library.ok && library.data && library.data.ok)) {
    return summaries;
  }
  const byId = new Map(library.data.pieces.map((piece) => [piece.id, piece]));
  await Promise.all(
    collections.map(async (corpus) => {
      const detail = await fetchJsonResult(`/api/corpus/${corpus.id}`);
      if (detail.ok && detail.data && detail.data.ok) {
        const members = detail.data.pieces.map((id) => byId.get(id)).filter(Boolean);
        summaries.set(corpus.id, PieceMetadata.collectionSummary(members));
      }
    }),
  );
  return summaries;
}

function patchCollection(id, body) {
  return fetchJsonResult(`/api/corpus/${id}`, {
    method: "PATCH",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
}

function collectionResultError(result, fallback) {
  return (result.data && (result.data.message || result.data.error)) || fallback;
}

function showCollectionsStatus(text, isError) {
  const statusEl = document.getElementById("corpus-create-status");
  if (statusEl) {
    statusEl.textContent = text;
    statusEl.classList.toggle("error", Boolean(isError));
  }
}

/* The card's ⋯ button and its menu: Rename, Edit description, Delete. Reachable
 * by keyboard (a button with aria-expanded; Escape closes). */
function buildCollectionCardMenu(corpus) {
  const wrap = document.createElement("div");
  wrap.classList.add("card-menu");
  wrap.addEventListener("click", (event) => event.stopPropagation());
  const toggle = document.createElement("button");
  toggle.type = "button";
  toggle.classList.add("icon-button");
  toggle.textContent = "⋯";
  toggle.setAttribute("aria-label", `Actions for ${corpus.name}`);
  toggle.setAttribute("aria-haspopup", "true");
  toggle.setAttribute("aria-expanded", "false");
  const menu = document.createElement("div");
  menu.classList.add("card-menu-list");
  menu.hidden = true;

  const close = () => {
    menu.hidden = true;
    toggle.setAttribute("aria-expanded", "false");
  };
  const addItem = (label, action) => {
    const item = document.createElement("button");
    item.type = "button";
    item.textContent = label;
    item.addEventListener("click", async () => {
      close();
      await action();
    });
    menu.appendChild(item);
  };
  addItem("Rename", async () => {
    const name = window.prompt("Collection name", corpus.name);
    if (name === null) {
      return;
    }
    const result = await patchCollection(corpus.id, { name });
    if (result.ok && result.data && result.data.ok) {
      renderCorpusView();
    } else {
      showCollectionsStatus(collectionResultError(result, "Rename failed."), true);
    }
  });
  addItem("Edit description", async () => {
    const description = window.prompt("Description", corpus.description || "");
    if (description === null) {
      return;
    }
    const result = await patchCollection(corpus.id, { description });
    if (result.ok && result.data && result.data.ok) {
      renderCorpusView();
    } else {
      showCollectionsStatus(collectionResultError(result, "Update failed."), true);
    }
  });
  addItem("Delete", async () => {
    if (!window.confirm(`Delete the collection "${corpus.name}"? Its pieces are not deleted.`)) {
      return;
    }
    const result = await fetchJsonResult(`/api/corpus/${corpus.id}`, { method: "DELETE" });
    if (result.ok && result.data && result.data.ok) {
      renderCorpusView();
    } else {
      showCollectionsStatus(collectionResultError(result, "Delete failed."), true);
    }
  });

  toggle.addEventListener("click", () => {
    menu.hidden = !menu.hidden;
    toggle.setAttribute("aria-expanded", String(!menu.hidden));
  });
  wrap.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      close();
      toggle.focus();
    }
  });
  wrap.append(toggle, menu);
  return wrap;
}

function buildCollectionCard(corpus, summary) {
  const card = document.createElement("div");
  card.classList.add("collection-card", "clickable");
  card.addEventListener("click", () => showCorpusDetail(corpus.id, corpus.name));

  const cover = document.createElement("div");
  cover.classList.add("collection-card-cover");
  card.appendChild(cover);

  const addLine = (className, text) => {
    if (!text) {
      return;
    }
    const line = document.createElement("div");
    line.classList.add(className);
    line.textContent = text;
    card.appendChild(line);
  };
  card.appendChild(buildCollectionCardMenu(corpus));
  addLine("collection-card-name", corpus.name);
  addLine("collection-card-description", corpus.description);
  addLine("collection-card-count", PieceMetadata.pieceCountText(corpus.piece_count || 0));
  if (summary) {
    addLine("collection-card-summary", PieceMetadata.summaryLine("Composers", summary.composers));
    addLine("collection-card-summary", PieceMetadata.summaryLine("Tags", summary.tags));
  }

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
      body: JSON.stringify({ name, description: document.getElementById("corpus-description-input").value }),
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

/* State of the open collection detail: its members joined with Library
 * metadata, the filter on them and the shared table. */
let collectionView = null;

function showCorpusDetail(id, name) {
  collectionView = { id, name, members: [], libraryPieces: [], filter: emptyLibraryFilter(), table: null };
  const content = getContentEl();
  content.innerHTML = `
    <div id="corpus-back-container"></div>
    <div id="corpus-heading" class="collection-heading"></div>
    <div class="upload-controls">
      <button id="corpus-build-style-button" type="button">Build style</button>
    </div>
    <div id="corpus-style-prompt"></div>
    <div id="corpus-style-result" class="style-result" role="status"></div>
    <div class="filter-row">
      <input type="search" id="corpus-search" placeholder="Search" aria-label="Search" />
      <label class="filter-field">Composer <select id="corpus-filter-composer"></select></label>
      <label class="filter-field">Source <select id="corpus-filter-source"></select></label>
      <label class="filter-field">Tag <select id="corpus-filter-tag"></select></label>
      <button type="button" id="corpus-clear-filters">Clear filters</button>
    </div>
    <div id="corpus-count" class="piece-list-status"></div>
    <div id="corpus-bulk" class="bulk-bar" hidden></div>
    <section class="panel library-panel">
      <div id="corpus-piece-status" class="piece-list-status">Loading pieces...</div>
      <div id="corpus-piece-table-container" hidden></div>
    </section>
    <div class="upload-controls">
      <button id="corpus-add-pieces-button" type="button">Add pieces</button>
    </div>
    <div id="corpus-piece-add-status" class="upload-status"></div>
    <section id="corpus-picker" class="panel" hidden></section>
  `;

  document.getElementById("corpus-back-container").appendChild(renderCorpusBackLink());
  loadCollectionHeading(id, name);
  document.getElementById("corpus-add-pieces-button").addEventListener("click", openCollectionPicker);
  const buildStyleButton = document.getElementById("corpus-build-style-button");
  buildStyleButton.addEventListener("click", () => {
    const title = document.querySelector("#corpus-heading h1");
    startStyleBuild({
      source: { kind: "collection", collection_id: id, name: title ? title.textContent : name },
      host: document.getElementById("corpus-style-prompt"),
      trigger: buildStyleButton,
      resultEl: document.getElementById("corpus-style-result"),
    });
  });
  wireCollectionFilterRow();

  loadCorpusPieces(id, name);
}

function wireCollectionFilterRow() {
  const f = () => collectionView.filter;
  const setUnassigned = (which, on) => {
    f().unassigned = f().unassigned.filter((item) => item !== which);
    if (on) {
      f().unassigned.push(which);
    }
  };
  const change = (id, apply) => {
    document.getElementById(id).addEventListener("change", (event) => {
      apply(event.target.value);
      showCollectionMembers();
    });
  };
  change("corpus-filter-composer", (value) => {
    setUnassigned("composer", value === UNASSIGNED);
    f().composer = value === UNASSIGNED ? "" : value;
  });
  change("corpus-filter-source", (value) => {
    setUnassigned("source", value === UNASSIGNED);
    f().source = value === UNASSIGNED ? "" : value;
  });
  change("corpus-filter-tag", (value) => {
    f().untagged = value === UNTAGGED;
    f().tag = f().untagged ? "" : value;
  });
  document.getElementById("corpus-search").addEventListener("input", (event) => {
    f().search = event.target.value;
    showCollectionMembers(false);
  });
  document.getElementById("corpus-clear-filters").addEventListener("click", () => {
    collectionView.filter = emptyLibraryFilter();
    showCollectionMembers();
  });
}

function isCollectionFilterOn() {
  const f = collectionView.filter;
  return Boolean(f.composer || f.source || f.tag || f.unassigned.length || f.untagged || f.search.trim());
}

/* Fills the filter selects from the members and draws the filtered table.
 * The selects are left alone while typing in the search box. */
function showCollectionMembers(refillSelects = true) {
  const view = collectionView;
  const f = view.filter;
  const groups = PieceMetadata.groupValues(view.members);
  if (refillSelects) {
    fillFilterSelect(
      document.getElementById("corpus-filter-composer"),
      "Any",
      UNASSIGNED,
      "Unassigned",
      groups.unassigned.composer,
      groups.composers,
      f.unassigned.includes("composer") ? UNASSIGNED : f.composer,
    );
    fillFilterSelect(
      document.getElementById("corpus-filter-source"),
      "Any",
      UNASSIGNED,
      "Unassigned",
      groups.unassigned.source,
      groups.sources,
      f.unassigned.includes("source") ? UNASSIGNED : f.source,
    );
    fillFilterSelect(
      document.getElementById("corpus-filter-tag"),
      "Any",
      UNTAGGED,
      "Untagged",
      groups.untagged,
      groups.tags,
      f.untagged ? UNTAGGED : f.tag,
    );
    document.getElementById("corpus-search").value = f.search;
  }

  const shown = PieceMetadata.sortPieces(PieceMetadata.matchPieces(view.members, f), "title");
  const statusEl = document.getElementById("corpus-piece-status");
  const tableEl = document.getElementById("corpus-piece-table-container");
  document.getElementById("corpus-count").textContent = isCollectionFilterOn()
    ? `${shown.length} of ${view.members.length} pieces`
    : "";
  view.table = null;
  tableEl.innerHTML = "";
  updateCollectionBulkBar();
  statusEl.classList.remove("error");
  if (shown.length === 0) {
    statusEl.textContent = view.members.length === 0 ? "No pieces yet." : "No pieces match.";
    tableEl.hidden = true;
    return;
  }
  statusEl.textContent = "";
  tableEl.hidden = false;
  view.table = buildPieceTable(tableEl, {
    pieces: shown,
    columns: ["composer", "source", "tags"],
    selectable: true,
    rowActions: (row, actionsCell, rowPiece) => {
      actionsCell.appendChild(
        buildActionButton({ icon: "↗", label: "Open analysis", onClick: () => showAnalysisView(rowPiece.id) }),
      );
      actionsCell.appendChild(
        buildActionButton({
          icon: "🗑",
          label: "Remove from collection",
          onClick: () => handleRemovePiece(view.id, view.name, rowPiece.id),
        }),
      );
    },
    onOpen: (piece) => showAnalysisView(piece.id),
    onSelectionChange: updateCollectionBulkBar,
  });
}

function updateCollectionBulkBar() {
  const bar = document.getElementById("corpus-bulk");
  const ids = collectionView && collectionView.table ? collectionView.table.selectedIds() : [];
  bar.innerHTML = "";
  bar.hidden = ids.length === 0;
  if (ids.length === 0) {
    return;
  }
  const count = document.createElement("span");
  count.textContent = `${ids.length} selected`;
  const remove = document.createElement("button");
  remove.type = "button";
  remove.textContent = "Remove from collection";
  remove.addEventListener("click", () => bulkRemoveFromCollection(ids));
  const clear = document.createElement("button");
  clear.type = "button";
  clear.textContent = "Clear selection";
  clear.addEventListener("click", () => {
    collectionView.table.clearSelection();
    updateCollectionBulkBar();
  });
  bar.append(count, remove, clear);
}

/* One DELETE per selected piece, in order; stops at the first failure and
 * names the piece. Reloads the members afterwards. */
async function bulkRemoveFromCollection(ids) {
  const view = collectionView;
  const labels = new Map(view.members.map((piece) => [piece.id, piece.filename || piece.id]));
  let failure = "";
  for (const pieceId of ids) {
    const result = await fetchJsonResult(`/api/corpus/${view.id}/pieces/${pieceId}`, { method: "DELETE" });
    if (!result.ok || !result.data || !result.data.ok) {
      failure = `Stopped at ${labels.get(pieceId) || pieceId}: ${collectionResultError(result, "failed")}`;
      break;
    }
  }
  await loadCorpusPieces(view.id, view.name);
  if (failure) {
    const statusEl = document.getElementById("corpus-piece-status");
    statusEl.textContent = failure;
    statusEl.classList.add("error");
  }
}

/* The detail title with inline rename and the description with an editor.
 * Shows the name passed in while the collection is fetched. */
async function loadCollectionHeading(id, fallbackName) {
  const heading = document.getElementById("corpus-heading");
  const draw = (name, description) => {
    heading.innerHTML = "";
    const title = document.createElement("h1");
    title.classList.add("view-title");
    title.textContent = name;
    const renameButton = document.createElement("button");
    renameButton.type = "button";
    renameButton.textContent = "Rename";
    const text = document.createElement("p");
    text.classList.add("collection-description");
    text.textContent = description || "No description.";
    const editButton = document.createElement("button");
    editButton.type = "button";
    editButton.textContent = "Edit description";
    const errorEl = document.createElement("div");
    errorEl.classList.add("piece-list-status", "error");

    const edit = (field, current, label) => {
      const input = document.createElement("input");
      input.type = "text";
      input.value = current;
      input.setAttribute("aria-label", label);
      const save = document.createElement("button");
      save.type = "button";
      save.textContent = "Save";
      const cancel = document.createElement("button");
      cancel.type = "button";
      cancel.textContent = "Cancel";
      const row = document.createElement("div");
      row.classList.add("inline-edit");
      row.append(input, save, cancel);
      heading.replaceChildren(row, errorEl);
      input.focus();
      cancel.addEventListener("click", () => draw(name, description));
      row.addEventListener("keydown", (event) => {
        if (event.key === "Escape") {
          draw(name, description);
        } else if (event.key === "Enter") {
          save.click();
        }
      });
      save.addEventListener("click", async () => {
        const result = await patchCollection(id, { [field]: input.value });
        if (result.ok && result.data && result.data.ok) {
          draw(result.data.name, result.data.description);
        } else {
          errorEl.textContent = collectionResultError(result, "Save failed.");
        }
      });
    };
    renameButton.addEventListener("click", () => edit("name", name, "Collection name"));
    editButton.addEventListener("click", () => edit("description", description || "", "Description"));
    heading.append(title, renameButton, text, editButton, errorEl);
  };
  draw(fallbackName, "");
  const result = await fetchJsonResult(`/api/corpus/${id}`);
  if (result.ok && result.data && result.data.ok && document.getElementById("corpus-heading") === heading) {
    draw(result.data.name, result.data.description);
  }
}

/* A collection stores piece ids only; this joins them against
 * /api/analyzer/pieces for the same columns the Library shows, falling back
 * to the bare id for a piece not found. */
async function loadCorpusPieces(id, name) {
  const statusEl = document.getElementById("corpus-piece-status");
  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");

  const [detail, library] = await Promise.all([
    fetchJsonResult(`/api/corpus/${id}`),
    fetchJsonResult("/api/analyzer/pieces"),
  ]);
  if (!collectionView || collectionView.id !== id) {
    return;
  }
  if (!detail.ok || !detail.data || !detail.data.ok) {
    statusEl.textContent = `Failed to load pieces: ${collectionResultError(detail, "unknown error")}`;
    statusEl.classList.add("error");
    return;
  }
  const libraryPieces = library.ok && library.data && library.data.ok ? library.data.pieces : [];
  const byId = new Map(libraryPieces.map((piece) => [piece.id, piece]));
  collectionView.name = detail.data.name || name;
  collectionView.libraryPieces = libraryPieces;
  collectionView.members = detail.data.pieces.map((pieceId) => byId.get(pieceId) || { id: pieceId, filename: "" });
  showCollectionMembers();
  if (collectionView.picker) {
    loadPickerPieces();
  }
}

/* The add-pieces picker: the Library's filters (applied by the backend)
 * over the pieces not yet in the collection. */
function openCollectionPicker() {
  const view = collectionView;
  const panel = document.getElementById("corpus-picker");
  view.picker = { filter: emptyLibraryFilter(), fetched: [], shown: [], table: null };
  panel.hidden = false;
  panel.innerHTML = `
    <div class="filter-row">
      <input type="search" id="picker-search" placeholder="Search" aria-label="Search pieces to add" />
      <label class="filter-field">Composer <select id="picker-filter-composer"></select></label>
      <label class="filter-field">Source <select id="picker-filter-source"></select></label>
      <label class="filter-field">Tag <select id="picker-filter-tag"></select></label>
      <button type="button" id="picker-clear-filters">Clear filters</button>
      <button type="button" id="picker-close">Close</button>
    </div>
    <div id="picker-actions" class="bulk-bar"></div>
    <div id="picker-status" class="piece-list-status"></div>
    <div id="picker-table-container" hidden></div>
  `;
  const f = () => view.picker.filter;
  const setUnassigned = (which, on) => {
    f().unassigned = f().unassigned.filter((item) => item !== which);
    if (on) {
      f().unassigned.push(which);
    }
  };
  const change = (id, apply) => {
    document.getElementById(id).addEventListener("change", (event) => {
      apply(event.target.value);
      loadPickerPieces();
    });
  };
  change("picker-filter-composer", (value) => {
    setUnassigned("composer", value === UNASSIGNED);
    f().composer = value === UNASSIGNED ? "" : value;
  });
  change("picker-filter-source", (value) => {
    setUnassigned("source", value === UNASSIGNED);
    f().source = value === UNASSIGNED ? "" : value;
  });
  change("picker-filter-tag", (value) => {
    f().untagged = value === UNTAGGED;
    f().tag = f().untagged ? "" : value;
  });
  document.getElementById("picker-search").addEventListener("input", (event) => {
    f().search = event.target.value;
    showPickerPieces();
  });
  document.getElementById("picker-clear-filters").addEventListener("click", () => {
    view.picker.filter = emptyLibraryFilter();
    populatePickerSelects();
    loadPickerPieces();
  });
  document.getElementById("picker-close").addEventListener("click", () => {
    view.picker = null;
    panel.hidden = true;
    panel.innerHTML = "";
  });
  loadLibraryGroups().then(() => {
    if (view.picker) {
      populatePickerSelects();
    }
  });
  loadPickerPieces();
}

function populatePickerSelects() {
  const f = collectionView.picker.filter;
  const g = libraryGroups;
  fillFilterSelect(
    document.getElementById("picker-filter-composer"),
    "Any",
    UNASSIGNED,
    "Unassigned",
    g.unassigned.composer,
    g.composers,
    f.unassigned.includes("composer") ? UNASSIGNED : f.composer,
  );
  fillFilterSelect(
    document.getElementById("picker-filter-source"),
    "Any",
    UNASSIGNED,
    "Unassigned",
    g.unassigned.source,
    g.sources,
    f.unassigned.includes("source") ? UNASSIGNED : f.source,
  );
  fillFilterSelect(
    document.getElementById("picker-filter-tag"),
    "Any",
    UNTAGGED,
    "Untagged",
    g.untagged,
    g.tags,
    f.untagged ? UNTAGGED : f.tag,
  );
  document.getElementById("picker-search").value = f.search;
}

async function loadPickerPieces() {
  const view = collectionView;
  const picker = view.picker;
  const statusEl = document.getElementById("picker-status");
  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");
  const query = PieceMetadata.filterQuery(picker.filter);
  const result = await fetchJsonResult(query ? `/api/analyzer/pieces?${query}` : "/api/analyzer/pieces");
  if (view.picker !== picker) {
    return;
  }
  if (!result.ok || !result.data || !result.data.ok) {
    statusEl.textContent = `Failed to load pieces: ${collectionResultError(result, "unknown error")}`;
    statusEl.classList.add("error");
    return;
  }
  picker.fetched = PieceMetadata.pickerCandidates(
    result.data.pieces,
    view.members.map((piece) => piece.id),
  );
  showPickerPieces();
}

function showPickerPieces() {
  const picker = collectionView.picker;
  const statusEl = document.getElementById("picker-status");
  const tableEl = document.getElementById("picker-table-container");
  picker.shown = PieceMetadata.sortPieces(PieceMetadata.searchPieces(picker.fetched, picker.filter.search), "title");
  picker.table = null;
  tableEl.innerHTML = "";
  updatePickerActions();
  statusEl.classList.remove("error");
  if (picker.shown.length === 0) {
    statusEl.textContent = "No pieces to add.";
    tableEl.hidden = true;
    return;
  }
  statusEl.textContent = "";
  tableEl.hidden = false;
  picker.table = buildPieceTable(tableEl, {
    pieces: picker.shown,
    columns: ["composer", "source", "tags"],
    selectable: true,
    onSelectionChange: updatePickerActions,
  });
}

function updatePickerActions() {
  const picker = collectionView.picker;
  const bar = document.getElementById("picker-actions");
  const selected = picker.table ? picker.table.selectedIds() : [];
  bar.innerHTML = "";
  const addSelected = document.createElement("button");
  addSelected.type = "button";
  addSelected.textContent = `Add selected (${selected.length})`;
  addSelected.disabled = selected.length === 0;
  addSelected.addEventListener("click", () => addPiecesToCollection(selected));
  const addAll = document.createElement("button");
  addAll.type = "button";
  addAll.textContent = `Add all matching (${picker.shown.length})`;
  addAll.disabled = picker.shown.length === 0;
  addAll.addEventListener("click", () => addPiecesToCollection(picker.shown.map((piece) => piece.id)));
  bar.append(addSelected, addAll);
}

/* One POST with piece_ids; the result line reads "Added 5, 2 were already
 * in it". Reloads the members and the picker afterwards. */
async function addPiecesToCollection(ids) {
  const view = collectionView;
  const statusEl = document.getElementById("corpus-piece-add-status");
  statusEl.textContent = "";
  statusEl.classList.remove("error");
  const result = await postJsonResult(`/api/corpus/${view.id}/pieces`, PieceMetadata.addPiecesBody(ids));
  if (!result.ok || !result.data || !result.data.ok) {
    statusEl.textContent = collectionResultError(result, "Add failed.");
    statusEl.classList.add("error");
    return;
  }
  statusEl.textContent = PieceMetadata.addResultText(result.data.added, result.data.already);
  await loadCorpusPieces(view.id, view.name);
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

/* The sidebar Tags panel (tags-panel.js). */
let tagsPanel = null;

async function fetchTagsPanelGroups() {
  const result = await fetchJsonResult("/api/analyzer/groups");
  if (!(result.ok && result.data && result.data.ok)) {
    throw new Error("groups unavailable");
  }
  return result.data;
}

/* Selects the Library tool already filtered to a Tags panel group. */
function openLibraryWithFilter(filter) {
  leaveWorkspace();
  setActiveTool("library");
  renderLibraryView(filter);
}

/* The Styles section of the Collections view: name, source, piece count and
 * updated date with Rebuild and Delete. */
function setStylesStatus(text, isError) {
  const statusEl = document.getElementById("styles-status");
  if (statusEl) {
    statusEl.textContent = text;
    statusEl.classList.toggle("error", Boolean(isError));
  }
}

async function loadStylesSection() {
  const section = document.getElementById("styles-section");
  setStylesStatus("Loading styles...", false);
  const [styles, collections] = await Promise.all([fetchJsonResult("/api/styles"), fetchJsonResult("/api/corpus")]);
  if (document.getElementById("styles-section") !== section) {
    return;
  }
  if (!(styles.ok && styles.data && styles.data.ok)) {
    setStylesStatus(`Failed to load styles: ${collectionResultError(styles, "unknown error")}`, true);
    return;
  }
  const names = {};
  if (collections.ok && collections.data && collections.data.ok) {
    for (const corpus of collections.data.corpora) {
      names[corpus.id] = corpus.name;
    }
  }
  const list = styles.data.styles || [];
  section.innerHTML = "";
  section.hidden = list.length === 0;
  setStylesStatus(list.length === 0 ? "No styles yet. Build one from the Library, a tag or a collection." : "", false);
  if (list.length === 0) {
    return;
  }
  const table = document.createElement("table");
  table.classList.add("library-table");
  const head = document.createElement("tr");
  for (const title of ["Name", "Source", "Pieces", "Updated", "Actions"]) {
    const th = document.createElement("th");
    th.textContent = title;
    head.appendChild(th);
  }
  const thead = document.createElement("thead");
  thead.appendChild(head);
  const tbody = document.createElement("tbody");
  for (const style of list) {
    tbody.appendChild(buildStyleRow(style, names));
  }
  table.append(thead, tbody);
  section.appendChild(table);
}

function buildStyleRow(style, collectionNames) {
  const row = document.createElement("tr");
  const cell = (text) => {
    const td = document.createElement("td");
    td.textContent = text;
    row.appendChild(td);
    return td;
  };
  cell(style.name);
  cell(PieceMetadata.styleSourceText(style.source, collectionNames));
  cell(String(style.piece_count));
  cell(String(style.updated_at || "").slice(0, 10));
  const actions = document.createElement("td");
  const wrap = document.createElement("div");
  wrap.classList.add("library-actions");
  wrap.appendChild(
    buildActionButton({ icon: "↗", label: `Open ${style.name}`, onClick: () => openStyleDetail(style.id) }),
  );
  wrap.appendChild(
    buildActionButton({ icon: "↻", label: `Rebuild ${style.name}`, onClick: () => rebuildStyle(style) }),
  );
  wrap.appendChild(
    buildActionButton({ icon: "🗑", label: `Delete ${style.name}`, onClick: () => deleteStyle(style) }),
  );
  actions.appendChild(wrap);
  row.appendChild(actions);
  return row;
}

async function rebuildStyle(style) {
  setStylesStatus(`Rebuilding ${style.name}...`, false);
  const result = await postJsonResult(`/api/styles/${style.id}/rebuild`, {});
  if (result.ok && result.data && result.data.ok) {
    const skipped = result.data.skipped || [];
    await loadStylesSection();
    setStylesStatus(`${style.name}: ${PieceMetadata.styleBuildText(result.data.style.piece_count, skipped.length, "Rebuilt")}`, false);
  } else {
    setStylesStatus(`Could not rebuild ${style.name}: ${collectionResultError(result, "failed")}`, true);
  }
}

async function deleteStyle(style) {
  if (!window.confirm(`Delete the style "${style.name}"? Compositions that use it can no longer fill or regenerate.`)) {
    return;
  }
  const result = await fetchJsonResult(`/api/styles/${style.id}`, { method: "DELETE" });
  if (result.ok && result.data && result.data.ok) {
    await loadStylesSection();
  } else {
    setStylesStatus(`Could not delete ${style.name}: ${collectionResultError(result, "failed")}`, true);
  }
}

/* The style detail: source, pieces used (linking to the analysis), skipped
 * pieces with reasons, and what the style learned as share bars. notice is
 * the result line shown right after a build. */
function appendStyleShareBars(parent, statistics) {
  const dimensions = StyleView.styleSummaryRows(statistics);
  if (dimensions.length === 0) {
    const none = document.createElement("p");
    none.classList.add("piece-list-status");
    none.textContent = "Nothing learned yet.";
    parent.appendChild(none);
    return;
  }
  for (const dimension of dimensions) {
    const heading = document.createElement("h3");
    heading.classList.add("section-title");
    heading.textContent = dimension.title;
    parent.appendChild(heading);
    for (const category of dimension.categories) {
      const block = document.createElement("div");
      block.classList.add("share-group");
      const name = document.createElement("h4");
      name.classList.add("share-title");
      name.textContent = category.title;
      block.appendChild(name);
      for (const row of category.rows) {
        const line = document.createElement("div");
        line.classList.add("share-row");
        const label = document.createElement("span");
        label.classList.add("share-label");
        label.textContent = row.label;
        const bar = document.createElement("div");
        bar.classList.add("share-bar");
        const fill = document.createElement("div");
        fill.classList.add("share-fill");
        fill.style.width = `${row.percent}%`;
        bar.appendChild(fill);
        const text = document.createElement("span");
        text.classList.add("share-text");
        text.textContent = row.text;
        line.append(label, bar, text);
        block.appendChild(line);
      }
      if (category.moreText) {
        const more = document.createElement("p");
        more.classList.add("share-more");
        more.textContent = category.moreText;
        block.appendChild(more);
      }
      parent.appendChild(block);
    }
  }
}

async function openStyleDetail(id, notice) {
  leaveWorkspace();
  setActiveTool("corpus");
  if (tagsPanel) {
    tagsPanel.setActive(null);
  }
  const content = getContentEl();
  content.innerHTML = `
    <div id="style-back-container"></div>
    <h1 id="style-title" class="view-title">Style</h1>
    <div id="style-notice" class="style-result" role="status"></div>
    <div id="style-status" class="piece-list-status">Loading style...</div>
    <div id="style-body"></div>
  `;
  document.getElementById("style-back-container").appendChild(renderCorpusBackLink());
  document.getElementById("style-notice").textContent = notice || "";
  const statusEl = document.getElementById("style-status");
  const body = document.getElementById("style-body");

  const [detail, library, collections] = await Promise.all([
    fetchJsonResult(`/api/styles/${id}`),
    fetchJsonResult("/api/analyzer/pieces"),
    fetchJsonResult("/api/corpus"),
  ]);
  if (document.getElementById("style-body") !== body) {
    return;
  }
  if (!(detail.ok && detail.data && detail.data.ok)) {
    statusEl.textContent = `Failed to load the style: ${collectionResultError(detail, "unknown error")}`;
    statusEl.classList.add("error");
    return;
  }
  statusEl.textContent = "";
  const style = detail.data.style;
  const titles = new Map();
  if (library.ok && library.data && library.data.ok) {
    for (const piece of library.data.pieces) {
      titles.set(piece.id, piece.filename || piece.id);
    }
  }
  const collectionNames = {};
  if (collections.ok && collections.data && collections.data.ok) {
    for (const corpus of collections.data.corpora) {
      collectionNames[corpus.id] = corpus.name;
    }
  }

  document.getElementById("style-title").textContent = style.name;
  const meta = document.createElement("p");
  meta.classList.add("collection-description");
  meta.textContent = [
    PieceMetadata.styleSourceText(style.source, collectionNames),
    PieceMetadata.pieceCountText(style.piece_count),
    `updated ${String(style.updated_at || "").slice(0, 10)}`,
  ].join(" · ");
  body.appendChild(meta);

  const piecesHeading = document.createElement("h2");
  piecesHeading.classList.add("section-title");
  piecesHeading.textContent = "Pieces used";
  const pieceList = document.createElement("ul");
  pieceList.classList.add("style-pieces");
  for (const pieceId of style.pieces || []) {
    const li = document.createElement("li");
    if (titles.has(pieceId)) {
      const link = document.createElement("button");
      link.type = "button";
      link.classList.add("link-button");
      link.textContent = titles.get(pieceId);
      link.addEventListener("click", () => showAnalysisView(pieceId));
      li.appendChild(link);
    } else {
      li.textContent = `${pieceId} (no longer in the Library)`;
    }
    pieceList.appendChild(li);
  }
  body.append(piecesHeading, pieceList);

  if ((style.skipped || []).length > 0) {
    const skippedHeading = document.createElement("h2");
    skippedHeading.classList.add("section-title");
    skippedHeading.textContent = "Skipped";
    const skippedList = document.createElement("ul");
    skippedList.classList.add("style-pieces");
    for (const item of style.skipped) {
      const li = document.createElement("li");
      li.textContent = `${titles.get(item.id) || item.id}: ${item.reason}`;
      skippedList.appendChild(li);
    }
    body.append(skippedHeading, skippedList);
  }

  const learnedHeading = document.createElement("h2");
  learnedHeading.classList.add("section-title");
  learnedHeading.textContent = "What it learned";
  body.appendChild(learnedHeading);
  appendStyleShareBars(body, style.statistics);
}

/* Building a style (POST /api/styles): an inline name prompt prefilled with
 * the default name, a "Building..." state on the button, and, on success, the
 * style detail with the result line. A failure is shown beside the button. */
function openStyleNamePrompt(host, defaultName, onSubmit) {
  host.innerHTML = "";
  const row = document.createElement("div");
  row.classList.add("style-prompt");
  const input = document.createElement("input");
  input.type = "text";
  input.value = defaultName;
  input.setAttribute("aria-label", "Style name");
  const build = document.createElement("button");
  build.type = "button";
  build.textContent = "Build";
  const cancel = document.createElement("button");
  cancel.type = "button";
  cancel.textContent = "Cancel";
  row.append(input, build, cancel);
  host.appendChild(row);
  input.focus();
  input.select();
  cancel.addEventListener("click", () => {
    host.innerHTML = "";
  });
  build.addEventListener("click", () => onSubmit(input.value));
  row.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      host.innerHTML = "";
    } else if (event.key === "Enter") {
      build.click();
    }
  });
}

/* source is {kind: "filter"|"collection"|"pieces", ...} (see
 * PieceMetadata.styleRequestBody). */
function startStyleBuild({ source, host, trigger, resultEl }) {
  openStyleNamePrompt(host, PieceMetadata.defaultStyleName(source), async (name) => {
    host.innerHTML = "";
    const label = trigger.textContent;
    trigger.textContent = "Building...";
    trigger.disabled = true;
    resultEl.classList.remove("error");
    resultEl.textContent = "";
    const result = await postJsonResult("/api/styles", PieceMetadata.styleRequestBody(source, name));
    trigger.textContent = label;
    trigger.disabled = false;
    if (result.ok && result.data && result.data.ok) {
      const skipped = result.data.skipped || [];
      openStyleDetail(result.data.style.id, PieceMetadata.styleBuildText(result.data.style.piece_count, skipped.length));
    } else {
      resultEl.textContent = collectionResultError(result, "Could not build the style.");
      resultEl.classList.add("error");
    }
  });
}

/* Build style on a Tags panel row. Rows that are not a plain composer,
 * source or tag value (Unassigned, Untagged) build from the matching ids. */
async function buildStyleFromPanelRow(row, trigger) {
  const resultEl = document.getElementById("tags-panel-style-result");
  let source = PieceMetadata.styleSourceForFilter(row.filter, []);
  if (source.kind === "pieces") {
    const listed = await fetchJsonResult(`/api/analyzer/pieces?${PieceMetadata.filterQuery(row.filter)}`);
    if (!(listed.ok && listed.data && listed.data.ok)) {
      resultEl.textContent = collectionResultError(listed, "Could not list the pieces.");
      resultEl.classList.add("error");
      return;
    }
    source = { kind: "pieces", piece_ids: listed.data.pieces.map((piece) => piece.id) };
  }
  startStyleBuild({
    source,
    host: document.getElementById("tags-panel-style-prompt"),
    trigger,
    resultEl,
  });
}

function mountTagsPanel() {
  const el = document.getElementById("tags-panel");
  tagsPanel = TagsPanel.mount(el, {
    fetchGroups: fetchTagsPanelGroups,
    onSelect: openLibraryWithFilter,
    onBuildStyle: buildStyleFromPanelRow,
  });
  tagsPanel.refresh();
}

/* The sidebar Subjects panel; null until mounted. Call notifySubjectsChanged()
 * after any subject or link change so its counts follow. */
let subjectsPanel = null;

async function fetchSubjectGroups() {
  const result = await fetchJsonResult("/api/subjects/groups");
  if (!(result.ok && result.data && result.data.ok)) {
    throw new Error("subject groups unavailable");
  }
  return result.data.groups;
}

function notifySubjectsChanged() {
  if (subjectsPanel) {
    subjectsPanel.refresh();
  }
}

function mountSubjectsPanel() {
  subjectsPanel = SubjectsPanel.mount(document.getElementById("subjects-panel"), {
    fetchGroups: fetchSubjectGroups,
    onSelect: openSubject,
  });
  subjectsPanel.refresh();
}

/* The Composer's words are loaded before any view mounts (the models read their lists from them). */
async function boot() {
  try {
    await Vocabulary.load();
  } catch (err) {
    getContentEl().innerHTML = `<section class="panel"><h1>Composer unavailable</h1><p class="error">${err.message}</p></section>`;
    return;
  }
  mountTagsPanel();
  mountSubjectsPanel();
  mountProjectPicker();
  selectTool("library");
}

boot();
