// Composer view: start a theme or a piece and work on it. Loaded after
// app.js. The request builders are plain functions (module.exports for the
// Node tests); the rest renders into #content.

const COMPOSER_API = "/api/composer";
const COMPOSER_METERS = ["2/4", "3/4", "4/4", "6/8"];
const COMPOSER_GROW_FORMS = ["sentence", "period", "hybrid"];
const COMPOSER_HYBRID_BEGINNINGS = ["presentation", "antecedent", "cbi"];
const COMPOSER_HYBRID_ENDINGS = ["continuation", "consequent", "cadential"];
const COMPOSER_PIECE_FORMS = ["binary", "rounded-binary", "ternary", "variations", "loop", "layered"];
const COMPOSER_CADENCES = ["PAC", "IAC", "HC"];
const COMPOSER_ACCOMPANIMENTS = ["block", "arpeggiated", "alberti", "oom-pah", "any"];
const COMPOSER_NOTE_HINT = "Notes as pitch:duration, for example D4:q F4:e A4:e D5:h (w h q e s)";

/* One optional whole number from a text field: {} when blank, {value} or
 * {error}. */
function composerOptionalInteger(text, label, low, high) {
  const trimmed = String(text === undefined || text === null ? "" : text).trim();
  if (trimmed === "") {
    return {};
  }
  if (!/^\d+$/.test(trimmed)) {
    return { error: `${label} must be a whole number.` };
  }
  const value = Number(trimmed);
  if (value < low || value > high) {
    return { error: `${label} must be ${low} to ${high}.` };
  }
  return { value };
}

/* Tempo is a whole number or "n/d", 20 to 400 beats per minute. */
function composerTempo(text) {
  const trimmed = String(text === undefined || text === null ? "" : text).trim();
  if (trimmed === "") {
    return {};
  }
  const fraction = /^(\d+)\/(\d+)$/.exec(trimmed);
  const bpm = fraction ? Number(fraction[1]) / Number(fraction[2]) : /^\d+$/.test(trimmed) ? Number(trimmed) : NaN;
  if (!(bpm >= 20 && bpm <= 400)) {
    return { error: "Tempo must be 20 to 400, a whole number or n/d." };
  }
  return { value: fraction ? trimmed : Number(trimmed) };
}

function composerText(values, key) {
  const text = values[key];
  return text === undefined || text === null ? "" : String(text).trim();
}

/* Adds the optional text fields that are not blank, the seed and the tempo
 * to body. Returns an error message or null. */
function composerAddCommon(body, values) {
  for (const key of ["cadence", "accompaniment"]) {
    if (composerText(values, key) !== "") {
      body[key] = composerText(values, key);
    }
  }
  const seed = composerOptionalInteger(values.seed, "Seed", 0, 4294967295);
  if (seed.error) {
    return seed.error;
  }
  if (seed.value !== undefined) {
    body.seed = seed.value;
  }
  const tempo = composerTempo(values.tempo);
  if (tempo.error) {
    return tempo.error;
  }
  if (tempo.value !== undefined) {
    body.tempo = tempo.value;
  }
  if (values.check) {
    body.check = true;
  }
  return null;
}

/* The body of POST /api/composer/grow from the New theme form's values:
 * {request} or {error}. */
function buildGrowRequest(values) {
  const body = {
    name: composerText(values, "name"),
    motive: composerText(values, "motive"),
    into: composerText(values, "into"),
    key: composerText(values, "key"),
    meter: composerText(values, "meter"),
  };
  for (const [key, label] of [["name", "Name"], ["motive", "Motive"], ["key", "Key"]]) {
    if (body[key] === "") {
      return { error: `${label} is required.` };
    }
  }
  if (body.into === "hybrid") {
    body.hybrid = composerText(values, "hybrid");
  }
  const bars = composerOptionalInteger(values.bars, "Bars", 4, 8);
  if (bars.error) {
    return { error: bars.error };
  }
  if (bars.value !== undefined) {
    body.bars = bars.value;
  }
  const error = composerAddCommon(body, values);
  return error ? { error } : { request: body };
}

