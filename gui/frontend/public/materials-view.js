/* Materials views (P12): the project picker and the Motifs, Themes and
 * Pieces tools. Renders from MaterialsModel; the page globals (getContentEl,
 * fetchJsonResult, TOOLS) come from app.js. */
const MATERIALS_TOOL_IDS = { motif: "motifs", theme: "themes", piece: "pieces" };

const materialsState = {
  projects: [],
  filter: MaterialsModel.ALL_PROJECTS,
  type: null,
  loaded: false,
};

function sendMaterialsRequest(request) {
  return fetchJsonResult(request.url, {
    method: request.method,
    headers: request.headers,
    body: request.body,
  });
}

function materialsViewActive() {
  const active = TOOLS.find((tool) => tool.active);
  return Boolean(active && materialsState.type && active.id === MATERIALS_TOOL_IDS[materialsState.type]);
}

/* Redraws the open Motifs, Themes or Pieces view, if one is open. */
function refreshMaterialsView() {
  if (materialsViewActive()) {
    renderMaterialsView(materialsState.type);
  }
}

async function loadMaterialsProjects() {
  const answer = await sendMaterialsRequest(MaterialsModel.buildProjectListRequest());
  materialsState.loaded = answer.ok;
  materialsState.projects = answer.ok ? MaterialsModel.projectsFrom(answer.data) : [];
  materialsState.filter = MaterialsModel.resolveFilter(materialsState.filter, materialsState.projects);
  MaterialsModel.writeFilter(materialsState.filter);
  return answer;
}

function setMaterialsStatus(text, isError) {
  const el = document.getElementById("materials-projects-status");
  if (el) {
    el.textContent = text || "";
    el.classList.toggle("error", Boolean(isError));
  }
}

function renderProjectPicker() {
  const select = document.getElementById("materials-project-select");
  if (!select) {
    return;
  }
  select.innerHTML = "";
  for (const option of MaterialsModel.projectOptions(materialsState.projects)) {
    const el = document.createElement("option");
    el.value = option.value;
    el.textContent = option.label;
    select.appendChild(el);
  }
  select.value = materialsState.filter;
  const specific = materialsState.filter !== MaterialsModel.ALL_PROJECTS;
  document.getElementById("materials-project-rename").disabled = !specific;
  document.getElementById("materials-project-delete").disabled = !specific;
}

async function reloadProjectsAndRedraw(filterAfter) {
  if (filterAfter) {
    materialsState.filter = filterAfter;
  }
  const answer = await loadMaterialsProjects();
  renderProjectPicker();
  setMaterialsStatus(answer.ok ? "" : MaterialsModel.errorText(answer.data, "Could not load projects"), !answer.ok);
  refreshMaterialsView();
}

async function handleCreateProject() {
  const name = window.prompt("New project name");
  if (name === null || name.trim() === "") {
    return;
  }
  const answer = await sendMaterialsRequest(MaterialsModel.buildCreateProjectRequest(name));
  if (!answer.ok) {
    setMaterialsStatus(MaterialsModel.errorText(answer.data, "Could not create the project"), true);
    return;
  }
  await reloadProjectsAndRedraw(answer.data.project.id);
}

async function handleRenameProject() {
  const id = materialsState.filter;
  const current = MaterialsModel.projectName(materialsState.projects, id);
  const name = window.prompt("Rename project", current);
  if (name === null || name.trim() === "" || name.trim() === current) {
    return;
  }
  const answer = await sendMaterialsRequest(MaterialsModel.buildRenameProjectRequest(id, name));
  if (!answer.ok) {
    setMaterialsStatus(MaterialsModel.errorText(answer.data, "Could not rename the project"), true);
    return;
  }
  await reloadProjectsAndRedraw();
}

async function handleDeleteProject() {
  const id = materialsState.filter;
  const name = MaterialsModel.projectName(materialsState.projects, id);
  if (!window.confirm(`Delete project "${name}"?`)) {
    return;
  }
  const answer = await sendMaterialsRequest(MaterialsModel.buildDeleteProjectRequest(id));
  if (!answer.ok) {
    setMaterialsStatus(MaterialsModel.errorText(answer.data, "Could not delete the project"), true);
    return;
  }
  await reloadProjectsAndRedraw(MaterialsModel.ALL_PROJECTS);
}

/* Fills the sidebar project picker (index.html #materials-projects). */
async function mountProjectPicker() {
  materialsState.filter = MaterialsModel.readFilter();
  document.getElementById("materials-project-select").addEventListener("change", (event) => {
    materialsState.filter = event.target.value;
    MaterialsModel.writeFilter(materialsState.filter);
    renderProjectPicker();
    refreshMaterialsView();
  });
  document.getElementById("materials-project-new").addEventListener("click", handleCreateProject);
  document.getElementById("materials-project-rename").addEventListener("click", handleRenameProject);
  document.getElementById("materials-project-delete").addEventListener("click", handleDeleteProject);
  await reloadProjectsAndRedraw();
}

function materialsElement(tag, className, text) {
  const el = document.createElement(tag);
  if (className) {
    el.className = className;
  }
  if (text !== undefined) {
    el.textContent = text;
  }
  return el;
}

function fillProjectSelect(select, selected) {
  select.innerHTML = "";
  for (const project of materialsState.projects) {
    const option = materialsElement("option", "", project.name);
    option.value = project.id;
    select.appendChild(option);
  }
  select.value = selected;
}

