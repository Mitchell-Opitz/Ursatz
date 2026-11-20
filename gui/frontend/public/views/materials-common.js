import Vocabulary from "../core/vocabulary.js";
import { el as element } from "../core/dom.js";
import ApiRequest from "../core/request.js";
import MaterialsModel from "../models/materials-model.js";
import BackendJobs from "../core/backend-jobs.js";
import ExportControl from "./export-control.js";
import { shell } from "./shell-context.js";

/* What the materials views share (P12): their page state, the request that carries a base version, the job bar, the field validation, the export
 * control and the uses, used-by and versions lists. */
/* Materials views (P12): the project picker and the Motifs, Themes and
 * Pieces tools. Renders from MaterialsModel; the page globals (getContentEl,
 * fetchJsonResult, TOOLS) come from app.js. */
const MATERIALS_TOOL_IDS = { motif: "motifs", theme: "themes", piece: "pieces" };

export const materialsState = {
  projects: [],
  filter: MaterialsModel.ALL_PROJECTS,
  type: null,
  loaded: false,
  notice: "",
};

/* The bar over the page while a job runs: its stage ("melody 6/9") and a Cancel button. Returns {update, remove, signal}. */
function showJobProgress() {
  const controller = new AbortController();
  const bar = document.createElement("div");
  bar.className = "job-progress";
  const label = document.createElement("span");
  label.textContent = BackendJobs.progressText({ state: "queued" });
  const cancel = document.createElement("button");
  cancel.type = "button";
  cancel.textContent = "Cancel";
  cancel.addEventListener("click", () => {
    cancel.disabled = true;
    label.textContent = "cancelling";
    controller.abort();
  });
  bar.append(label, cancel);
  document.body.appendChild(bar);
  return {
    signal: controller.signal,
    update: (progress) => {
      if (!controller.signal.aborted) {
        label.textContent = BackendJobs.progressText(progress);
      }
    },
    remove: () => bar.remove(),
  };
}

/* A job request (R-219): run on the backend queue, answered as the route answered before. A cancelled job answers
   {ok: false, status: 499, cancelled: true}, and the page is as it was before the request. */
async function sendJobRequest(request) {
  const progress = showJobProgress();
  try {
    const result = await BackendJobs.runJob(request, { signal: progress.signal, onProgress: progress.update });
    const data = result.body;
    return {
      ok: result.status >= 200 && result.status < 300 && !(data && data.error),
      status: result.status,
      data,
      cancelled: Boolean(result.cancelled),
    };
  } finally {
    progress.remove();
  }
}

/* Sends a materials request: a command carries the version the page holds as its base_version (core/commands.js), and every answer that holds
 * an item records its current version. */
export async function sendMaterialsRequest(request) {
  const answer = request.job
    ? await sendJobRequest(request)
    : await ApiRequest.sendRequest(request);
  if (answer && answer.ok) {
    ApiRequest.rememberVersions(answer.data);
  }
  return answer;
}

/* Checks a form with the Composer while it is edited (R-276). options: {roots (elements whose input, change and click
 * events change the form), build() (the validate request of the model), field(path) (the model's fieldForPath),
 * fields() (the elements by field name)}. Returns {element (the Composer's message, shown as given), check(), now(),
 * refuse(data) (marks the field a failed job's path names)}.
 * The refused field is marked with the class field-invalid; an incomplete form shows nothing. */
export function attachFormValidation(options) {
  const message = document.createElement("div");
  message.className = "materials-field-error";
  let marked = [];
  function show(result) {
    if (options.onResult) {
      options.onResult(result);
    }
    for (const el of marked) {
      el.classList.remove("field-invalid");
    }
    marked = [];
    message.textContent = !result.ok && result.complete ? result.message : "";
    const target = result.field ? options.fields()[result.field] : null;
    if (target) {
      const holder = target.closest("label") || target;
      holder.classList.add("field-invalid");
      marked.push(holder);
    }
  }
  const validator = Vocabulary.createValidator({ build: options.build, field: options.field, show });
  for (const root of options.roots) {
    for (const type of ["input", "change", "click"]) {
      root.addEventListener(type, () => validator.check());
    }
  }
  validator.check();
  /* A job's failure answer {error, path} marks the field as the validate route does; no path marks nothing. */
  function refuse(data) {
    const path = data && typeof data.path === "string" ? data.path : "";
    if (path === "") {
      return;
    }
    show({ ok: false, complete: true, message: String(data.error || ""), path, field: options.field(path) });
  }
  return { element: message, check: validator.check, now: validator.now, refuse };
}

/* The Export control of one item version (P31 B5): the Analyzer's text of a motif, theme or piece, to Copy or Download.
 * version is the stored version number; the file is named after the item. */
export function buildMaterialExport(id, version, name) {
  const wrap = element("div", "materials-export");
  wrap.appendChild(
    ExportControl.create({
      fetchText: () => shell().fetchExportText(`/api/materials/items/${encodeURIComponent(id)}/export?version=${Number(version)}`),
      filename: () => name,
    }),
  );
  return wrap;
}