/* The body of POST /api/composer/plan from the New piece form's values. */
function buildPlanRequest(values) {
  const body = {
    name: composerText(values, "name"),
    form: composerText(values, "form"),
    key: composerText(values, "key"),
    meter: composerText(values, "meter"),
  };
  for (const [key, label] of [["name", "Name"], ["key", "Key"]]) {
    if (body[key] === "") {
      return { error: `${label} is required.` };
    }
  }
  for (const [key, label, low, high] of [["bars", "Bars", 1, 256], ["count", "Count", 1, 8]]) {
    const number = composerOptionalInteger(values[key], label, low, high);
    if (number.error) {
      return { error: number.error };
    }
    if (number.value !== undefined) {
      body[key] = number.value;
    }
  }
  if (values.modulate) {
    body.modulate = true;
  }
  if (composerText(values, "motive") !== "") {
    body.motive = composerText(values, "motive");
  }
  if (composerText(values, "given_notes") !== "") {
    if (composerText(values, "given_at") === "") {
      return { error: "Enter where the given melody goes (At), for example B or B:2." };
    }
    body.given_melody = { notes: composerText(values, "given_notes"), at: composerText(values, "given_at") };
  }
  const error = composerAddCommon(body, values);
  if (error) {
    return { error };
  }
  delete body.cadence;
  return { request: body };
}

/* Text for an error answer: the backend's message and the Composer's stderr. */
function composerErrorText(status, data) {
  const message = data && data.error ? data.error : `request failed (${status})`;
  return data && data.stderr ? `${message}\n${String(data.stderr).trim()}` : message;
}

/* Sends a request to the Composer routes (fetchJsonResult in app.js). */
function composerFetch(path, options) {
  return fetchJsonResult(`${COMPOSER_API}${path}`, options);
}

function composerPost(path, body) {
  return postJsonResult(`${COMPOSER_API}${path}`, body);
}

/* Runs action() with the buttons disabled and a spinner in statusEl until it
 * settles, so a run of tens of seconds shows it is working. */
async function composerBusy(buttons, statusEl, message, action) {
  for (const button of buttons) {
    button.disabled = true;
  }
  statusEl.classList.remove("error");
  statusEl.innerHTML = "";
  const spinner = document.createElement("span");
  spinner.classList.add("spinner");
  statusEl.appendChild(spinner);
  statusEl.appendChild(document.createTextNode(` ${message}`));
  try {
    return await action();
  } finally {
    for (const button of buttons) {
      button.disabled = false;
    }
    if (statusEl.contains(spinner)) {
      statusEl.textContent = "";
    }
  }
}

function composerShowError(statusEl, status, data) {
  statusEl.textContent = composerErrorText(status, data);
  statusEl.classList.add("error");
}

function composerField(label, control) {
  const wrapper = document.createElement("label");
  wrapper.classList.add("composer-field");
  wrapper.appendChild(document.createTextNode(label));
  wrapper.appendChild(control);
  return wrapper;
}

function composerInput(name, placeholder, value) {
  const input = document.createElement("input");
  input.type = "text";
  input.name = name;
  if (placeholder) {
    input.placeholder = placeholder;
  }
  if (value !== undefined) {
    input.value = value;
  }
  return input;
}

function composerTextarea(name, hint) {
  const area = document.createElement("textarea");
  area.name = name;
  area.rows = 2;
  area.placeholder = hint;
  return area;
}

function composerSelect(name, options, blank) {
  const select = document.createElement("select");
  select.name = name;
  if (blank) {
    const empty = document.createElement("option");
    empty.value = "";
    empty.textContent = blank;
    select.appendChild(empty);
  }
  for (const option of options) {
    const element = document.createElement("option");
    element.value = option;
    element.textContent = option;
    select.appendChild(element);
  }
  return select;
}

function composerCheckbox(name) {
  const box = document.createElement("input");
  box.type = "checkbox";
  box.name = name;
  return box;
}

function composerFormValues(form) {
  const values = {};
  for (const element of form.elements) {
    if (element.name) {
      values[element.name] = element.type === "checkbox" ? element.checked : element.value;
    }
  }
  return values;
}

