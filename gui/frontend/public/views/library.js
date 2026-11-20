import ApiRequest from "../core/request.js";
import BackendJobs from "../core/backend-jobs.js";
import PieceMetadata from "../models/piece-metadata.js";
import UploadQueue from "../core/upload-queue.js";
import { UNASSIGNED, UNTAGGED, buildActionButton, buildPieceTable, collectionResultError, emptyLibraryFilter, fillFilterSelect, libraryGroups, loadLibraryGroups } from "./library-parts.js";
import { formatPieceLabel, reanalyzePiece, showAnalysisView } from "./analysis.js";
import { showCorpusDetail } from "./collections.js";
import { startStyleBuild } from "./styles.js";
import { buildSubjectsField } from "./subjects-view.js";
import { shell } from "./shell-context.js";
import { commandResult, fetchJsonResult } from "../core/api.js";
import { commandRequest } from "../core/commands.js";

/* The Library's current filter: composer, source, tag, unassigned (list of
 * "composer"/"source"), untagged, plus client-side search and sort. */
let libraryFilter = emptyLibraryFilter();

/* The pieces currently shown, after search and sort. */
let libraryShownPieces = [];

export async function renderLibraryView(filter) {
  libraryFilter = { ...emptyLibraryFilter(), ...(filter || {}) };
  const content = shell().contentEl();
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

export async function loadLibraryPieces() {
  const statusEl = document.getElementById("library-status");
  const tableEl = document.getElementById("library-table-container");

  shell().setTagsActive(libraryFilter);
  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");
  tableEl.hidden = true;
  tableEl.innerHTML = "";
  libraryTable = null;

  try {
    const query = PieceMetadata.filterQuery(libraryFilter);
    const answer = await ApiRequest.apiRequest("GET", query ? `/api/analyzer/pieces?${query}` : "/api/analyzer/pieces");
    const data = answer.data || {};

    if (!answer.ok) {
      statusEl.textContent = `Failed to load pieces: ${ApiRequest.errorText(data, "unknown error")}`;
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
  const created = await commandResult("collection.create", { params: { name } });
  if (!created.ok || !created.data || !created.data.ok) {
    statusEl.textContent = collectionResultError(created, "Could not create the collection.");
    statusEl.classList.add("error");
    button.disabled = false;
    return;
  }
  const added = await commandResult("collection.add-pieces", { target: created.data.id, params: PieceMetadata.addPiecesBody(ids) });
  if (!added.ok || !added.data || !added.data.ok) {
    statusEl.textContent = `Created "${created.data.name}" but adding pieces failed: ${collectionResultError(added, "failed")}`;
    statusEl.classList.add("error");
    button.disabled = false;
    return;
  }
  shell().setActiveTool("corpus");
  shell().setTagsActive(null);
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
    const result = await commandResult("library.update", { target: id, params: body });
    if (!result.ok || !result.data || !result.data.ok) {
      const reason = (result.data && result.data.error) || "failed";
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

/* Called after any upload, edit, bulk edit or delete in the Library;
 * refreshes the sidebar Tags panel. */
function notifyMetadataChanged() {
  shell().refreshTags();
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
    const result = await commandResult("library.update", { target: piece.id, params: body });
    if (!result.ok || !result.data || !result.data.ok) {
      errorEl.textContent = (result.data && result.data.error) || "Save failed.";
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
    const answer = await commandResult("library.delete", { target: id });
    const data = answer.data || {};

    if (!answer.ok) {
      statusEl.textContent = ApiRequest.errorText(data, "Delete failed.");
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
      const res = await BackendJobs.runJob(
        commandRequest("library.import", { params: { filename: files[i].name }, file: bytes, filename: files[i].name }),
      );
      const data = res.body || {};
      if (!data.ok) {
        queue = UploadQueue.markFailed(queue, i, data.error || "Upload failed.");
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