/* The Uses section: the uses of this version, remove, and an Add use picker
 * over the items of every project. Adding or removing writes a new version.
 * ctx: {id, type, body, statuses, allItems, statusEl}. */
export function buildUsesSection(ctx) {
  const rows = MaterialsModel.useRows(ctx.statuses, materialsState.projects);
  const derived = ctx.type === "motif";
  const section = element("div");
  section.appendChild(element("h2", "section-title", derived ? "Derived from" : "Uses"));
  const compareEl = element("div");

  async function change(sourceId, version, add, note) {
    const next = MaterialsModel.bodyWithUse(ctx.body, sourceId, version, add);
    const saved = await sendMaterialsRequest(MaterialsModel.buildAddVersionRequest(ctx.id, next, note));
    if (!saved.ok) {
      ctx.statusEl.textContent = MaterialsModel.errorText(saved.data, "Could not change the uses");
      ctx.statusEl.classList.add("error");
      return;
    }
    shell().openItem(ctx.type, ctx.id, saved.data.item.analysis_error);
  }

  if (rows.length === 0) {
    section.appendChild(element("div", "piece-list-status", derived ? "Not derived from anything yet." : "Uses nothing yet."));
  } else {
    const table = element("table", "library-table");
    const head = element("tr");
    for (const label of ["Name", "Type", "Project", "Used version", "Source", ""]) {
      head.appendChild(element("th", "", label));
    }
    table.appendChild(head);
    for (const row of rows) {
      const tr = element("tr");
      tr.appendChild(element("td", row.missing ? "error" : "", row.name));
      tr.appendChild(element("td", "", row.type));
      tr.appendChild(element("td", "", row.project));
      tr.appendChild(element("td", "", `v${row.version}`));
      tr.appendChild(element("td", row.missing ? "error" : "", row.note));
      const actions = element("td");
      if (row.stale) {
        const update = element("button", "", "Update");
        update.type = "button";
        update.addEventListener("click", async () => {
          const answer = await sendMaterialsRequest(MaterialsModel.buildUpdateUseRequest(ctx.id, row.item_id));
          if (!answer.ok) {
            ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not update the use");
            ctx.statusEl.classList.add("error");
            return;
          }
          shell().openItem(ctx.type, ctx.id, answer.data.item.analysis_error);
        });
        const compare = element("button", "", "Compare");
        compare.type = "button";
        compare.addEventListener("click", () => showUseComparison(compareEl, row, ctx.statusEl));
        actions.append(update, compare);
      }
      const remove = element("button", "button-secondary", "Remove");
      remove.type = "button";
      remove.addEventListener("click", () => change(row.item_id, row.version, false, `Removed use of ${row.name}`));
      actions.appendChild(remove);
      tr.appendChild(actions);
      table.appendChild(tr);
    }
    section.appendChild(table);
  }

  const picker = element("div", "upload-controls");
  const query = element("input");
  query.type = "text";
  query.placeholder = "Search items by name";
  const typeSelect = element("select");
  for (const entry of [{ type: "all", label: "All types" }, ...MaterialsModel.TYPES]) {
    const option = element("option", "", entry.label);
    option.value = entry.type;
    typeSelect.appendChild(option);
  }
  const projectSelect = element("select");
  for (const option of MaterialsModel.projectOptions(materialsState.projects)) {
    const o = element("option", "", option.label);
    o.value = option.value;
    projectSelect.appendChild(o);
  }
  const choice = element("select", "materials-use-choice");
  const add = element("button", "", "Add use");
  add.type = "button";

  function fillChoices() {
    const found = MaterialsModel.pickerItems(ctx.allItems, materialsState.projects, {
      query: query.value,
      type: typeSelect.value,
      project: projectSelect.value,
      exclude: [ctx.id, ...rows.map((row) => row.item_id)],
    });
    choice.innerHTML = "";
    for (const item of found) {
      const option = element("option", "", item.label);
      option.value = item.id;
      option.dataset.version = String(item.version);
      option.dataset.name = item.name;
      choice.appendChild(option);
    }
    add.disabled = found.length === 0;
  }
  for (const control of [query, typeSelect, projectSelect]) {
    control.addEventListener("input", fillChoices);
  }
  add.addEventListener("click", () => {
    const selected = choice.selectedOptions[0];
    if (selected) {
      change(selected.value, selected.dataset.version, true, `Added use of ${selected.dataset.name}`);
    }
  });
  fillChoices();
  picker.append(query, typeSelect, projectSelect, choice, add);
  section.appendChild(picker);
  section.appendChild(compareEl);
  return section;
}