function composerHybrids() {
  const list = [];
  for (const beginning of COMPOSER_HYBRID_BEGINNINGS) {
    for (const ending of COMPOSER_HYBRID_ENDINGS) {
      list.push(`${beginning}+${ending}`);
    }
  }
  return list;
}

function buildComposerThemeForm(onCreated) {
  const form = document.createElement("form");
  form.classList.add("composer-form");
  const into = composerSelect("into", COMPOSER_GROW_FORMS);
  const hybrid = composerSelect("hybrid", composerHybrids());
  const hybridField = composerField("Hybrid", hybrid);
  hybridField.hidden = true;
  into.addEventListener("change", () => {
    hybridField.hidden = into.value !== "hybrid";
  });
  const meter = composerSelect("meter", COMPOSER_METERS);
  meter.value = "4/4";
  const bars = composerInput("bars", "8");
  const submit = document.createElement("button");
  submit.type = "submit";
  submit.textContent = "Grow theme";
  const status = document.createElement("div");
  status.classList.add("upload-status", "composer-status");

  form.append(
    composerField("Name", composerInput("name", "My theme")),
    composerField("Motive", composerTextarea("motive", COMPOSER_NOTE_HINT)),
    composerField("Into", into),
    hybridField,
    composerField("Key", composerInput("key", "D minor", "C major")),
    composerField("Meter", meter),
    composerField("Bars (4 to 8)", bars),
    composerField("Cadence", composerSelect("cadence", COMPOSER_CADENCES, "default")),
    composerField("Accompaniment", composerSelect("accompaniment", COMPOSER_ACCOMPANIMENTS, "default")),
    composerField("Check", composerCheckbox("check")),
    composerField("Seed", composerInput("seed", "random")),
    composerField("Tempo", composerInput("tempo", "120")),
    submit,
    status,
  );
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    submitComposerForm(form, buildGrowRequest, "/grow", submit, status, onCreated);
  });
  return form;
}

function buildComposerPieceForm(onCreated) {
  const form = document.createElement("form");
  form.classList.add("composer-form");
  const meter = composerSelect("meter", COMPOSER_METERS);
  meter.value = "4/4";
  const submit = document.createElement("button");
  submit.type = "submit";
  submit.textContent = "Plan piece";
  const status = document.createElement("div");
  status.classList.add("upload-status", "composer-status");

  form.append(
    composerField("Name", composerInput("name", "My piece")),
    composerField("Form", composerSelect("form", COMPOSER_PIECE_FORMS)),
    composerField("Key", composerInput("key", "D minor", "C major")),
    composerField("Meter", meter),
    composerField("Bars", composerInput("bars", "16")),
    composerField("Count", composerInput("count", "1 to 8")),
    composerField("Modulate", composerCheckbox("modulate")),
    composerField("Motive (optional)", composerTextarea("motive", COMPOSER_NOTE_HINT)),
    composerField("Given melody (optional)", composerTextarea("given_notes", COMPOSER_NOTE_HINT)),
    composerField("At", composerInput("given_at", "B or B:2")),
    composerField("Accompaniment", composerSelect("accompaniment", COMPOSER_ACCOMPANIMENTS, "default")),
    composerField("Check", composerCheckbox("check")),
    composerField("Seed", composerInput("seed", "random")),
    composerField("Tempo", composerInput("tempo", "120")),
    submit,
    status,
  );
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    submitComposerForm(form, buildPlanRequest, "/plan", submit, status, onCreated);
  });
  return form;
}

async function submitComposerForm(form, build, path, submit, status, onCreated) {
  const built = build(composerFormValues(form));
  if (built.error) {
    status.textContent = built.error;
    status.classList.add("error");
    return;
  }
  const result = await composerBusy([submit], status, "Composing, this can take a while...", () =>
    composerPost(path, built.request),
  );
  if (!result.ok) {
    composerShowError(status, result.status, result.data);
    return;
  }
  form.reset();
  await onCreated(result.data, built.request.name);
}

