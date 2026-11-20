import PieceMetadata from "../models/piece-metadata.js";
import StyleView from "../models/style-view.js";
import { buildActionButton, collectionResultError, renderCorpusBackLink } from "./library-parts.js";
import { showAnalysisView } from "./analysis.js";
import { shell } from "./shell-context.js";
import { fetchJsonResult, jobJsonResult } from "../core/api.js";

/* The styles of the Library sidebar: the style list, its build, a style's statistics and its name prompt. */
/* The Styles section of the Collections view: name, source, piece count and
 * updated date with Rebuild and Delete. */
function setStylesStatus(text, isError) {
  const statusEl = document.getElementById("styles-status");
  if (statusEl) {
    statusEl.textContent = text;
    statusEl.classList.toggle("error", Boolean(isError));
  }
}

export async function loadStylesSection() {
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
  const result = await jobJsonResult(`/api/styles/${style.id}/rebuild`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({}),
  });
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
  shell().leaveWorkspace();
  shell().setActiveTool("corpus");
  shell().setTagsActive(null);
  const content = shell().contentEl();
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
export function startStyleBuild({ source, host, trigger, resultEl }) {
  openStyleNamePrompt(host, PieceMetadata.defaultStyleName(source), async (name) => {
    host.innerHTML = "";
    const label = trigger.textContent;
    trigger.textContent = "Building...";
    trigger.disabled = true;
    resultEl.classList.remove("error");
    resultEl.textContent = "";
    const result = await jobJsonResult("/api/styles", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(PieceMetadata.styleRequestBody(source, name)),
    });
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
export async function buildStyleFromPanelRow(row, trigger) {
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