/* Adds an "uses updated" badge to the rows of items with a newer source. */
export async function markUpdatedUses(table, rows, type) {
  const answers = await Promise.all(
    rows.map((row) => sendMaterialsRequest(MaterialsModel.buildItemGetRequest(row.id))),
  );
  if (!materialsViewActive() || materialsState.type !== type || !table.isConnected) {
    return;
  }
  rows.forEach((row, index) => {
    const answer = answers[index];
    if (answer.ok && MaterialsModel.hasUpdatedUse(answer.data.uses_status)) {
      const cell = table.querySelector(`tr[data-id="${row.id}"] td`);
      if (cell) {
        cell.appendChild(element("span", "materials-badge", "uses updated"));
      }
    }
  });
}

/* Shows the version a use holds and the source's current version side by
 * side, read only. */
async function showUseComparison(el, row, statusEl) {
  const [used, current] = await Promise.all([
    sendMaterialsRequest(MaterialsModel.buildVersionGetRequest(row.item_id, row.version)),
    sendMaterialsRequest(MaterialsModel.buildVersionGetRequest(row.item_id, row.current)),
  ]);
  if (!used.ok || !current.ok) {
    statusEl.textContent = MaterialsModel.errorText((used.ok ? current : used).data, "Could not load the versions");
    statusEl.classList.add("error");
    return;
  }
  el.innerHTML = "";
  el.appendChild(element("h3", "", `${row.name}: v${row.version} (used) and v${row.current} (current), read only`));
  const grid = element("div", "materials-compare");
  for (const [title, answer] of [[`v${row.version} (used)`, used], [`v${row.current} (current)`, current]]) {
    const column = element("div");
    column.appendChild(element("h4", "", title));
    column.appendChild(element("pre", "materials-version-body", JSON.stringify(answer.data.body, null, 2)));
    grid.appendChild(column);
  }
  el.appendChild(grid);
}

/* The Used by table of a motif (noun names another type, as "theme"): name (opens the item), type, the version it uses, project, and whether its use is
 * behind currentVersion. */
export function buildUsedByTable(users, currentVersion, noun) {
  const rows = MaterialsModel.usedByTable(users, materialsState.projects, currentVersion);
  const section = element("div");
  section.appendChild(element("h2", "section-title", "Used by"));
  if (rows.length === 0) {
    section.appendChild(element("div", "piece-list-status", `Nothing uses this ${noun || "motif"} yet.`));
    return section;
  }
  const table = element("table", "library-table");
  const head = element("tr");
  for (const label of ["Name", "Type", "Version used", "Project", ""]) {
    head.appendChild(element("th", "", label));
  }
  table.appendChild(head);
  for (const row of rows) {
    const tr = element("tr");
    const nameCell = element("td");
    const open = element("a", "clickable", row.name);
    open.addEventListener("click", () => openLinkedItem(row.type, row.id));
    nameCell.appendChild(open);
    tr.appendChild(nameCell);
    tr.appendChild(element("td", "", row.type));
    tr.appendChild(element("td", "", `v${row.version}`));
    tr.appendChild(element("td", "", row.project));
    tr.appendChild(element("td", "", row.updateAvailable ? "update available" : ""));
    table.appendChild(tr);
  }
  section.appendChild(table);
  return section;
}

export function buildVersionsList(item, viewEl, statusEl, renderBody) {
  const section = element("div");
  section.appendChild(element("h2", "section-title", "Versions"));
  const table = element("table", "library-table");
  const head = element("tr");
  for (const label of ["Version", "Date", "Note", ""]) {
    head.appendChild(element("th", "", label));
  }
  table.appendChild(head);
  for (const row of MaterialsModel.versionRows(item)) {
    const tr = element("tr");
    tr.appendChild(element("td", "", row.current ? `v${row.number} (current)` : `v${row.number}`));
    tr.appendChild(element("td", "", row.date));
    tr.appendChild(element("td", "", row.note));
    const cell = element("td");
    const view = element("button", "", "View");
    view.type = "button";
    view.addEventListener("click", async () => {
      const answer = await sendMaterialsRequest(MaterialsModel.buildVersionGetRequest(item.id, row.number));
      if (!answer.ok) {
        statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not load the version");
        statusEl.classList.add("error");
        return;
      }
      statusEl.textContent = "";
      statusEl.classList.remove("error");
      if (renderBody) {
        renderBody(row.number, answer.data.body);
        return;
      }
      viewEl.innerHTML = "";
      viewEl.appendChild(element("h2", "section-title", `Version ${row.number} (read only)`));
      viewEl.appendChild(element("pre", "materials-version-body", JSON.stringify(answer.data.body, null, 2)));
    });
    cell.appendChild(view);
    tr.appendChild(cell);
    table.appendChild(tr);
  }
  section.appendChild(table);
  return section;
}

export function materialsViewActive() {
  const active = shell().activeToolId();
  return Boolean(active && materialsState.type && active === MATERIALS_TOOL_IDS[materialsState.type]);
}

/* Opens a linked item in its own tool. */
export function openLinkedItem(type, id) {
  const toolId = MATERIALS_TOOL_IDS[type];
  if (!toolId) {
    return;
  }
  shell().leaveWorkspace();
  shell().setActiveTool(toolId);
  materialsState.type = type;
  shell().openItem(type, id);
}