async function renderComposerView() {
  const content = getContentEl();
  content.innerHTML = `
    <h1 class="view-title">Composer</h1>
    <div class="composer-forms">
      <section class="panel"><h2>New theme</h2><div id="composer-theme-form"></div></section>
      <section class="panel"><h2>New piece</h2><div id="composer-piece-form"></div></section>
    </div>
    <section class="panel library-panel">
      <h2>Compositions</h2>
      <div id="composer-list-status" class="piece-list-status">Loading compositions...</div>
      <table id="composer-list-table" class="library-table" hidden>
        <thead><tr><th>Name</th><th>Kind</th><th>Versions</th><th>Created</th></tr></thead>
        <tbody id="composer-list-body"></tbody>
      </table>
    </section>
  `;
  const onCreated = async (data, name) => {
    await loadComposerList(`Created ${name}.`);
  };
  document.getElementById("composer-theme-form").appendChild(buildComposerThemeForm(onCreated));
  document.getElementById("composer-piece-form").appendChild(buildComposerPieceForm(onCreated));
  await loadComposerList("");
}

function composerTime(text) {
  return text ? String(text).replace("T", " ").replace(/Z$/, "") : "";
}

async function loadComposerList(message) {
  const statusEl = document.getElementById("composer-list-status");
  const tableEl = document.getElementById("composer-list-table");
  const bodyEl = document.getElementById("composer-list-body");
  statusEl.classList.remove("error");
  const result = await composerFetch("/compositions");
  if (!result.ok) {
    composerShowError(statusEl, result.status, result.data);
    return;
  }
  bodyEl.innerHTML = "";
  const compositions = result.data.compositions || [];
  tableEl.hidden = compositions.length === 0;
  statusEl.textContent = message || (compositions.length === 0 ? "No compositions yet." : "");
  for (const composition of compositions) {
    const row = document.createElement("tr");
    row.classList.add("clickable");
    row.addEventListener("click", () => showComposition(composition.id));
    for (const text of [composition.name, composition.kind, composition.versions, composerTime(composition.created_at)]) {
      const cell = document.createElement("td");
      cell.textContent = text;
      row.appendChild(cell);
    }
    bodyEl.appendChild(row);
  }
}

/* ---- plan tree ----------------------------------------------------- */

function composerRational(value) {
  return value && value.den ? value.num / value.den : 0;
}

/* Length of one bar in whole notes, from the plan's meter. */
function composerBarLength(plan) {
  const spans = plan && plan.meter && plan.meter.spans;
  const payload = spans && spans[0] && spans[0].composer && spans[0].composer.payload;
  return payload && payload.beats_per_bar && payload.beat_unit ? payload.beats_per_bar / payload.beat_unit : 1;
}

/* "bar 3" or "bars 1-8" for a span given in bars from the start. */
function composerBarsLabel(start, end) {
  const first = Math.floor(start + 1e-9) + 1;
  const last = Math.max(first, Math.ceil(end - 1e-9));
  return first === last ? `bar ${first}` : `bars ${first}-${last}`;
}

/* A form letter as the Composer takes it: A, B, A'. A section such as
 * "A return" has none and is addressed by its node. */
function composerSectionLetter(label) {
  return /^[A-Z]'*$/.test(label || "") ? label : null;
}

/* The plan's form as a tree of {id, kind, label, state, letter, bars, start,
 * end, children}. start and end are in whole notes. The trailing "material"
 * section (the motive pool, not part of the form) is left out. */
function buildPlanTree(plan) {
  const barLength = composerBarLength(plan);
  function convert(node, isSection) {
    const start = composerRational(node.span && node.span.start);
    const end = composerRational(node.span && node.span.end);
    const composer = node.composer || {};
    return {
      id: composer.id,
      kind: node.kind,
      label: node.label,
      state: composer.state,
      letter: isSection ? composerSectionLetter(node.label) : null,
      bars: composerBarsLabel(start / barLength, end / barLength),
      start,
      end,
      children: (node.children || []).map((child) => convert(child, false)),
    };
  }
  const form = plan && plan.form;
  if (!form) {
    return null;
  }
  const root = convert(form, false);
  root.children = (form.children || [])
    .filter((section) => !((section.composer && section.composer.constraints) || []).some((c) => c.kind === "material"))
    .map((section) => convert(section, true));
  if (root.children.length > 0) {
    root.end = root.children[root.children.length - 1].end;
    root.bars = composerBarsLabel(root.start / barLength, root.end / barLength);
  }
  return root;
}

