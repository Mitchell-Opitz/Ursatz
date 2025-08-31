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
    for (const value of [row.name, row.project, row.version, row.updated]) {
      tr.appendChild(materialsElement("td", "", value));
    }
    tr.addEventListener("click", () => onOpen(row.id));
    table.appendChild(tr);
  }
  return table;
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
  content.append(statusEl, listEl);

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
  panel.appendChild(buildItemTable(rows, (id) => renderMaterialsDetail(type, id)));
  listEl.appendChild(panel);
}

function buildUsesList(uses) {
  const rows = MaterialsModel.useRows(uses);
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Uses"));
  if (rows.length === 0) {
    section.appendChild(materialsElement("div", "piece-list-status", "Uses nothing yet."));
    return section;
  }
  const list = materialsElement("ul", "materials-list");
  for (const row of rows) {
    list.appendChild(materialsElement("li", row.missing ? "error" : "", row.missing ? `${row.label} (missing)` : row.label));
  }
  section.appendChild(list);
  return section;
}

function buildVersionsList(item, viewEl, statusEl) {
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

async function renderMaterialsDetail(type, id) {
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
  controls.append(name, project, save);
  content.append(controls, statusEl);

  const versionEl = materialsElement("div");
  content.appendChild(buildUsesList(answer.data.uses_status));
  content.appendChild(buildVersionsList(item, versionEl, statusEl));
  content.appendChild(versionEl);
}