function buildNewItemControls(type, info, statusEl, reload) {
  const controls = materialsElement("div", "upload-controls");
  const name = materialsElement("input");
  name.type = "text";
  name.placeholder = `New ${info.singular} name`;
  const project = materialsElement("select");
  fillProjectSelect(project, MaterialsModel.defaultProject(materialsState.filter, materialsState.projects));
  const create = materialsElement("button", "", "New");
  create.type = "button";
  create.disabled = materialsState.projects.length === 0;
  async function submit() {
    if (type === "motif") {
      renderNewMotif(name.value, project.value);
      return;
    }
    if (name.value.trim() === "") {
      return;
    }
    const answer = await sendMaterialsRequest(MaterialsModel.buildCreateItemRequest(type, name.value, project.value));
    if (!answer.ok) {
      statusEl.textContent = MaterialsModel.errorText(answer.data, `Could not create the ${info.singular}`);
      statusEl.classList.add("error");
      return;
    }
    await reload();
  }
  create.addEventListener("click", submit);
  name.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      submit();
    }
  });
  controls.append(name, project, create);
  return controls;
}

function buildItemTable(rows, onOpen) {
  const table = materialsElement("table", "library-table");
  const head = materialsElement("tr");
  for (const label of ["Name", "Project", "Version", "Updated"]) {
    head.appendChild(materialsElement("th", "", label));
  }
  table.appendChild(head);
  for (const row of rows) {
    const tr = materialsElement("tr", "clickable");
    tr.dataset.id = row.id;
    for (const value of [row.name, row.project, row.version, row.updated]) {
      tr.appendChild(materialsElement("td", "", value));
    }
    tr.addEventListener("click", () => onOpen(row.id));
    table.appendChild(tr);
  }
  return table;
}

/* The Random dialog of the Motifs list: draws candidates from the Composer, plays them through the score workspace and
 * saves the ones kept as motif items. Nothing is stored until Save. */