/* Rows of the version's summary: one per section (or one for a grown theme)
 * with the form heard and the cadences. */
function buildSummaryRows(summary) {
  if (!summary) {
    return [];
  }
  const sections = (summary.fill && summary.fill.sections) || summary.sections;
  if (Array.isArray(sections)) {
    return sections.map((section) => ({
      section: section.letter,
      heard: section.heard,
      cadences: (section.cadences || []).join(", "),
    }));
  }
  if (Array.isArray(summary.themes)) {
    return [
      {
        section: "theme",
        heard: summary.pairing ? `${summary.into}, ${summary.pairing}` : summary.into,
        cadences: summary.cadence || "",
      },
    ];
  }
  return [];
}

function composerVersionLabel(version) {
  const seed = version.seed === null || version.seed === undefined ? "no seed" : `seed ${version.seed}`;
  return `v${version.n} · ${version.action} · ${seed} · ${composerTime(version.created_at)}`;
}

/* ---- composition view ---------------------------------------------- */

/* The version of the composition shown, so the actions know what they change. */
let composerCurrent = null;

function composerBackLink() {
  const back = document.createElement("a");
  back.textContent = "← Compositions";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => renderComposerView());
  return back;
}

async function showComposition(id, versionNumber, message) {
  setActiveTool("composer");
  const content = getContentEl();
  content.innerHTML = `
    <div id="composition-back"></div>
    <h1 class="view-title" id="composition-title">Loading...</h1>
    <div id="composition-status" class="piece-list-status composer-status"></div>
    <div id="composition-body" hidden>
      <div class="composer-toolbar">
        <label class="composer-field">Version <select id="composition-version"></select></label>
        <a id="composition-midi" class="back-link clickable" download>Download MIDI</a>
      </div>
      <section class="panel composer-panel"><h2>Plan</h2>
        <div class="composer-toolbar">
          <label class="composer-field">Lane <select id="composer-lane">
            <option value="">all lanes</option><option>harmony</option><option>bass</option><option>melody</option><option>inner</option>
          </select></label>
          <label class="composer-field">Seed <input type="text" id="composer-seed" placeholder="random" /></label>
        </div>
        <div id="composition-plan"></div>
      </section>
      <section class="panel composer-panel"><h2>Variations</h2><div id="composition-variations"></div></section>
      <section class="panel composer-panel"><h2>Summary</h2><div id="composition-summary"></div></section>
      <section class="panel composer-panel"><h2>Score</h2>
        <div id="composition-score-status" class="piece-list-status"></div>
        <div id="composition-score" class="analysis-score-container"></div>
      </section>
    </div>
  `;
  document.getElementById("composition-back").appendChild(composerBackLink());

  const statusEl = document.getElementById("composition-status");
  const result = await composerFetch(`/compositions/${encodeURIComponent(id)}`);
  if (!result.ok) {
    composerShowError(statusEl, result.status, result.data);
    return;
  }
  const meta = result.data;
  const versions = meta.versions || [];
  document.getElementById("composition-title").textContent = meta.name || id;
  document.getElementById("composition-body").hidden = false;

  const select = document.getElementById("composition-version");
  for (const version of versions) {
    const option = document.createElement("option");
    option.value = String(version.n);
    option.textContent = composerVersionLabel(version);
    select.appendChild(option);
  }
  const wanted = versionNumber || (versions.length ? versions[versions.length - 1].n : 1);
  select.value = String(wanted);
  select.addEventListener("change", () => showComposerVersion(id, versions, Number(select.value)));
  await showComposerVersion(id, versions, Number(select.value));
  if (message) {
    statusEl.textContent = message;
  }
}

