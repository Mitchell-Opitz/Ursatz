import ApiRequest from "../core/request.js";
import PieceMetadata from "../models/piece-metadata.js";
import { UNASSIGNED, UNTAGGED, buildActionButton, buildPieceTable, collectionResultError, emptyLibraryFilter, fillFilterSelect, libraryGroups, loadLibraryGroups, renderCorpusBackLink } from "./library-parts.js";
import { showAnalysisView } from "./analysis.js";
import { loadStylesSection, startStyleBuild } from "./styles.js";
import { shell } from "./shell-context.js";
import { commandResult, fetchJsonResult } from "../core/api.js";

/* The Collections tool: the collection cards, a collection's members and the picker that adds pieces to it. */
export async function renderCorpusView() {
  const content = shell().contentEl();
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
    const answer = await ApiRequest.apiRequest("GET", "/api/corpus");
    const data = answer.data || {};

    if (!answer.ok) {
      statusEl.textContent = `Failed to load collections: ${ApiRequest.errorText(data, "unknown error")}`;
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

/* Resolves to Map(collection id -> collectionSummary) from each collection's groups (the groups route limited to its members). A failed lookup
 * leaves that card without a summary. */
async function loadCollectionSummaries(collections) {
  const summaries = new Map();
  await Promise.all(
    collections.map(async (corpus) => {
      const grouped = await fetchJsonResult(`/api/analyzer/groups?${PieceMetadata.filterQuery({}, corpus.id)}`);
      if (grouped.ok && grouped.data && grouped.data.ok) {
        summaries.set(corpus.id, PieceMetadata.collectionSummary(grouped.data));
      }
    }),
  );
  return summaries;
}

function patchCollection(id, body) {
  return commandResult("collection.update", { target: id, params: body });
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
    const result = await commandResult("collection.delete", { target: corpus.id });
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
    const answer = await commandResult("collection.create", { params: { name, description: document.getElementById("corpus-description-input").value } });
    const data = answer.data || {};

    if (!answer.ok) {
      statusEl.textContent = ApiRequest.errorText(data, "Create failed.");
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

/* State of the open collection detail: the ids of its members, the members the filter keeps (as the backend lists them), the groups of all its
 * members (for the filter selects), the filter and the shared table. */
let collectionView = null;

export function showCorpusDetail(id, name) {
  collectionView = { id, name, memberIds: [], members: [], groups: emptyGroups(), filter: emptyLibraryFilter(), table: null };
  const content = shell().contentEl();
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
      loadCollectionMembers();
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
    loadCollectionMembers();
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
  const groups = view.groups;
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

  const shown = PieceMetadata.sortPieces(PieceMetadata.searchPieces(view.members, f.search), "title");
  const statusEl = document.getElementById("corpus-piece-status");
  const tableEl = document.getElementById("corpus-piece-table-container");
  document.getElementById("corpus-count").textContent = isCollectionFilterOn()
    ? `${shown.length} of ${view.memberIds.length} pieces`
    : "";
  view.table = null;
  tableEl.innerHTML = "";
  updateCollectionBulkBar();
  statusEl.classList.remove("error");
  if (shown.length === 0) {
    statusEl.textContent = view.memberIds.length === 0 ? "No pieces yet." : "No pieces match.";
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
    const result = await commandResult("collection.remove-piece", { target: view.id, params: { piece_id: pieceId } });
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

/* The groups of a collection's members with no pieces: what the filter selects show before the groups are read. */
function emptyGroups() {
  return { composers: [], sources: [], tags: [], unassigned: { composer: 0, source: 0 }, untagged: 0, total: 0 };
}

/* The members the filter keeps, listed by the backend (the Library list limited to this collection), then drawn; the search box narrows them
 * in the page. */
async function loadCollectionMembers() {
  const view = collectionView;
  const query = PieceMetadata.filterQuery(view.filter, view.id);
  const listed = await fetchJsonResult(`/api/analyzer/pieces?${query}`);
  if (collectionView !== view) {
    return;
  }
  if (!listed.ok || !listed.data || !listed.data.ok) {
    const statusEl = document.getElementById("corpus-piece-status");
    statusEl.textContent = `Failed to load pieces: ${collectionResultError(listed, "unknown error")}`;
    statusEl.classList.add("error");
    return;
  }
  view.members = listed.data.pieces;
  showCollectionMembers();
}

/* The open collection from the backend: its name and member ids, the groups of its members for the filter selects and the members the filter
 * keeps (the Library list and groups limited to this collection). */
async function loadCorpusPieces(id, name) {
  const statusEl = document.getElementById("corpus-piece-status");
  statusEl.textContent = "Loading pieces...";
  statusEl.classList.remove("error");

  const [detail, grouped] = await Promise.all([
    fetchJsonResult(`/api/corpus/${id}`),
    fetchJsonResult(`/api/analyzer/groups?${PieceMetadata.filterQuery({}, id)}`),
  ]);
  if (!collectionView || collectionView.id !== id) {
    return;
  }
  if (!detail.ok || !detail.data || !detail.data.ok) {
    statusEl.textContent = `Failed to load pieces: ${collectionResultError(detail, "unknown error")}`;
    statusEl.classList.add("error");
    return;
  }
  collectionView.name = detail.data.name || name;
  collectionView.memberIds = detail.data.pieces;
  collectionView.groups = grouped.ok && grouped.data && grouped.data.ok ? grouped.data : emptyGroups();
  await loadCollectionMembers();
  if (collectionView && collectionView.id === id && collectionView.picker) {
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
    view.memberIds,
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
  const result = await commandResult("collection.add-pieces", { target: view.id, params: PieceMetadata.addPiecesBody(ids) });
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
    const answer = await commandResult("collection.remove-piece", { target: corpusId, params: { piece_id: pieceId } });
    const data = answer.data || {};

    if (!answer.ok) {
      const statusEl = document.getElementById("corpus-piece-status");
      statusEl.textContent = ApiRequest.errorText(data, "Remove failed.");
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