function buildRandomMotifs(statusEl, reload) {
  const section = materialsElement("section", "panel materials-random");
  const toggle = materialsElement("button", "", "Random");
  toggle.type = "button";
  const form = materialsElement("div", "upload-controls");
  form.hidden = true;
  const problemEl = materialsElement("div", "upload-status error");
  const candidatesEl = materialsElement("ol", "variant-candidates");
  const selectedEl = materialsElement("div");
  let workspace = null;
  let blobUrls = [];
  let shown = [];
  let runValues = null;

  function field(labelText, control) {
    const label = materialsElement("label", "filter-field", `${labelText} `);
    label.appendChild(control);
    form.appendChild(label);
    return control;
  }
  function select(options) {
    const el = materialsElement("select");
    for (const [value, text] of options) {
      const o = materialsElement("option", "", text);
      o.value = value;
      el.appendChild(o);
    }
    return el;
  }
  function input(placeholder, value) {
    const el = materialsElement("input");
    el.type = "text";
    el.placeholder = placeholder;
    el.value = value || "";
    return el;
  }
  const mode = field("Mode", select([["major", "Major"], ["minor", "Minor"]]));
  const meter = field("Meter", input("4/4", "4/4"));
  const formSelect = field("Form", select(MaterialsModel.MOTIF_FORMS.map((f) => [f.value, f.label])));
  const style = field("Style", select([["", "No style"]]));
  const count = field("Count", input("6", "6"));
  const seed = field("Seed", input("random"));
  const role = field("Role", select(MaterialsModel.MOTIF_ROLES.map((r) => [r.value, r.label])));
  const generate = materialsElement("button", "", "Generate");
  generate.type = "button";
  form.appendChild(generate);

  async function loadStyles() {
    const answer = await sendMaterialsRequest({ method: "GET", url: "/api/styles" });
    if (answer.ok && answer.data && Array.isArray(answer.data.styles)) {
      for (const item of answer.data.styles) {
        const o = materialsElement("option", "", item.name || item.id);
        o.value = item.id;
        style.appendChild(o);
      }
    }
  }

  function values() {
    return {
      mode: mode.value,
      meter: meter.value,
      form: formSelect.value,
      style_id: style.value,
      count: count.value.trim() === "" ? 6 : count.value.trim(),
      seed: seed.value,
      role: role.value,
    };
  }

  function clearSelection() {
    if (workspace) {
      workspace.destroy();
      workspace = null;
    }
    for (const url of blobUrls) {
      URL.revokeObjectURL(url);
    }
    blobUrls = [];
    selectedEl.innerHTML = "";
  }

  async function play(candidate) {
    const body = VariantTree.candidateBody(MaterialsModel.randomCandidateToBody(candidate, runValues), candidate);
    statusEl.classList.remove("error");
    statusEl.textContent = "Rendering...";
    const answer = await sendMaterialsRequest(MaterialsModel.renderRequest(body));
    if (!answer.ok) {
      statusEl.classList.add("error");
      statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not render the motif");
      return;
    }
    statusEl.textContent = "";
    clearSelection();
    const xml = URL.createObjectURL(new Blob([answer.data.musicxml], { type: "application/xml" }));
    const playback = URL.createObjectURL(new Blob([JSON.stringify(answer.data.playback)], { type: "application/json" }));
    blobUrls = [xml, playback];
    workspace = ScoreWorkspace.mount(selectedEl, {
      title: "Random motif",
      subtitle: `candidate · seed ${candidate.seed}`,
      musicxmlUrl: xml,
      playbackUrl: playback,
    });
  }

  async function reroll(candidate) {
    const request = { ...runValues, count: 1, seed: MaterialsModel.rerollSeed(candidate.seed) };
    const answer = await sendMaterialsRequest(MaterialsModel.randomRequest(request));
    const next = answer.ok && answer.data && answer.data.motives ? answer.data.motives[0] : null;
    if (!next) {
      statusEl.classList.add("error");
      statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not draw another motif");
      return;
    }
    next.seed = request.seed;
    shown = shown.map((other) => (other === candidate ? next : other));
    drawCandidates();
  }

  function saveControls(candidate) {
    const row = materialsElement("div", "variant-actions");
    const name = materialsElement("input");
    name.type = "text";
    name.placeholder = "Motif name";
    const project = materialsElement("select");
    fillProjectSelect(project, MaterialsModel.defaultProject(materialsState.filter, materialsState.projects));
    const save = materialsElement("button", "", "Save");
    save.type = "button";
    save.addEventListener("click", async () => {
      if (name.value.trim() === "" || project.value === "") {
        statusEl.classList.add("error");
        statusEl.textContent = "Give the motif a name and a project.";
        return;
      }
      save.disabled = true;
      const answer = await sendMaterialsRequest(
        MaterialsModel.buildSaveMotifRequest({
          name: name.value,
          project_id: project.value,
          body: MaterialsModel.randomCandidateToBody(candidate, runValues),
        }),
      );
      save.disabled = false;
      if (!answer.ok) {
        statusEl.classList.add("error");
        statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not save the motif");
        return;
      }
      await reload();
    });
    row.append(name, project, save);
    return row;
  }

  function drawCandidates() {
    candidatesEl.innerHTML = "";
    for (const candidate of shown) {
      const card = materialsElement("li", "variant-candidate");
      const analysis = (candidate.motive && candidate.motive.analysis) || {};
      const textName = runValues.form === "rhythm" ? "rhythm_text" : runValues.form === "contour" ? "contour_text" : "notes_text";
      const keyText = MaterialsModel.candidateKeyText(candidate);
      card.appendChild(materialsElement("div", "variant-derivation", analysis[textName] || ""));
      card.appendChild(materialsElement("div", "variant-meta", `${runValues.meter}${keyText ? ` · ${keyText}` : ""} · seed ${candidate.seed}`));
      const buttons = materialsElement("div", "variant-actions");
      for (const [text, action] of [["Play", () => play(candidate)], ["Reroll", () => reroll(candidate)]]) {
        const button = materialsElement("button", "", text);
        button.type = "button";
        button.addEventListener("click", action);
        buttons.appendChild(button);
      }
      card.append(buttons, saveControls(candidate));
      candidatesEl.appendChild(card);
    }
  }

  generate.addEventListener("click", async () => {
    const current = values();
    const problems = MaterialsModel.randomProblems(current);
    problemEl.textContent = problems.map((p) => p.message).join(" ");
    if (problems.length > 0) {
      return;
    }
    generate.disabled = true;
    statusEl.classList.remove("error");
    statusEl.textContent = "Drawing motifs...";
    const answer = await sendMaterialsRequest(MaterialsModel.randomRequest(current));
    generate.disabled = false;
    if (!answer.ok) {
      statusEl.classList.add("error");
      statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not draw motifs");
      return;
    }
    statusEl.textContent = "";
    clearSelection();
    runValues = current;
    const start = Number.isInteger(answer.data.seed) ? answer.data.seed : 0;
    shown = (answer.data.motives || []).map((motive, index) => ({ ...motive, seed: start + index }));
    drawCandidates();
  });
  toggle.addEventListener("click", () => {
    form.hidden = !form.hidden;
  });
  loadStyles();
  section.append(toggle, form, problemEl, candidatesEl, selectedEl);
  return section;
}

async function renderMaterialsView(type) {
  const info = MaterialsModel.typeInfo(type);
  materialsState.type = type;
  const content = getContentEl();
  content.innerHTML = "";
  content.appendChild(materialsElement("h1", "view-title", info.label));
  const statusEl = materialsElement("div", "upload-status");
  statusEl.setAttribute("role", "status");
  const listEl = materialsElement("div");
  const reload = () => renderMaterialsView(type);

  if (!materialsState.loaded) {
    await loadMaterialsProjects();
    renderProjectPicker();
  }
  content.appendChild(buildNewItemControls(type, info, statusEl, reload));
  if (materialsState.projects.length === 0) {
    statusEl.textContent = "Create a project from the sidebar first.";
  }
  content.append(statusEl);
  if (type === "motif" && materialsState.projects.length > 0) {
    content.appendChild(buildRandomMotifs(statusEl, reload));
  }
  content.append(listEl);

  const answer = await sendMaterialsRequest(MaterialsModel.buildItemListRequest(type, materialsState.filter));
  if (!materialsViewActive() || materialsState.type !== type) {
    return;
  }
  if (!answer.ok) {
    listEl.appendChild(
      materialsElement("div", "piece-list-status error", MaterialsModel.errorText(answer.data, "Could not load items")),
    );
    return;
  }
  const rows = MaterialsModel.itemRows(answer.data.items, materialsState.projects);
  if (rows.length === 0) {
    listEl.appendChild(materialsElement("div", "piece-list-status", `No ${info.label.toLowerCase()} yet.`));
    return;
  }
  const panel = materialsElement("section", "panel library-panel");
  const table = buildItemTable(rows, (id) => renderMaterialsDetail(type, id));
  panel.appendChild(table);
  listEl.appendChild(panel);
  markUpdatedUses(table, rows, type);
}