async function showComposerVersion(id, versions, number) {
  const version = versions.find((item) => item.n === number) || versions[versions.length - 1];
  const base = `/compositions/${encodeURIComponent(id)}/versions/${version.n}`;
  composerCurrent = { id, versions, version: version.n, plan: null };

  document.getElementById("composition-midi").href = `${COMPOSER_API}${base}/midi`;
  renderComposerSummary(version.summary);

  const planEl = document.getElementById("composition-plan");
  planEl.textContent = "Loading plan...";
  const [planResult] = await Promise.all([composerFetch(`${base}/plan`), renderComposerScore(`${base}/musicxml`)]);
  if (!planResult.ok) {
    composerShowError(planEl, planResult.status, planResult.data);
    return;
  }
  composerCurrent.plan = planResult.data;
  planEl.textContent = "";
  const tree = buildPlanTree(planResult.data);
  if (tree) {
    planEl.appendChild(renderComposerTree(tree, true));
    renderComposerVariations(tree);
  }
}

function renderComposerSummary(summary) {
  const el = document.getElementById("composition-summary");
  el.innerHTML = "";
  const rows = buildSummaryRows(summary);
  if (rows.length === 0) {
    el.textContent = "No summary for this version.";
    return;
  }
  const table = document.createElement("table");
  table.classList.add("library-table");
  table.innerHTML = "<thead><tr><th>Section</th><th>Heard form</th><th>Cadences</th></tr></thead>";
  const body = document.createElement("tbody");
  for (const row of rows) {
    const tr = document.createElement("tr");
    for (const text of [row.section, row.heard, row.cadences]) {
      const td = document.createElement("td");
      td.textContent = text;
      tr.appendChild(td);
    }
    body.appendChild(tr);
  }
  table.appendChild(body);
  el.appendChild(table);
}

function renderComposerTree(node, isRoot) {
  const list = document.createElement("ul");
  list.classList.add("composer-tree");
  const item = document.createElement("li");
  const line = document.createElement("div");
  line.classList.add("composer-node");
  const label = document.createElement("span");
  label.classList.add("composer-node-label");
  label.textContent = node.letter ? `${node.letter} (${node.label})` : node.label;
  const bars = document.createElement("span");
  bars.classList.add("claim-row-where");
  bars.textContent = `${node.kind} · ${node.bars}`;
  const state = document.createElement("span");
  state.classList.add("composer-state", `composer-state-${node.state}`);
  state.textContent = node.state;
  line.append(label, bars, state);
  if (!isRoot) {
    renderComposerNodeActions(node, line);
  }
  item.appendChild(line);
  if (node.children.length > 0) {
    const children = document.createElement("li");
    children.classList.add("composer-children");
    for (const child of node.children) {
      children.appendChild(renderComposerTree(child, false));
    }
    item.appendChild(children);
  }
  list.appendChild(item);
  return list;
}

function composerDrawScore(path, containerEl, statusEl, zoom) {
  return drawMusicXmlScore(`${COMPOSER_API}${path}`, containerEl, statusEl, zoom);
}

function renderComposerScore(path) {
  return composerDrawScore(
    path,
    document.getElementById("composition-score"),
    document.getElementById("composition-score-status"),
  );
}

/* ---- lock, regenerate, variations ---------------------------------- */

const COMPOSER_LANES = ["harmony", "bass", "melody", "inner"];

function composerTarget(request, target) {
  if (target.node !== undefined) {
    request.node = target.node;
  } else {
    request.section = target.section;
  }
  if (target.lane) {
    request.lane = target.lane;
  }
  return request;
}

/* target: {node} or {section}, and lane (blank for all lanes). */
function buildLockRequest(version, target) {
  return composerTarget({ version }, target);
}

/* seedText blank takes a new random seed. */
function buildRegenerateRequest(version, target, seedText) {
  const seed = composerOptionalInteger(seedText, "Seed", 0, 4294967295);
  if (seed.error) {
    return { error: seed.error };
  }
  const request = composerTarget({ version }, target);
  if (seed.value !== undefined) {
    request.seed = seed.value;
  }
  return { request };
}

