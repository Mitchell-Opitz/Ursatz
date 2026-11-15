/* The Motive links tool (R-154, R-156): pick a scope, run the link search over
 * its pieces, and follow the run. Pure parts are in motive-links-model.js;
 * fetchJsonResult, materialsElement, getContentEl, subjectsState and the page
 * globals come from app.js, materials-view.js and subjects-view.js. */
const motiveLinksState = { timer: null };

function motiveLinksStatus(text, isError) {
  const el = materialsElement("div", isError ? "upload-status error" : "upload-status", text || "");
  el.setAttribute("role", "status");
  return el;
}

function stopMotiveLinksPolling() {
  if (motiveLinksState.timer !== null) {
    clearInterval(motiveLinksState.timer);
    motiveLinksState.timer = null;
  }
}

function motiveLinksSelect(options, placeholder) {
  const select = materialsElement("select");
  if (placeholder !== undefined) {
    const blank = materialsElement("option", "", placeholder);
    blank.value = "";
    select.appendChild(blank);
  }
  for (const option of options) {
    const el = materialsElement("option", "", option.label);
    el.value = option.value;
    select.appendChild(el);
  }
  return select;
}

/* The pickers of the scope kinds, filled from the existing lists. */
async function loadMotiveLinksChoices() {
  const [collections, pieces, subjects, projects] = await Promise.all([
    fetchJsonResult("/api/corpus"),
    fetchJsonResult("/api/analyzer/pieces"),
    loadSubjectList(),
    sendMaterialsRequest(MaterialsModel.buildProjectListRequest()),
  ]);
  const groups = PieceMetadata.groupValues(pieces.ok && pieces.data && Array.isArray(pieces.data.pieces) ? pieces.data.pieces : []);
  const valueOptions = (list) => list.map((entry) => ({ value: entry.value, label: entry.value }));
  return {
    collections: collections.ok && collections.data && Array.isArray(collections.data.corpora) ? collections.data.corpora : [],
    composers: valueOptions(groups.composers),
    sources: valueOptions(groups.sources),
    tags: valueOptions(groups.tags),
    subjects: subjectsState.subjects,
    projects: projects.ok ? MaterialsModel.projectsFrom(projects.data) : [],
  };
}

/* Names for the scope text of earlier runs. */
function motiveLinksNames(choices) {
  const table = (list) => Object.fromEntries(list.map((entry) => [entry.id, entry.name]));
  return {
    collections: table(choices.collections),
    subjects: table(choices.subjects),
    projects: table(choices.projects),
  };
}

/* Opens a piece's Analysis at one motive family. */
function openMotiveFamily(pieceId, family) {
  showAnalysisView(pieceId, { motiveFamily: family });
}

function motiveLinksTable(headings, rows) {
  const table = materialsElement("table", "library-table");
  const head = materialsElement("tr");
  for (const label of headings) {
    head.appendChild(materialsElement("th", "", label));
  }
  table.appendChild(head);
  for (const cells of rows) {
    const tr = materialsElement("tr");
    for (const cell of cells) {
      const td = materialsElement("td");
      if (cell instanceof Node) {
        td.appendChild(cell);
      } else {
        td.textContent = String(cell);
      }
      tr.appendChild(td);
    }
    table.appendChild(tr);
  }
  const panel = materialsElement("section", "panel library-panel");
  panel.appendChild(table);
  return panel;
}

function motiveLinksPieceLink(row) {
  const link = materialsElement("a", "clickable", row.title);
  link.addEventListener("click", () => openMotiveFamily(row.piece, row.family));
  return link;
}

/* The groups first, then the links table, of a done run's result. */
function renderMotiveLinksResult(target, result) {
  target.innerHTML = "";
  const groups = MotiveLinksModel.groupRows(result);
  target.appendChild(materialsElement("h2", "section-title", "Groups"));
  if (groups.length === 0) {
    target.appendChild(materialsElement("div", "piece-list-status", "No linked families."));
  } else {
    const rows = [];
    groups.forEach((group, i) => {
      for (const member of group.members) {
        rows.push([i + 1, motiveLinksPieceLink(member), member.measures, member.degrees]);
      }
    });
    target.appendChild(motiveLinksTable(["Group", "Piece", "Measures", "Degrees"], rows));
  }

  target.appendChild(materialsElement("h2", "section-title", "Links"));
  const links = MotiveLinksModel.linkRows(result);
  if (links.length === 0) {
    target.appendChild(materialsElement("div", "piece-list-status", "No links found."));
    return;
  }
  const rows = links.map((link) => [
    motiveLinksPieceLink(link.a),
    link.a.measures,
    link.a.degrees,
    motiveLinksPieceLink(link.b),
    link.b.measures,
    link.b.degrees,
    link.steps,
    link.distance,
    link.kept.join(", "),
    link.changed.join(", "),
  ]);
  target.appendChild(
    motiveLinksTable(
      ["Piece A", "Measures", "Degrees", "Piece B", "Measures", "Degrees", "Steps", "Distance", "Kept", "Changed"],
      rows,
    ),
  );
}