/* Adds an "uses updated" badge to the rows of items with a newer source. */
async function markUpdatedUses(table, rows, type) {
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
        cell.appendChild(materialsElement("span", "materials-badge", "uses updated"));
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
  el.appendChild(materialsElement("h3", "", `${row.name}: v${row.version} (used) and v${row.current} (current), read only`));
  const grid = materialsElement("div", "materials-compare");
  for (const [title, answer] of [[`v${row.version} (used)`, used], [`v${row.current} (current)`, current]]) {
    const column = materialsElement("div");
    column.appendChild(materialsElement("h4", "", title));
    column.appendChild(materialsElement("pre", "materials-version-body", JSON.stringify(answer.data.body, null, 2)));
    grid.appendChild(column);
  }
  el.appendChild(grid);
}

/* The Uses section: the uses of this version, remove, and an Add use picker
 * over the items of every project. Adding or removing writes a new version.
 * ctx: {id, type, body, statuses, allItems, statusEl}. */
function buildUsesSection(ctx) {
  const rows = MaterialsModel.useRows(ctx.statuses, materialsState.projects);
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Uses"));
  const compareEl = materialsElement("div");

  async function change(sourceId, version, add, note) {
    const next = MaterialsModel.bodyWithUse(ctx.body, sourceId, version, add);
    const saved = await sendMaterialsRequest(MaterialsModel.buildAddVersionRequest(ctx.id, next, note));
    if (!saved.ok) {
      ctx.statusEl.textContent = MaterialsModel.errorText(saved.data, "Could not change the uses");
      ctx.statusEl.classList.add("error");
      return;
    }
    renderMaterialsDetail(ctx.type, ctx.id, saved.data.item.analysis_error);
  }

  if (rows.length === 0) {
    section.appendChild(materialsElement("div", "piece-list-status", "Uses nothing yet."));
  } else {
    const table = materialsElement("table", "library-table");
    const head = materialsElement("tr");
    for (const label of ["Name", "Type", "Project", "Used version", "Source", ""]) {
      head.appendChild(materialsElement("th", "", label));
    }
    table.appendChild(head);
    for (const row of rows) {
      const tr = materialsElement("tr");
      tr.appendChild(materialsElement("td", row.missing ? "error" : "", row.name));
      tr.appendChild(materialsElement("td", "", row.type));
      tr.appendChild(materialsElement("td", "", row.project));
      tr.appendChild(materialsElement("td", "", `v${row.version}`));
      tr.appendChild(materialsElement("td", row.missing ? "error" : "", row.note));
      const actions = materialsElement("td");
      if (row.stale) {
        const update = materialsElement("button", "", "Update");
        update.type = "button";
        update.addEventListener("click", async () => {
          const answer = await sendMaterialsRequest(MaterialsModel.buildUpdateUseRequest(ctx.id, row.item_id));
          if (!answer.ok) {
            ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not update the use");
            ctx.statusEl.classList.add("error");
            return;
          }
          renderMaterialsDetail(ctx.type, ctx.id, answer.data.item.analysis_error);
        });
        const compare = materialsElement("button", "", "Compare");
        compare.type = "button";
        compare.addEventListener("click", () => showUseComparison(compareEl, row, ctx.statusEl));
        actions.append(update, compare);
      }
      const remove = materialsElement("button", "", "Remove");
      remove.type = "button";
      remove.addEventListener("click", () => change(row.item_id, row.version, false, `Removed use of ${row.name}`));
      actions.appendChild(remove);
      tr.appendChild(actions);
      table.appendChild(tr);
    }
    section.appendChild(table);
  }

  const picker = materialsElement("div", "upload-controls");
  const query = materialsElement("input");
  query.type = "text";
  query.placeholder = "Search items by name";
  const typeSelect = materialsElement("select");
  for (const entry of [{ type: "all", label: "All types" }, ...MaterialsModel.TYPES]) {
    const option = materialsElement("option", "", entry.label);
    option.value = entry.type;
    typeSelect.appendChild(option);
  }
  const projectSelect = materialsElement("select");
  for (const option of MaterialsModel.projectOptions(materialsState.projects)) {
    const o = materialsElement("option", "", option.label);
    o.value = option.value;
    projectSelect.appendChild(o);
  }
  const choice = materialsElement("select", "materials-use-choice");
  const add = materialsElement("button", "", "Add use");
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
      const option = materialsElement("option", "", item.label);
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

/* The Used by section: the items that use this one; each opens its detail. */
function buildUsedBySection(users) {
  const rows = MaterialsModel.usedByRows(users, materialsState.projects);
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Used by"));
  if (rows.length === 0) {
    section.appendChild(materialsElement("div", "piece-list-status", "Not used by any item."));
    return section;
  }
  const list = materialsElement("ul", "materials-list");
  for (const row of rows) {
    const li = materialsElement("li");
    const open = materialsElement("a", "clickable", row.label);
    open.addEventListener("click", () => openLinkedItem(row.type, row.id));
    li.appendChild(open);
    li.appendChild(document.createTextNode(row.project ? ` · ${row.project}` : ""));
    list.appendChild(li);
  }
  section.appendChild(list);
  return section;
}

function buildVersionsList(item, viewEl, statusEl, renderBody) {
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Versions"));
  const table = materialsElement("table", "library-table");
  const head = materialsElement("tr");
  for (const label of ["Version", "Date", "Note", ""]) {
    head.appendChild(materialsElement("th", "", label));
  }
  table.appendChild(head);
  for (const row of MaterialsModel.versionRows(item)) {
    const tr = materialsElement("tr");
    tr.appendChild(materialsElement("td", "", row.current ? `v${row.number} (current)` : `v${row.number}`));
    tr.appendChild(materialsElement("td", "", row.date));
    tr.appendChild(materialsElement("td", "", row.note));
    const cell = materialsElement("td");
    const view = materialsElement("button", "", "View");
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
      viewEl.appendChild(materialsElement("h2", "section-title", `Version ${row.number} (read only)`));
      viewEl.appendChild(materialsElement("pre", "materials-version-body", JSON.stringify(answer.data.body, null, 2)));
    });
    cell.appendChild(view);
    tr.appendChild(cell);
    table.appendChild(tr);
  }
  section.appendChild(table);
  return section;
}