/* A harmonization takes a whole section (by its letter) and no lane. */
function buildVariationsRequest(version, target, countText, seedText, harmonize) {
  const count = composerOptionalInteger(countText, "Count", 1, 8);
  if (count.error || count.value === undefined) {
    return { error: count.error || "Count must be 1 to 8." };
  }
  const seed = composerOptionalInteger(seedText, "Seed", 0, 4294967295);
  if (seed.error) {
    return { error: seed.error };
  }
  let request;
  if (harmonize) {
    if (!target.letter) {
      return { error: "Harmonize needs a section with a form letter." };
    }
    request = { version, section: target.letter, harmonize: true };
  } else {
    request = composerTarget({ version }, target);
  }
  request.count = count.value;
  if (seed.value !== undefined) {
    request.seed = seed.value;
  }
  return { request };
}

/* Harmonize is offered when every melody span inside the section is Given
 * (the harmony is then written around the melody) and the section has a
 * letter. node is a plan tree node (start, end in whole notes). */
function canHarmonize(plan, node) {
  if (!node || !node.letter || !plan || !Array.isArray(plan.melody)) {
    return false;
  }
  const spans = plan.melody.filter((entry) => {
    const start = composerRational(entry.span && entry.span.start);
    const end = composerRational(entry.span && entry.span.end);
    return start >= node.start && end <= node.end;
  });
  return spans.length > 0 && spans.every((entry) => entry.composer && entry.composer.state === "given");
}

/* Every node below the form, flat, for the target selector. */
function composerTargets(tree) {
  const list = [];
  (function visit(node, depth) {
    if (depth > 0) {
      list.push(node);
    }
    node.children.forEach((child) => visit(child, depth + 1));
  })(tree, 0);
  return list;
}

function composerHeardText(sections) {
  return (sections || []).map((section) => `${section.letter}: ${section.heard}`).join("; ");
}

function composerActionButtons() {
  return Array.from(document.querySelectorAll(".composer-action"));
}

/* Runs one command on the shown version and opens the version it wrote. */
async function runComposerAction(path, request, message) {
  const statusEl = document.getElementById("composition-status");
  const current = composerCurrent;
  const result = await composerBusy(composerActionButtons(), statusEl, message, () =>
    composerPost(`/compositions/${encodeURIComponent(current.id)}${path}`, request),
  );
  if (!result.ok) {
    composerShowError(statusEl, result.status, result.data);
    return null;
  }
  await showComposition(current.id, result.data.version, `Wrote v${result.data.version}.`);
  return result.data;
}

function composerTargetOf(node, lane) {
  return { node: node.id, lane };
}

function renderComposerNodeActions(node, line) {
  const lane = () => document.getElementById("composer-lane").value;
  const seed = () => document.getElementById("composer-seed").value;
  const lock = document.createElement("button");
  lock.type = "button";
  lock.classList.add("composer-action");
  lock.textContent = "Lock";
  lock.addEventListener("click", () =>
    runComposerAction("/lock", buildLockRequest(composerCurrent.version, composerTargetOf(node, lane())), "Locking..."),
  );
  const regenerate = document.createElement("button");
  regenerate.type = "button";
  regenerate.classList.add("composer-action");
  regenerate.textContent = "Regenerate";
  regenerate.addEventListener("click", () => {
    const built = buildRegenerateRequest(composerCurrent.version, composerTargetOf(node, lane()), seed());
    if (built.error) {
      composerShowError(document.getElementById("composition-status"), 0, { error: built.error });
      return;
    }
    runComposerAction("/regenerate", built.request, "Regenerating, this can take a while...");
  });
  line.append(lock, regenerate);
}

