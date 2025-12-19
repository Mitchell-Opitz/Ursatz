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
  content.append(controls, progressEl, skippedEl, resultEl);

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
    }
  }

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