/* The motif form. Returns {el, values(), showProblems(problems), showError(text)}.
 * body is the version body being edited, or null for a new motif. */
function buildMotifEditor(body) {
  const initial = MaterialsModel.motifFormValues(body);
  const el = materialsElement("section", "panel materials-editor");
  const errors = {};

  function field(labelText, control, key) {
    const label = materialsElement("label", "filter-field", labelText + " ");
    label.appendChild(control);
    el.appendChild(label);
    errors[key] = materialsElement("div", "materials-field-error");
    el.appendChild(errors[key]);
    return control;
  }

  const role = materialsElement("select");
  for (const option of MaterialsModel.MOTIF_ROLES) {
    const o = materialsElement("option", "", option.label);
    o.value = option.value;
    role.appendChild(o);
  }
  role.value = initial.role;
  field("Role", role, "role");

  const form = materialsElement("select");
  for (const option of MaterialsModel.MOTIF_FORMS) {
    const o = materialsElement("option", "", option.label);
    o.value = option.value;
    o.disabled = !option.enabled;
    form.appendChild(o);
  }
  form.value = initial.form;
  field("Form", form, "form");

  const notes = materialsElement("textarea", "materials-notes");
  notes.rows = 4;
  notes.spellcheck = false;
  notes.value = initial.notes;
  field("Notes", notes, "notes");
  const hint = materialsElement("div", "materials-hint", MaterialsModel.formHint(initial.form));
  el.appendChild(hint);
  form.addEventListener("change", () => {
    hint.textContent = MaterialsModel.formHint(form.value);
  });
  el.appendChild(materialsElement("div", "materials-hint", MaterialsModel.TONIC_HINT));

  const importRow = materialsElement("div", "upload-controls");
  const importButton = materialsElement("button", "", "Upload MIDI");
  importButton.type = "button";
  const importFile = materialsElement("input");
  importFile.type = "file";
  importFile.accept = ".mid,.midi";
  importFile.hidden = true;
  const importStatus = materialsElement("span", "upload-status");
  importStatus.setAttribute("role", "status");
  importRow.append(importButton, importFile, importStatus);
  el.appendChild(importRow);
  importButton.addEventListener("click", () => importFile.click());
  importFile.addEventListener("change", async () => {
    const file = importFile.files && importFile.files[0];
    importFile.value = "";
    if (!file) {
      return;
    }
    importStatus.classList.remove("error");
    importStatus.textContent = "Reading the file...";
    const answer = await sendMaterialsRequest(MaterialsModel.buildImportMidiRequest(file));
    if (!answer.ok) {
      importStatus.classList.add("error");
      importStatus.textContent = MaterialsModel.errorText(answer.data, "Could not read the MIDI file");
      return;
    }
    const filled = MaterialsModel.importToFormValues(answer.data);
    form.value = filled.form;
    hint.textContent = MaterialsModel.formHint(filled.form);
    notes.value = filled.notes;
    meter.value = filled.meter;
    mode.value = filled.mode;
    pickup.value = filled.pickup;
    importStatus.textContent = MaterialsModel.importNotice(answer.data);
  });

  let legacyKey = initial.legacyKey;
  const legacyRow = materialsElement("div", "upload-controls");
  const legacyText = materialsElement("span", "materials-hint");
  const rewriteButton = materialsElement("button", "", "Rewrite in C");
  rewriteButton.type = "button";
  const rewriteStatus = materialsElement("span", "upload-status");
  rewriteStatus.setAttribute("role", "status");
  legacyRow.append(legacyText, rewriteButton, rewriteStatus);
  el.appendChild(legacyRow);
  function showLegacy() {
    legacyRow.hidden = !legacyKey;
    legacyText.textContent = legacyKey ? MaterialsModel.legacyNotice(legacyKey) : "";
  }
  showLegacy();
  rewriteButton.addEventListener("click", async () => {
    rewriteStatus.classList.remove("error");
    rewriteStatus.textContent = "";
    if (form.value !== "full") {
      legacyKey = null;
      showLegacy();
      return;
    }
    const request = MaterialsModel.toCRequest(body, mode.value);
    if (!request) {
      rewriteStatus.classList.add("error");
      rewriteStatus.textContent = "This version has no degrees to rewrite. Re-analyze it first.";
      return;
    }
    rewriteButton.disabled = true;
    rewriteStatus.textContent = "Rewriting...";
    const answer = await sendMaterialsRequest(request);
    rewriteButton.disabled = false;
    if (!answer.ok) {
      rewriteStatus.classList.add("error");
      rewriteStatus.textContent = MaterialsModel.errorText(answer.data, "Could not rewrite the motif in C");
      return;
    }
    notes.value = answer.data.notes_text;
    legacyKey = null;
    showLegacy();
  });

  const meter = materialsElement("input");
  meter.type = "text";
  meter.value = initial.meter;
  meter.size = 6;
  field("Meter", meter, "meter");

  const mode = materialsElement("select");
  for (const option of MaterialsModel.MOTIF_MODES) {
    const o = materialsElement("option", "", option);
    o.value = option;
    mode.appendChild(o);
  }
  mode.value = initial.mode;
  field("Mode", mode, "mode");

  const pickup = materialsElement("input");
  pickup.type = "text";
  pickup.value = initial.pickup;
  pickup.size = 6;
  field("Pickup (beats)", pickup, "pickup");

  const harmony = materialsElement("input");
  harmony.type = "text";
  harmony.value = initial.harmony;
  harmony.size = 30;
  harmony.maxLength = 200;
  harmony.placeholder = "I V6@2 I@3 (optional)";
  field("Harmony", harmony, "harmony");
  el.appendChild(materialsElement("div", "materials-hint", MaterialsModel.HARMONY_HINT));

  el.appendChild(materialsElement("h2", "section-title", "Features"));
  const featureTable = materialsElement("table", "library-table materials-features");
  el.appendChild(featureTable);
  errors.features = materialsElement("div", "materials-field-error");
  el.appendChild(errors.features);
  let featureRows = initial.features.map((feature) => ({ ...feature }));

  function drawFeatures() {
    featureTable.innerHTML = "";
    if (featureRows.length > 0) {
      const head = materialsElement("tr");
      for (const label of ["Label", "First note", "Last note", ""]) {
        head.appendChild(materialsElement("th", "", label));
      }
      featureTable.appendChild(head);
    }
    featureRows.forEach((row, index) => {
      const tr = materialsElement("tr");
      for (const prop of ["label", "first", "last"]) {
        const input = materialsElement("input");
        input.type = "text";
        input.size = prop === "label" ? 8 : 4;
        input.value = String(row[prop]);
        input.setAttribute("aria-label", `Feature ${index + 1} ${prop}`);
        input.addEventListener("input", () => {
          row[prop] = prop === "label" ? input.value : input.value.trim() === "" ? NaN : Number(input.value);
        });
        const cell = materialsElement("td");
        cell.appendChild(input);
        tr.appendChild(cell);
      }
      const remove = materialsElement("button", "", "Remove");
      remove.type = "button";
      remove.addEventListener("click", () => {
        featureRows.splice(index, 1);
        drawFeatures();
      });
      const last = materialsElement("td");
      last.appendChild(remove);
      tr.appendChild(last);
      featureTable.appendChild(tr);
    });
  }
  drawFeatures();
  const addFeature = materialsElement("button", "", "Add feature");
  addFeature.type = "button";
  addFeature.addEventListener("click", () => {
    featureRows.push(MaterialsModel.newFeatureRow(featureRows));
    drawFeatures();
  });
  el.appendChild(addFeature);

  function values() {
    return {
      role: role.value,
      form: form.value,
      notes: notes.value,
      meter: meter.value,
      mode: mode.value,
      legacyKey,
      pickup: pickup.value,
      harmony: harmony.value,
      features: featureRows.map((row) => ({ ...row })),
    };
  }

  function showError(text, fieldName) {
    for (const holder of Object.values(errors)) {
      holder.textContent = "";
    }
    if (text) {
      errors[fieldName || "notes"].textContent = text;
    }
  }

  function showProblems(problems) {
    showError("");
    for (const problem of problems) {
      const holder = errors[problem.field];
      holder.textContent = holder.textContent ? `${holder.textContent} ${problem.message}` : problem.message;
    }
  }

  return { el, values, showProblems, showError };
}

