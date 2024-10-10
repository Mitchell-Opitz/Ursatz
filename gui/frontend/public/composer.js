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

/* Sends a request to the Composer routes and resolves to {ok, status, data};
 * data is null for an answer that is not JSON. A network failure resolves to
 * {ok:false, status:0, data:{error}}. */
async function composerFetch(path, options) {
  try {
    const res = await fetch(`${COMPOSER_API}${path}`, options);
    let data = null;
    try {
      data = await res.json();
    } catch (err) {
      data = null;
    }
    return { ok: res.ok && !(data && data.error), status: res.status, data };
  } catch (err) {
    return { ok: false, status: 0, data: { error: `Could not reach backend: ${err}` } };
  }
}

function composerPost(path, body) {
  return composerFetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
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
    for (const text of [composition.name, composition.kind, composition.versions, composerTime(composition.created_at)]) {
      const cell = document.createElement("td");
      cell.textContent = text;
      row.appendChild(cell);
    }
    bodyEl.appendChild(row);
  }
}

if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    buildGrowRequest,
    buildPlanRequest,
    composerErrorText,
    composerTempo,
    composerOptionalInteger,
  };
}
