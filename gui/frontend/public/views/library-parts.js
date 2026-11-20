import { shell } from "./shell-context.js";
import { fetchJsonResult } from "../core/api.js";

/* What the Library, Collections and Styles views share: the filter constants, the piece table with its rows, the action button, the collection back link and the collection error text. */
/* The Library tool: the piece table with its filters, bulk edits and saved collections, the editor of a piece, re-analysis, deletion and the upload queue. */
export const UNASSIGNED = "__unassigned__";
export const UNTAGGED = "__untagged__";

export function emptyLibraryFilter() {
  return { composer: "", source: "", tag: "", unassigned: [], untagged: false, search: "", sort: "added" };
}

export function fillFilterSelect(select, anyLabel, specialValue, specialLabel, special, groupList, current) {
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

/* Icon-only action button. icon is a single glyph (no icon font/CDN --
 * plain Unicode, consistent with this app's no-external-assets style);
 * label is the accessible name, shown as both aria-label and a hover
 * tooltip so the icon-only button stays identifiable. */
export function buildActionButton({ icon, label, disabled, onClick }) {
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
  if (piece.analysis_status && piece.analysis_status !== "current") {
    const badge = document.createElement("span");
    badge.className = "stale-badge";
    badge.textContent = "stale";
    badge.title = `Analysis ${piece.analysis_status.replace("_", " ")}; it is analyzed again when opened.`;
    span.append(" ", badge);
  }
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

const PIECE_TABLE_COLUMN_LABELS = { composer: "Composer", source: "Source", tags: "Tags" };

/* The shared piece table used by the Library, a collection's detail and the
 * add-pieces picker. options: pieces; columns (any of "composer", "source",
 * "tags" after the always-present Title); selectable (a checkbox column and
 * a select-all box); rowActions(row, actionsCell, piece); onOpen(piece) for a
 * title click; onSelectionChange(). Draws into container and returns
 * {table, columnCount, buildRow(piece), selectedIds(), selectAll(checked),
 * clearSelection()}. */
export function buildPieceTable(container, options) {
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
export let libraryGroups = { composers: [], sources: [], tags: [], unassigned: { composer: 0, source: 0 }, untagged: 0, total: 0 };

export async function loadLibraryGroups() {
  const result = await fetchJsonResult("/api/analyzer/groups");
  if (result.ok && result.data && result.data.ok) {
    libraryGroups = result.data;
  }
  return libraryGroups;
}

export function collectionResultError(result, fallback) {
  return (result.data && result.data.error) || fallback;
}

export function renderCorpusBackLink() {
  const back = document.createElement("a");
  back.textContent = "Back to list";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => shell().showCollections());
  return back;
}