/* The Save controls of a motif editor: an optional version note and a
 * button. onSave(values, note) resolves to the answer; its error text shows
 * beside the note list and the input stays. */
function buildMotifSaveControls(editor, statusEl, label, withNote, onSave) {
  const controls = materialsElement("div", "upload-controls");
  const note = materialsElement("input");
  note.type = "text";
  note.placeholder = "Version note (optional)";
  const save = materialsElement("button", "", label);
  save.type = "button";
  save.addEventListener("click", async () => {
    const values = editor.values();
    const problems = MaterialsModel.motifFormProblems(values);
    editor.showProblems(problems);
    if (problems.length > 0) {
      return;
    }
    save.disabled = true;
    statusEl.textContent = "";
    statusEl.classList.remove("error");
    const answer = await onSave(values, note.value);
    save.disabled = false;
    if (answer && !answer.ok) {
      const message = MaterialsModel.errorText(answer.data, "Could not save the motif");
      editor.showError(message, MaterialsModel.errorField(message));
    }
  });
  if (withNote) {
    controls.appendChild(note);
  }
  controls.appendChild(save);
  return controls;
}

function renderNewMotif(draftName, draftProject) {
  const content = getContentEl();
  content.innerHTML = "";
  const back = materialsElement("a", "back-link clickable", "Back to Motifs");
  back.addEventListener("click", () => renderMaterialsView("motif"));
  content.appendChild(back);
  content.appendChild(materialsElement("h1", "view-title", "New motif"));
  const statusEl = materialsElement("div", "upload-status");
  statusEl.setAttribute("role", "status");

  const meta = materialsElement("div", "upload-controls");
  const name = materialsElement("input");
  name.type = "text";
  name.placeholder = "Motif name";
  name.value = draftName || "";
  const project = materialsElement("select");
  fillProjectSelect(project, draftProject);
  meta.append(name, project);
  content.append(meta, statusEl);

  const editor = buildMotifEditor(null);
  content.appendChild(editor.el);
  content.appendChild(
    buildMotifSaveControls(editor, statusEl, "Save motif", false, async (values) => {
      if (name.value.trim() === "" || project.value === "") {
        statusEl.textContent = "Give the motif a name and a project.";
        statusEl.classList.add("error");
        return null;
      }
      const request = MaterialsModel.buildSaveMotifRequest({
        name: name.value,
        project_id: project.value,
        body: MaterialsModel.buildMotifBody(values, null),
      });
      const answer = await sendMaterialsRequest(request);
      if (answer.ok) {
        renderMaterialsDetail("motif", answer.data.item.id, answer.data.item.analysis_error);
      }
      return answer;
    }),
  );
}