function renderComposerVariations(tree) {
  const el = document.getElementById("composition-variations");
  el.innerHTML = "";
  const targets = composerTargets(tree);
  const target = document.createElement("select");
  for (const node of targets) {
    const option = document.createElement("option");
    option.value = String(node.id);
    option.textContent = `${node.letter || node.label} (node ${node.id}, ${node.bars})`;
    target.appendChild(option);
  }
  const lane = composerSelect("lane", COMPOSER_LANES, "all lanes");
  const count = composerInput("count", "", "3");
  const seed = composerInput("seed", "random");
  const harmonize = composerCheckbox("harmonize");
  const generate = document.createElement("button");
  generate.type = "button";
  generate.classList.add("composer-action");
  generate.textContent = "Generate";
  const status = document.createElement("div");
  status.classList.add("piece-list-status", "composer-status");
  const results = document.createElement("div");
  results.classList.add("composer-variation-grid");

  const selected = () => targets.find((node) => String(node.id) === target.value);
  function updateHarmonize() {
    const allowed = canHarmonize(composerCurrent.plan, selected());
    harmonize.disabled = !allowed;
    if (!allowed) {
      harmonize.checked = false;
    }
    lane.disabled = harmonize.checked;
  }
  target.addEventListener("change", updateHarmonize);
  harmonize.addEventListener("change", updateHarmonize);
  updateHarmonize();

  generate.addEventListener("click", async () => {
    const node = selected();
    const built = buildVariationsRequest(
      composerCurrent.version,
      { node: node.id, letter: node.letter, lane: lane.value },
      count.value,
      seed.value,
      harmonize.checked,
    );
    if (built.error) {
      composerShowError(status, 0, { error: built.error });
      return;
    }
    const current = composerCurrent;
    const result = await composerBusy(composerActionButtons(), status, "Generating variations, this can take a while...", () =>
      composerPost(`/compositions/${encodeURIComponent(current.id)}/variations`, built.request),
    );
    if (!result.ok) {
      composerShowError(status, result.status, result.data);
      return;
    }
    status.textContent = `Kept ${result.data.kept} of ${result.data.requested}, ${result.data.seeds_tried} seeds tried.`;
    renderComposerVariationResults(results, current.id, result.data);
  });

  const controls = document.createElement("div");
  controls.classList.add("composer-toolbar");
  controls.append(
    composerField("Target", target),
    composerField("Lane", lane),
    composerField("Count (1 to 8)", count),
    composerField("Seed", seed),
    composerField("Harmonize", harmonize),
    generate,
  );
  el.append(controls, status, results);
}

function renderComposerVariationResults(container, id, data) {
  container.innerHTML = "";
  for (const variation of data.variations || []) {
    const card = document.createElement("div");
    card.classList.add("composer-variation");
    const title = document.createElement("div");
    title.textContent = `#${variation.index} · seed ${variation.seed}`;
    const heard = document.createElement("div");
    heard.classList.add("claim-row-where");
    heard.textContent = composerHeardText(variation.sections);
    const scoreStatus = document.createElement("div");
    scoreStatus.classList.add("piece-list-status");
    const score = document.createElement("div");
    score.classList.add("analysis-score-container");
    const choose = document.createElement("button");
    choose.type = "button";
    choose.classList.add("composer-action");
    choose.textContent = "Choose";
    choose.addEventListener("click", async () => {
      const statusEl = document.getElementById("composition-status");
      const result = await composerBusy(composerActionButtons(), statusEl, "Choosing...", () =>
        composerFetch(
          `/compositions/${encodeURIComponent(id)}/variations/${data.set}/${variation.index}/choose`,
          { method: "POST" },
        ),
      );
      if (!result.ok) {
        composerShowError(statusEl, result.status, result.data);
        return;
      }
      await showComposition(id, result.data.version, `Wrote v${result.data.version}.`);
    });
    card.append(title, heard, scoreStatus, score, choose);
    container.appendChild(card);
    composerDrawScore(`/compositions/${encodeURIComponent(id)}/variations/${data.set}/${variation.index}/musicxml`, score, scoreStatus, 0.5);
  }
}

if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    buildPlanTree,
    buildLockRequest,
    buildRegenerateRequest,
    buildVariationsRequest,
    canHarmonize,
    buildSummaryRows,
    composerVersionLabel,
    composerBarsLabel,
    buildGrowRequest,
    buildPlanRequest,
    composerErrorText,
    composerTempo,
    composerOptionalInteger,
  };
}