async function renderMotiveLinksView() {
  stopMotiveLinksPolling();
  const content = getContentEl();
  content.innerHTML = "";
  content.appendChild(materialsElement("h1", "view-title", "Motive links"));

  const choices = await loadMotiveLinksChoices();
  const selection = { kind: "all" };

  const controls = materialsElement("div", "upload-controls");
  const kind = motiveLinksSelect(
    MotiveLinksModel.SCOPE_KINDS.map((entry) => ({ value: entry.kind, label: entry.title })),
  );
  const pickers = materialsElement("span");
  const run = materialsElement("button", "", "Run");
  run.type = "button";
  controls.append(kind, pickers, run);

  const progressEl = motiveLinksStatus("");
  const skippedEl = materialsElement("div", "piece-list-status");
  const resultEl = materialsElement("div");
  const runsEl = materialsElement("div");
  content.append(controls, progressEl, skippedEl, resultEl, materialsElement("h2", "section-title", "Earlier runs"), runsEl);
  const names = motiveLinksNames(choices);

  function drawPickers() {
    pickers.innerHTML = "";
    const bind = (select, key) => {
      select.addEventListener("change", () => {
        selection[key] = select.value;
      });
      selection[key] = select.value;
      pickers.append(select, document.createTextNode(" "));
    };
    const idOptions = (list) => list.map((entry) => ({ value: entry.id, label: entry.name }));
    switch (selection.kind) {
      case "collection":
        bind(motiveLinksSelect(idOptions(choices.collections), "Choose a collection"), "collectionId");
        break;
      case "filter":
        bind(motiveLinksSelect(choices.composers, "Any composer"), "composer");
        bind(motiveLinksSelect(choices.sources, "Any source"), "source");
        bind(motiveLinksSelect(choices.tags, "Any tag"), "tag");
        break;
      case "subject":
        bind(motiveLinksSelect(idOptions(choices.subjects), "Choose a subject"), "subjectId");
        break;
      case "project":
        bind(motiveLinksSelect(idOptions(choices.projects), "Choose a project"), "projectId");
        break;
      default:
        break;
    }
  }
  kind.addEventListener("change", () => {
    selection.kind = kind.value;
    drawPickers();
  });
  drawPickers();

  function showRun(runData) {
    progressEl.textContent = MotiveLinksModel.progressText(runData);
    progressEl.classList.toggle("error", runData.status === "failed");
    skippedEl.textContent = MotiveLinksModel.skippedLines(runData).join("; ");
  }

  async function poll(id) {
    const answer = await fetchJsonResult(`/api/links/runs/${id}`);
    if (!progressEl.isConnected) {
      stopMotiveLinksPolling();
      return;
    }
    if (!answer.ok || !answer.data || !answer.data.run) {
      stopMotiveLinksPolling();
      progressEl.textContent = (answer.data && (answer.data.message || answer.data.error)) || "Could not read the run";
      progressEl.classList.add("error");
      run.disabled = false;
      return;
    }
    showRun(answer.data.run);
    if (answer.data.run.status !== "running") {
      stopMotiveLinksPolling();
      run.disabled = false;
      if (answer.data.run.status === "done") {
        renderMotiveLinksResult(resultEl, answer.data.run.result);
      }
      loadRuns();
    }
  }

  async function openRun(id) {
    const answer = await fetchJsonResult(`/api/links/runs/${id}`);
    if (!answer.ok || !answer.data || !answer.data.run) {
      progressEl.textContent = (answer.data && (answer.data.message || answer.data.error)) || "Could not read the run";
      progressEl.classList.add("error");
      return;
    }
    progressEl.classList.remove("error");
    showRun(answer.data.run);
    resultEl.innerHTML = "";
    if (answer.data.run.status === "done") {
      renderMotiveLinksResult(resultEl, answer.data.run.result);
    }
  }

  async function deleteRun(id) {
    const answer = await fetchJsonResult(`/api/links/runs/${id}`, { method: "DELETE" });
    if (!answer.ok) {
      progressEl.textContent = (answer.data && (answer.data.message || answer.data.error)) || "Could not delete the run";
      progressEl.classList.add("error");
      return;
    }
    loadRuns();
  }

  async function loadRuns() {
    const answer = await fetchJsonResult("/api/links/runs");
    runsEl.innerHTML = "";
    const rows = MotiveLinksModel.runRows(answer.ok && answer.data ? answer.data.runs : [], names);
    if (rows.length === 0) {
      runsEl.appendChild(materialsElement("div", "piece-list-status", "No earlier runs."));
      return;
    }
    const list = materialsElement("ul", "materials-list");
    for (const row of rows) {
      const li = materialsElement("li");
      const open = materialsElement("a", "clickable", row.label);
      open.addEventListener("click", () => openRun(row.id));
      li.append(open, document.createTextNode(row.progress ? ` (${row.progress}) ` : " "));
      const del = materialsElement("button", "", "Delete");
      del.type = "button";
      del.disabled = row.status === "running";
      del.addEventListener("click", () => deleteRun(row.id));
      li.appendChild(del);
      list.appendChild(li);
    }
    runsEl.appendChild(list);
  }
  loadRuns();

  run.addEventListener("click", async () => {
    const body = MotiveLinksModel.scopeBody(selection);
    if (!body) {
      progressEl.textContent = "Choose what to compare first.";
      progressEl.classList.add("error");
      return;
    }
    run.disabled = true;
    progressEl.classList.remove("error");
    progressEl.textContent = "Starting...";
    skippedEl.textContent = "";
    const answer = await fetchJsonResult("/api/links/runs", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!answer.ok || !answer.data || !answer.data.run) {
      progressEl.textContent = (answer.data && (answer.data.message || answer.data.error)) || "Could not start the run";
      progressEl.classList.add("error");
      run.disabled = false;
      return;
    }
    const id = answer.data.run.id;
    showRun(answer.data.run);
    stopMotiveLinksPolling();
    motiveLinksState.timer = setInterval(() => poll(id), 1000);
  });
}