/* The score and playback of a motif version in the score workspace, sounded in its stored key. Nothing here is saved. */
function buildMotifScore(id, item, body) {
  const motif = (body && body.motif) || {};
  const section = materialsElement("section", "panel materials-score");
  if (typeof motif.degrees !== "string" || motif.degrees === "") {
    return null;
  }
  const mountEl = materialsElement("div", "materials-score-mount");
  let workspace = null;
  function mountScore() {
    if (workspace) {
      workspace.destroy();
    }
    mountEl.innerHTML = "";
    const urls = MaterialsModel.versionScoreUrls(id, item.current_version);
    workspace = ScoreWorkspace.mount(mountEl, {
      title: item.name,
      subtitle: `${motif.form || "full"} motif · v${item.current_version} · ${motif.key || "C major"}`,
      musicxmlUrl: urls.musicxmlUrl,
      playbackUrl: urls.playbackUrl,
    });
  }
  section.appendChild(materialsElement("h2", "section-title", "Score"));
  section.appendChild(mountEl);
  mountScore();
  return section;
}

/* The Composer's analysis of a motif version body, or why there is none. */
function buildAnalysisPanel(body, analysisError, onReanalyzed) {
  const info = MaterialsModel.analysisRows(body && body.motif, analysisError);
  const section = materialsElement("section", "panel materials-analysis");
  section.appendChild(materialsElement("h2", "section-title", "Analysis"));
  const statusRow = materialsElement("div", "upload-controls");
  statusRow.appendChild(materialsElement("div", "piece-list-status", info.message));
  const again = materialsElement("button", "", "Re-analyze");
  again.type = "button";
  again.addEventListener("click", async () => {
    again.disabled = true;
    const answer = await onReanalyzed.send();
    if (answer.ok) {
      onReanalyzed.done(answer.data.item.analysis_error);
      return;
    }
    again.disabled = false;
    statusRow.appendChild(materialsElement("div", "upload-status error", MaterialsModel.errorText(answer.data, "Could not re-analyze")));
  });
  statusRow.appendChild(again);
  section.appendChild(statusRow);
  if (!info.analyzed) {
    return section;
  }
  const list = materialsElement("dl", "materials-analysis-list");
  for (const row of info.rows) {
    list.appendChild(materialsElement("dt", "", row.label));
    list.appendChild(materialsElement("dd", "", row.text));
  }
  section.appendChild(list);
  return section;
}

