import MaterialsModel from "../models/materials-model.js";
import { el as element } from "../core/dom.js";
import MotifDetailView from "./motif-detail-view.js";
import ScoreWorkspace from "../score/score-workspace.js";
import ThemeDetailView from "./theme-detail-view.js";
import VariantTree from "../models/variant-tree.js";
import { buildUsesSection, buildVersionsList, markUpdatedUses, materialsState, materialsViewActive, openLinkedItem, sendMaterialsRequest } from "./materials-common.js";
import { buildSubjectsField } from "./subjects-view.js";
import { buildPieceEditor, buildPieceResult, pieceVersionView } from "./pieces-view.js";
import { shell } from "./shell-context.js";
import { buildMotifEditor, buildMotifSaveControls } from "./motif-detail-view.js";

/* Redraws the open Motifs, Themes or Pieces view, if one is open. */
export function refreshMaterialsView() {
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
export async function mountProjectPicker() {
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

function fillProjectSelect(select, selected) {
  select.innerHTML = "";
  for (const project of materialsState.projects) {
    const option = element("option", "", project.name);
    option.value = project.id;
    select.appendChild(option);
  }
  select.value = selected;
}

function buildNewItemControls(type, info, statusEl, reload) {
  const controls = element("div", "upload-controls");
  const name = element("input");
  name.type = "text";
  name.placeholder = `New ${info.singular} name`;
  const project = element("select");
  fillProjectSelect(project, MaterialsModel.defaultProject(materialsState.filter, materialsState.projects));
  const create = element("button", "", "New");
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
  const table = element("table", "library-table");
  const head = element("tr");
  for (const label of ["Name", "Project", "Version", "Updated"]) {
    head.appendChild(element("th", "", label));
  }
  table.appendChild(head);
  for (const row of rows) {
    const tr = element("tr", "clickable");
    tr.dataset.id = row.id;
    for (const value of [row.name, row.project, row.version, row.updated]) {
      tr.appendChild(element("td", "", value));
    }
    tr.addEventListener("click", () => onOpen(row.id));
    table.appendChild(tr);
  }
  return table;
}

/* The Random section of the New Motif page: one button draws one motif from the Composer (seed, mode and meter all
 * drawn) and shows its score and playback in the score workspace. Pressing it again draws another; "Use" hands the
 * motif's form values to onUse(values) to fill the editor. Nothing is stored. */
function buildRandomMotifs(statusEl, onUse) {
  const section = element("section", "panel materials-random");
  section.appendChild(element("h2", "section-title", "Random"));
  const buttons = element("div", "button-row");
  const draw = element("button", "", "Random");
  const use = element("button", "", "Use");
  for (const button of [draw, use]) {
    button.type = "button";
  }
  use.hidden = true;
  buttons.append(draw, use);
  const infoEl = element("div", "variant-meta");
  const selectedEl = element("div", "materials-score-mount");
  let workspace = null;
  let blobUrls = [];
  let current = null;

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

  function fail(data, fallback) {
    statusEl.classList.add("error");
    statusEl.textContent = MaterialsModel.errorText(data, fallback);
  }

  async function drawSeed(seed) {
    const values = { form: "full", role: "melodic", seed };
    draw.disabled = true;
    statusEl.classList.remove("error");
    statusEl.textContent = "Drawing a motif...";
    const answer = await sendMaterialsRequest(MaterialsModel.randomRequest(values));
    const motive = answer.ok && answer.data && answer.data.motives ? answer.data.motives[0] : null;
    const drawn = MaterialsModel.drawnSettings(answer.data);
    if (!motive || !drawn) {
      draw.disabled = false;
      fail(answer.data, "Could not draw a motif");
      return;
    }
    values.material = drawn.material;
    values.meter = drawn.meter;
    const candidate = MaterialsModel.randomCandidate(motive, seed);
    const rendered = await sendMaterialsRequest(MaterialsModel.renderRequest(VariantTree.candidateBody(MaterialsModel.randomCandidateToBody(candidate, values), candidate), MaterialsModel.candidateKeyText(candidate)));
    draw.disabled = false;
    if (!rendered.ok) {
      fail(rendered.data, "Could not render the motif");
      return;
    }
    statusEl.textContent = "";
    clearSelection();
    current = { candidate, values };
    const keyText = MaterialsModel.candidateKeyText(candidate);
    infoEl.textContent = `${values.meter}${keyText ? ` · ${keyText}` : ""} · seed ${seed}`;
    const xml = URL.createObjectURL(new Blob([rendered.data.musicxml], { type: "application/xml" }));
    const playback = URL.createObjectURL(new Blob([JSON.stringify(rendered.data.playback)], { type: "application/json" }));
    blobUrls = [xml, playback];
    workspace = ScoreWorkspace.mount(selectedEl, { title: "Random motif", subtitle: `seed ${seed}`, musicxmlUrl: xml, playbackUrl: playback });
    use.hidden = false;
  }

  draw.addEventListener("click", () => drawSeed(Math.floor(Math.random() * 2147483648)));
  use.addEventListener("click", () => current && onUse(MaterialsModel.motifFormValues(MaterialsModel.randomCandidateToBody(current.candidate, current.values))));
  section.append(buttons, infoEl, selectedEl);
  return section;
}

export async function renderMaterialsView(type) {
  const info = MaterialsModel.typeInfo(type);
  materialsState.type = type;
  const page = shell().contentEl();
  page.innerHTML = "";
  const content = type === "motif" ? page.appendChild(element("div", "motifs-section")) : page;
  content.appendChild(element("h1", "view-title", info.label));
  const statusEl = element("div", "upload-status");
  statusEl.setAttribute("role", "status");
  const listEl = element("div");
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
  content.append(listEl);

  const answer = await sendMaterialsRequest(MaterialsModel.buildItemListRequest(type, materialsState.filter));
  if (!materialsViewActive() || materialsState.type !== type) {
    return;
  }
  if (!answer.ok) {
    listEl.appendChild(
      element("div", "piece-list-status error", MaterialsModel.errorText(answer.data, "Could not load items")),
    );
    return;
  }
  const rows = MaterialsModel.itemRows(answer.data.items, materialsState.projects);
  if (rows.length === 0) {
    listEl.appendChild(element("div", "piece-list-status", `No ${info.label.toLowerCase()} yet.`));
    return;
  }
  const panel = element("section", "panel library-panel");
  const table = buildItemTable(rows, (id) => renderMaterialsDetail(type, id));
  panel.appendChild(table);
  listEl.appendChild(panel);
  markUpdatedUses(table, rows, type);
}

/* The Used by section: the items that use this one; each opens its detail. */
function buildUsedBySection(users) {
  const rows = MaterialsModel.usedByRows(users, materialsState.projects);
  const section = element("div");
  section.appendChild(element("h2", "section-title", "Used by"));
  if (rows.length === 0) {
    section.appendChild(element("div", "piece-list-status", "Not used by any item."));
    return section;
  }
  const list = element("ul", "materials-list");
  for (const row of rows) {
    const li = element("li");
    const open = element("a", "clickable", row.label);
    open.addEventListener("click", () => openLinkedItem(row.type, row.id));
    li.appendChild(open);
    li.appendChild(document.createTextNode(row.project ? ` · ${row.project}` : ""));
    list.appendChild(li);
  }
  section.appendChild(list);
  return section;
}

function renderNewMotif(draftName, draftProject) {
  const page = shell().contentEl();
  page.innerHTML = "";
  const content = page.appendChild(element("div", "motifs-section"));
  const back = element("a", "back-link clickable", "Back to Motifs");
  back.addEventListener("click", () => renderMaterialsView("motif"));
  content.appendChild(back);
  content.appendChild(element("h1", "view-title", "New motif"));
  const statusEl = element("div", "upload-status");
  statusEl.setAttribute("role", "status");

  const meta = element("div", "upload-controls");
  const name = element("input");
  name.type = "text";
  name.placeholder = "Motif name";
  name.value = draftName || "";
  const project = element("select");
  fillProjectSelect(project, draftProject);
  meta.append(name, project);
  content.append(meta, statusEl);

  const subjectsField = buildSubjectsField("items", null, []);
  const subjectsRow = element("div", "materials-subjects-row");
  subjectsRow.append(element("h2", "section-title", "Subjects"), subjectsField.el);
  content.appendChild(subjectsRow);

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
        note: "Created",
        body: MaterialsModel.buildMotifBody(values, null),
      });
      const answer = await sendMaterialsRequest(request);
      if (answer.ok) {
        const failed = await subjectsField.applyTo(answer.data.item.id);
        materialsState.notice = failed.length > 0 ? `Could not link the subjects: ${failed.join(", ")}.` : "";
        renderMaterialsDetail("motif", answer.data.item.id, answer.data.item.analysis_error);
      }
      return answer;
    }),
  );
  if (materialsState.projects.length > 0) {
    content.appendChild(
      buildRandomMotifs(statusEl, (values) => {
        editor.fill(values);
        editor.el.scrollIntoView();
      }),
    );
  }
}

