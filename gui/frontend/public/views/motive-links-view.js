import BackendJobs from "../core/backend-jobs.js";
import { el as element } from "../core/dom.js";
import MaterialsModel from "../models/materials-model.js";
import MotiveLinksModel from "../models/motive-links-model.js";
import { sendMaterialsRequest } from "./materials-common.js";
import { loadSubjectList, subjectsState } from "./subjects-view.js";
import { shell } from "./shell-context.js";
import { fetchJsonResult } from "../core/api.js";

/* The Motive links tool (R-154, R-156): pick a scope, run the link search over
 * its pieces, and follow the run. Pure parts are in motive-links-model.js;
 * fetchJsonResult, element, getContentEl, subjectsState and the page
 * globals come from app.js, materials-view.js and subjects-view.js. */
const motiveLinksState = { timer: null };

function motiveLinksStatus(text, isError) {
  const el = element("div", isError ? "upload-status error" : "upload-status", text || "");
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
  const select = element("select");
  if (placeholder !== undefined) {
    const blank = element("option", "", placeholder);
    blank.value = "";
    select.appendChild(blank);
  }
  for (const option of options) {
    const el = element("option", "", option.label);
    el.value = option.value;
    select.appendChild(el);
  }
  return select;
}

/* The pickers of the scope kinds, filled from the existing lists. */
export async function loadMotiveLinksChoices() {
  const [collections, grouped, subjects, projects] = await Promise.all([
    fetchJsonResult("/api/corpus"),
    fetchJsonResult("/api/analyzer/groups"),
    loadSubjectList(),
    sendMaterialsRequest(MaterialsModel.buildProjectListRequest()),
  ]);
  const groups = grouped.ok && grouped.data ? grouped.data : {};
  const valueOptions = (list) => list.map((entry) => ({ value: entry.value, label: entry.value }));
  return {
    collections: collections.ok && collections.data && Array.isArray(collections.data.corpora) ? collections.data.corpora : [],
    composers: valueOptions(groups.composers || []),
    sources: valueOptions(groups.sources || []),
    tags: valueOptions(groups.tags || []),
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
  shell().showAnalysis(pieceId, { motiveFamily: family });
}

function motiveLinksTable(headings, rows) {
  const table = element("table", "library-table");
  const head = element("tr");
  for (const label of headings) {
    head.appendChild(element("th", "", label));
  }
  table.appendChild(head);
  for (const cells of rows) {
    const tr = element("tr");
    for (const cell of cells) {
      const td = element("td");
      if (cell instanceof Node) {
        td.appendChild(cell);
      } else {
        td.textContent = String(cell);
      }
      tr.appendChild(td);
    }
    table.appendChild(tr);
  }
  const panel = element("section", "panel library-panel");
  panel.appendChild(table);
  return panel;
}

function motiveLinksPieceLink(row) {
  const link = element("a", "clickable", row.title);
  link.addEventListener("click", () => openMotiveFamily(row.piece, row.family));
  return link;
}

/* The groups first, then the links table, of a done run's result. */
function renderMotiveLinksResult(target, result) {
  target.innerHTML = "";
  const groups = MotiveLinksModel.groupRows(result);
  target.appendChild(element("h2", "section-title", "Groups"));
  if (groups.length === 0) {
    target.appendChild(element("div", "piece-list-status", "No linked families."));
  } else {
    const rows = [];
    groups.forEach((group, i) => {
      for (const member of group.members) {
        rows.push([i + 1, motiveLinksPieceLink(member), member.measures, member.degrees]);
      }
    });
    target.appendChild(motiveLinksTable(["Group", "Piece", "Measures", "Degrees"], rows));
  }

  target.appendChild(element("h2", "section-title", "Links"));
  const links = MotiveLinksModel.linkRows(result);
  if (links.length === 0) {
    target.appendChild(element("div", "piece-list-status", "No links found."));
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

/* The open view's reload of its list of runs (set while the view is open; the event stream calls it when a run changes). */
export let reloadMotiveLinkRuns = null;

export async function renderMotiveLinksView() {
  stopMotiveLinksPolling();
  const content = shell().contentEl();
  content.innerHTML = "";
  content.appendChild(element("h1", "view-title", "Motive links"));

  const choices = await loadMotiveLinksChoices();
  const selection = { kind: "all" };

  const controls = element("div", "upload-controls");
  const kind = motiveLinksSelect(
    MotiveLinksModel.SCOPE_KINDS.map((entry) => ({ value: entry.kind, label: entry.title })),
  );
  const pickers = element("span");
  const run = element("button", "", "Run");
  run.type = "button";
  controls.append(kind, pickers, run);

  const progressEl = motiveLinksStatus("");
  const skippedEl = element("div", "piece-list-status");
  const resultEl = element("div");
  const runsEl = element("div");
  content.append(controls, progressEl, skippedEl, resultEl, element("h2", "section-title", "Earlier runs"), runsEl);
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
      runsEl.appendChild(element("div", "piece-list-status", "No earlier runs."));
      return;
    }
    const list = element("ul", "materials-list");
    for (const row of rows) {
      const li = element("li");
      const open = element("a", "clickable", row.label);
      open.addEventListener("click", () => openRun(row.id));
      li.append(open, document.createTextNode(row.progress ? ` (${row.progress}) ` : " "));
      const del = element("button", "", "Delete");
      del.type = "button";
      del.disabled = row.status === "running";
      del.addEventListener("click", () => deleteRun(row.id));
      li.appendChild(del);
      list.appendChild(li);
    }
    runsEl.appendChild(list);
  }
  loadRuns();
  reloadMotiveLinkRuns = loadRuns;

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
    /* The run is a job on the backend queue (R-645): a second run waits behind the first. runJob polls the job and answers {"ok", "run"} when it ends. */
    const outcome = await BackendJobs.runJob(
      { method: "POST", url: "/api/links/runs", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) },
      { onProgress: (progress) => { if (progressEl.isConnected) { progressEl.textContent = BackendJobs.progressText(progress); } } },
    );
    if (!progressEl.isConnected) {
      return;
    }
    const finished = outcome.body && outcome.body.run;
    if (outcome.status < 200 || outcome.status >= 300 || !finished) {
      progressEl.textContent = (outcome.body && (outcome.body.message || outcome.body.error)) || "Could not run the comparison";
      progressEl.classList.add("error");
      run.disabled = false;
      loadRuns();
      return;
    }
    showRun(finished);
    run.disabled = false;
    if (finished.status === "done") {
      renderMotiveLinksResult(resultEl, finished.result);
    }
    loadRuns();
  });
}