async function renderMaterialsDetail(type, id, analysisError) {
  const info = MaterialsModel.typeInfo(type);
  const content = getContentEl();
  content.innerHTML = "";
  const back = materialsElement("a", "back-link clickable", `Back to ${info.label}`);
  back.addEventListener("click", () => renderMaterialsView(type));
  content.appendChild(back);
  const statusEl = materialsElement("div", "upload-status");
  statusEl.setAttribute("role", "status");
  const answer = await sendMaterialsRequest(MaterialsModel.buildItemGetRequest(id));
  if (!answer.ok) {
    statusEl.textContent = MaterialsModel.errorText(answer.data, `Could not load the ${info.singular}`);
    statusEl.classList.add("error");
    content.appendChild(statusEl);
    return;
  }
  const item = answer.data.item;
  content.appendChild(materialsElement("h1", "view-title", item.name));

  const controls = materialsElement("div", "upload-controls");
  const name = materialsElement("input");
  name.type = "text";
  name.value = item.name;
  const project = materialsElement("select");
  fillProjectSelect(project, item.project_id);
  const save = materialsElement("button", "", "Save");
  save.type = "button";
  save.addEventListener("click", async () => {
    const fields = {};
    if (name.value.trim() !== item.name) {
      fields.name = name.value;
    }
    if (project.value !== item.project_id) {
      fields.project_id = project.value;
    }
    if (Object.keys(fields).length === 0) {
      return;
    }
    const saved = await sendMaterialsRequest(MaterialsModel.buildUpdateItemRequest(id, fields));
    if (!saved.ok) {
      statusEl.textContent = MaterialsModel.errorText(saved.data, "Could not save");
      statusEl.classList.add("error");
      return;
    }
    renderMaterialsDetail(type, id);
  });
  const del = materialsElement("button", "", "Delete");
  del.type = "button";
  del.addEventListener("click", async () => {
    if (!window.confirm(`Delete ${info.singular} "${item.name}" and all its versions?`)) {
      return;
    }
    let result = await sendMaterialsRequest(MaterialsModel.buildDeleteItemRequest(id, false));
    const warning = MaterialsModel.deleteWarning(item.name, result.status, result.data);
    if (warning) {
      if (!window.confirm(warning.message)) {
        return;
      }
      result = await sendMaterialsRequest(MaterialsModel.buildDeleteItemRequest(id, true));
    }
    if (!result.ok) {
      statusEl.textContent = MaterialsModel.errorText(result.data, `Could not delete the ${info.singular}`);
      statusEl.classList.add("error");
      return;
    }
    notifySubjectsChanged();
    renderMaterialsView(type);
  });
  controls.append(name, project, save, del);
  content.append(controls, statusEl);

  const versionEl = materialsElement("div");
  if (type !== "motif") {
    const subjectsField = buildSubjectsField("items", id, answer.data.subjects);
    const subjectsRow = materialsElement("div", "materials-subjects-row");
    subjectsRow.append(materialsElement("h2", "section-title", "Subjects"), subjectsField.el);
    content.appendChild(subjectsRow);
  }
  const allItemsAnswer = await sendMaterialsRequest(MaterialsModel.buildAllItemsRequest());
  const itemNameOf = (itemId) => {
    const found = (allItemsAnswer.ok ? allItemsAnswer.data.items : []).find((entry) => entry.id === itemId);
    return found ? found.name : itemId;
  };
  if (type === "motif") {
    const composerStatus = await sendMaterialsRequest({ method: "GET", url: "/api/composer/status" });
    if (!MaterialsModel.composerAvailable(composerStatus)) {
      const banner = materialsElement("div", "upload-status error", MaterialsModel.COMPOSER_MISSING_TEXT);
      banner.setAttribute("role", "alert");
      content.insertBefore(banner, content.children[1]);
    }
    MotifDetailView.render(content, {
      id,
      item,
      data: answer.data,
      analysisError,
      statusEl,
      allItems: allItemsAnswer.ok ? allItemsAnswer.data.items : [],
      rerender: (reason) => renderMaterialsDetail(type, id, reason),
    });
    return;
  }
  if (type === "theme") {
    const editor = buildThemeEditor({
      id,
      body: answer.data.body,
      usesStatus: answer.data.uses_status,
      items: allItemsAnswer.ok ? allItemsAnswer.data.items : [],
      statusEl,
      saved: () => renderMaterialsDetail(type, id),
    });
    content.appendChild(editor.el);
    const resultEl = buildThemeResult({
      id,
      version: item.current_version,
      theme: answer.data.body.theme,
      nameOf: itemNameOf,
      title: item.name,
    });
    if (resultEl) {
      content.appendChild(resultEl);
    }
  }
  if (type === "piece") {
    const editor = buildPieceEditor({
      id,
      body: answer.data.body,
      usesStatus: answer.data.uses_status,
      items: allItemsAnswer.ok ? allItemsAnswer.data.items : [],
      statusEl,
      saved: () => renderMaterialsDetail(type, id),
    });
    content.appendChild(editor.el);
    const resultEl = buildPieceResult({
      id,
      version: item.current_version,
      piece: answer.data.body.piece,
      nameOf: itemNameOf,
      title: item.name,
      statusEl,
      saved: () => renderMaterialsDetail(type, id),
    });
    if (resultEl) {
      content.appendChild(resultEl);
    }
  }
  content.appendChild(
    buildUsesSection({
      id,
      type,
      body: answer.data.body,
      statuses: answer.data.uses_status,
      allItems: allItemsAnswer.ok ? allItemsAnswer.data.items : [],
      statusEl,
    }),
  );
  content.appendChild(buildUsedBySection(answer.data.used_by));
  let renderThemeVersion;
  if (type === "theme") {
    renderThemeVersion = (number, body) => themeVersionView(versionEl, id, number, body, itemNameOf, item.name);
  } else if (type === "piece") {
    renderThemeVersion = (number, body) => pieceVersionView(versionEl, id, number, body, itemNameOf, item.name);
  }
  content.appendChild(buildVersionsList(item, versionEl, statusEl, renderThemeVersion));
  content.appendChild(versionEl);
}