export async function renderMaterialsDetail(type, id, analysisError) {
  const info = MaterialsModel.typeInfo(type);
  const content = shell().contentEl();
  content.innerHTML = "";
  const back = element("a", "back-link clickable", `Back to ${info.label}`);
  back.addEventListener("click", () => renderMaterialsView(type));
  content.appendChild(back);
  const statusEl = element("div", "upload-status");
  statusEl.setAttribute("role", "status");
  const answer = await sendMaterialsRequest(MaterialsModel.buildItemGetRequest(id));
  if (!answer.ok) {
    statusEl.textContent = MaterialsModel.errorText(answer.data, `Could not load the ${info.singular}`);
    statusEl.classList.add("error");
    content.appendChild(statusEl);
    return;
  }
  const item = answer.data.item;
  /* A variant belongs to its motif (R-187) and has no page of its own: its link lands on the parent's Variants tab. */
  if (type === "motif" && item.variant_of && item.variant_of !== id) {
    MaterialsModel.writeMotifTab("variants");
    renderMaterialsDetail("motif", item.variant_of, analysisError);
    return;
  }
  content.appendChild(element("h1", "view-title", item.name));

  const controls = element("div", "upload-controls");
  const name = element("input");
  name.type = "text";
  name.value = item.name;
  const project = element("select");
  fillProjectSelect(project, item.project_id);
  const save = element("button", "", "Save");
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
  const del = element("button", "button-danger", "Delete");
  del.type = "button";
  del.addEventListener("click", async () => {
    const variants = type === "motif" && allItemsAnswer.ok ? MaterialsModel.variantCount(allItemsAnswer.data.items, id) : 0;
    if (!window.confirm(MaterialsModel.deleteConfirmText(info.singular, item.name, variants))) {
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
    shell().notifySubjectsChanged();
    renderMaterialsView(type);
  });
  if (type === "motif" || type === "theme") {
    controls.append(name, project, del);
  } else {
    controls.append(name, project, save, del);
  }
  content.append(controls, statusEl);

  const versionEl = element("div");
  if (type !== "motif" && type !== "theme") {
    const subjectsField = buildSubjectsField("items", id, answer.data.subjects);
    const subjectsRow = element("div", "materials-subjects-row");
    subjectsRow.append(element("h2", "section-title", "Subjects"), subjectsField.el);
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
      const banner = element("div", "upload-status error", MaterialsModel.COMPOSER_MISSING_TEXT);
      banner.setAttribute("role", "alert");
      content.insertBefore(banner, content.children[1]);
    }
    if (materialsState.notice) {
      statusEl.textContent = materialsState.notice;
      statusEl.classList.add("error");
      materialsState.notice = "";
    }
    MotifDetailView.render(content, {
      id,
      item,
      data: answer.data,
      analysisError,
      statusEl,
      nameInput: name,
      projectSelect: project,
      allItems: allItemsAnswer.ok ? allItemsAnswer.data.items : [],
      rerender: (reason) => renderMaterialsDetail(type, id, reason),
    });
    return;
  }
  if (type === "theme") {
    ThemeDetailView.render(content, {
      id,
      item,
      data: answer.data,
      statusEl,
      nameInput: name,
      projectSelect: project,
      allItems: allItemsAnswer.ok ? allItemsAnswer.data.items : [],
      nameOf: itemNameOf,
      rerender: () => renderMaterialsDetail(type, id),
    });
    return;
  }
  if (type === "piece") {
    const editor = buildPieceEditor({
      id,
      version: item.current_version,
      title: item.name,
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
      piece: answer.data.body,
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
  const renderPieceVersion = type === "piece" ? (number, body) => pieceVersionView(versionEl, id, number, body, itemNameOf, item.name) : undefined;
  content.appendChild(buildVersionsList(item, versionEl, statusEl, renderPieceVersion));
  content.appendChild(versionEl);
}
