/* The Themes view's editor (P16, R-167): the form, slot motifs, pool and
 * accompaniment of a theme, mounted in the theme detail of materials-view.js.
 * Pure parts are in themes-model.js; materialsElement, sendMaterialsRequest,
 * materialsState and renderMaterialsDetail come from materials-view.js. */

function themeField(parent, labelText, control) {
  const label = materialsElement("label", "filter-field", `${labelText} `);
  label.appendChild(control);
  parent.appendChild(label);
  return control;
}

function themeSelect(options, value) {
  const select = materialsElement("select");
  for (const option of options) {
    const entry = typeof option === "object" ? option : { value: option, label: String(option) };
    const el = materialsElement("option", "", entry.label);
    el.value = String(entry.value);
    select.appendChild(el);
  }
  select.value = String(value);
  return select;
}

function themeInput(placeholder, value) {
  const el = materialsElement("input");
  el.type = "text";
  el.placeholder = placeholder;
  el.value = value || "";
  return el;
}

function themeButton(text, onClick) {
  const button = materialsElement("button", "", text);
  button.type = "button";
  button.addEventListener("click", onClick);
  return button;
}

/* Plays one motif body through the render route in a score workspace below the picker. */
async function themePlayMotif(host, title, body, statusEl) {
  statusEl.classList.remove("error");
  statusEl.textContent = "Rendering...";
  const answer = await sendMaterialsRequest(MaterialsModel.renderRequest(body));
  if (!answer.ok) {
    statusEl.classList.add("error");
    statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not render the motif");
    return;
  }
  statusEl.textContent = "";
  if (host.themeWorkspace) {
    host.themeWorkspace.destroy();
  }
  for (const url of host.themeBlobUrls || []) {
    URL.revokeObjectURL(url);
  }
  host.innerHTML = "";
  const xml = URL.createObjectURL(new Blob([answer.data.musicxml], { type: "application/xml" }));
  const playback = URL.createObjectURL(new Blob([JSON.stringify(answer.data.playback)], { type: "application/json" }));
  host.themeBlobUrls = [xml, playback];
  host.themeWorkspace = ScoreWorkspace.mount(host, { title, subtitle: "motif", musicxmlUrl: xml, playbackUrl: playback });
}

/*
 * A picker of one full motif: project select, name search, the motif, its
 * version (current by default), its degrees and a Play button. role is
 * "melodic" or "accompaniment". value is {item_id, version} or null;
 * onChange gets the same, or null when the pick is cleared or not usable.
 * Returns {el, setValue(value)}.
 */
function buildThemeMotifPicker(options) {
  const role = options.role;
  const el = materialsElement("div", "theme-motif-picker");
  const controls = materialsElement("div", "upload-controls");
  const projectSelect = themeSelect(MaterialsModel.projectOptions(materialsState.projects), MaterialsModel.ALL_PROJECTS);
  const query = themeInput("Search motifs by name");
  const choice = materialsElement("select", "materials-use-choice");
  const versionSelect = materialsElement("select");
  const infoEl = materialsElement("div", "piece-list-status");
  const playHost = materialsElement("div");
  const playStatus = materialsElement("div", "upload-status");
  playStatus.setAttribute("role", "status");
  const play = themeButton("Play", () => themePlayMotif(playHost, infoEl.dataset.name || "Motif", current.body, playStatus));
  const clear = themeButton("Clear", () => {
    choice.value = "";
    current = { item_id: "", version: 0, body: null };
    versionSelect.innerHTML = "";
    show("");
    options.onChange(null);
  });
  play.disabled = true;
  let current = { item_id: "", version: 0, body: null };
  let loading = 0;

  function fillChoices(keep) {
    const found = MaterialsModel.pickerItems(options.items(), materialsState.projects, {
      query: query.value,
      type: "motif",
      project: projectSelect.value,
    });
    choice.innerHTML = "";
    const blank = materialsElement("option", "", found.length === 0 ? "No motifs" : "Choose a motif");
    blank.value = "";
    choice.appendChild(blank);
    for (const item of found) {
      const option = materialsElement("option", "", item.label);
      option.value = item.id;
      option.dataset.name = item.name;
      choice.appendChild(option);
    }
    choice.value = keep && found.some((item) => item.id === keep) ? keep : "";
  }

  function show(text, isError) {
    infoEl.textContent = text;
    infoEl.classList.toggle("error", Boolean(isError));
    play.disabled = !current.body;
  }

  /* Loads a version body and accepts it when it is a full motif of the role. */
  async function loadVersion(itemId, version, announce) {
    const ticket = ++loading;
    const answer = await sendMaterialsRequest(MaterialsModel.buildVersionGetRequest(itemId, version));
    if (ticket !== loading) {
      return;
    }
    const motif = answer.ok && answer.data && answer.data.body ? answer.data.body.motif : null;
    if (!motif || motif.form !== "full" || (motif.role || "melodic") !== role) {
      current = { item_id: "", version: 0, body: null };
      show(`Pick a full ${role} motif.`, true);
      if (announce) {
        options.onChange(null);
      }
      return;
    }
    current = { item_id: itemId, version: Number(version), body: answer.data.body };
    infoEl.dataset.name = choice.selectedOptions[0] ? choice.selectedOptions[0].dataset.name : itemId;
    show(`${motif.degrees} · ${motif.meter}`);
    if (announce) {
      options.onChange({ item_id: itemId, version: Number(version) });
    }
  }

  async function chooseItem(itemId, version) {
    versionSelect.innerHTML = "";
    if (!itemId) {
      current = { item_id: "", version: 0, body: null };
      show("");
      options.onChange(null);
      return;
    }
    const answer = await sendMaterialsRequest(MaterialsModel.buildItemGetRequest(itemId));
    if (!answer.ok) {
      show(MaterialsModel.errorText(answer.data, "Could not load the motif"), true);
      return;
    }
    const item = answer.data.item;
    for (const row of MaterialsModel.versionRows(item)) {
      const option = materialsElement("option", "", row.current ? `v${row.number} (current)` : `v${row.number}`);
      option.value = String(row.number);
      versionSelect.appendChild(option);
    }
    versionSelect.value = String(version || item.current_version);
    await loadVersion(itemId, Number(versionSelect.value), true);
  }

  projectSelect.addEventListener("change", () => fillChoices(choice.value));
  query.addEventListener("input", () => fillChoices(choice.value));
  choice.addEventListener("change", () => chooseItem(choice.value, 0));
  versionSelect.addEventListener("change", () => loadVersion(choice.value, Number(versionSelect.value), true));
  controls.append(projectSelect, query, choice, versionSelect, play, clear);
  el.append(controls, infoEl, playStatus, playHost);
  fillChoices("");

  /* Shows a stored value without announcing it. */
  async function setValue(value) {
    if (!value || !value.item_id) {
      return;
    }
    const known = options.items().some((item) => item.id === value.item_id);
    if (!known) {
      show(`Motif ${value.item_id} not found.`, true);
      return;
    }
    projectSelect.value = MaterialsModel.ALL_PROJECTS;
    query.value = "";
    fillChoices(value.item_id);
    const answer = await sendMaterialsRequest(MaterialsModel.buildItemGetRequest(value.item_id));
    if (answer.ok) {
      versionSelect.innerHTML = "";
      for (const row of MaterialsModel.versionRows(answer.data.item)) {
        const option = materialsElement("option", "", row.current ? `v${row.number} (current)` : `v${row.number}`);
        option.value = String(row.number);
        versionSelect.appendChild(option);
      }
      versionSelect.value = String(value.version);
    }
    await loadVersion(value.item_id, value.version, false);
  }
  return { el, setValue };
}

/* The pool section: the saved forms each slot motif has, by flag. */
function buildThemePoolSection(state, itemName, onPool) {
  const section = materialsElement("div");
  section.appendChild(materialsElement("h3", "section-title", "Pool"));
  const poolSelect = themeSelect(ThemesModel.POOLS, state.pool);
  poolSelect.addEventListener("change", () => onPool(poolSelect.value));
  themeField(section, "Saved forms", poolSelect);
  const list = materialsElement("ul", "materials-list");
  section.appendChild(list);

  /* Redraws the counts for the slots now chosen. */
  async function refresh() {
    list.innerHTML = "";
    for (const entry of state.slots) {
      const li = materialsElement("li", "", `${ThemesModel.SLOT_LABELS[entry.slot]}: ${itemName(entry.item_id)} · `);
      list.appendChild(li);
      const answer = await sendMaterialsRequest({ method: "GET", url: `/api/materials/items/${encodeURIComponent(entry.item_id)}/variant-tree` });
      li.appendChild(
        document.createTextNode(answer.ok ? ThemesModel.poolText(ThemesModel.poolCounts(answer.data.tree, entry.item_id)) : "forms unknown"),
      );
    }
  }
  return { el: section, refresh };
}

/* The lists the brief picker offers, as {collections, composers, sources, tags, subjects, projects, pieces, styles}. */
async function loadThemeBriefChoices() {
  const [choices, pieces, styles] = await Promise.all([
    loadMotiveLinksChoices(),
    fetchJsonResult("/api/analyzer/pieces"),
    fetchJsonResult("/api/styles"),
  ]);
  choices.pieces = pieces.ok && pieces.data && Array.isArray(pieces.data.pieces) ? pieces.data.pieces : [];
  choices.styles = styles.ok && styles.data && Array.isArray(styles.data.styles) ? styles.data.styles : [];
  return choices;
}

/* The brief section: chips of what is picked, the picker, the motive switches, weight, tempo, check and seed. */
function buildThemeBriefSection(state) {
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Brief"));
  const chipsEl = materialsElement("div", "theme-chips");
  const emptyEl = materialsElement("div", "piece-list-status", "No brief: the theme is grown without a style.");
  const picker = materialsElement("div", "upload-controls");
  let names = {};
  let choices = null;

  function drawChips() {
    chipsEl.innerHTML = "";
    emptyEl.hidden = state.picks.length > 0;
    state.picks.forEach((pick, index) => {
      const chip = materialsElement("span", "theme-chip", ThemesModel.pickLabel(pick, names));
      chip.appendChild(
        themeButton("Remove", () => {
          state.picks.splice(index, 1);
          drawChips();
        }),
      );
      chipsEl.appendChild(chip);
    });
  }

  const kindSelect = themeSelect(
    [
      { value: "collection", label: "Collection" },
      { value: "filter", label: "Composer / source / tag" },
      { value: "subject", label: "Subject" },
      { value: "project", label: "Project" },
      { value: "piece", label: "Piece" },
      { value: "style", label: "Style" },
    ],
    "collection",
  );
  const valueSelect = materialsElement("select");
  const filterSelects = {
    composer: materialsElement("select"),
    source: materialsElement("select"),
    tag: materialsElement("select"),
  };

  function fillSelect(select, options, blank) {
    select.innerHTML = "";
    if (blank !== undefined) {
      const o = materialsElement("option", "", blank);
      o.value = "";
      select.appendChild(o);
    }
    for (const option of options) {
      const o = materialsElement("option", "", option.label);
      o.value = option.value;
      select.appendChild(o);
    }
  }

  function fillPicker() {
    const kind = kindSelect.value;
    valueSelect.hidden = kind === "filter";
    for (const select of Object.values(filterSelects)) {
      select.hidden = kind !== "filter";
    }
    if (!choices) {
      return;
    }
    const byId = (list, labelOf) => list.map((entry) => ({ value: entry.id, label: labelOf(entry) }));
    if (kind === "collection") {
      fillSelect(valueSelect, byId(choices.collections, (entry) => entry.name || entry.id));
    } else if (kind === "subject") {
      fillSelect(valueSelect, byId(choices.subjects, (entry) => entry.name || entry.id));
    } else if (kind === "project") {
      fillSelect(valueSelect, byId(choices.projects, (entry) => entry.name || entry.id));
    } else if (kind === "piece") {
      fillSelect(valueSelect, byId(choices.pieces, (entry) => entry.filename || entry.id));
    } else if (kind === "style") {
      fillSelect(valueSelect, byId(choices.styles, (entry) => entry.name || entry.id));
    } else {
      fillSelect(filterSelects.composer, choices.composers, "Any composer");
      fillSelect(filterSelects.source, choices.sources, "Any source");
      fillSelect(filterSelects.tag, choices.tags, "Any tag");
    }
  }

  const add = themeButton("Add to brief", () => {
    const kind = kindSelect.value;
    let pick;
    if (kind === "filter") {
      pick = { kind, composer: filterSelects.composer.value, source: filterSelects.source.value, tag: filterSelects.tag.value };
    } else if (kind === "piece" || kind === "style") {
      pick = { kind, id: valueSelect.value };
    } else {
      pick = { kind, [`${kind}Id`]: valueSelect.value };
    }
    if ((kind !== "filter" && valueSelect.value === "") || !ThemesModel.briefFromPicks([pick])) {
      return;
    }
    if (!state.picks.some((other) => ThemesModel.pickLabel(other, names) === ThemesModel.pickLabel(pick, names))) {
      state.picks.push(pick);
    }
    drawChips();
  });
  kindSelect.addEventListener("change", fillPicker);
  picker.append(kindSelect, valueSelect, filterSelects.composer, filterSelects.source, filterSelects.tag, add);
  fillPicker();
  drawChips();
  loadThemeBriefChoices().then((loaded) => {
    choices = loaded;
    const table = (list, labelOf) => Object.fromEntries(list.map((entry) => [entry.id, labelOf(entry)]));
    names = {
      collections: table(loaded.collections, (entry) => entry.name || entry.id),
      subjects: table(loaded.subjects, (entry) => entry.name || entry.id),
      projects: table(loaded.projects, (entry) => entry.name || entry.id),
      pieces: table(loaded.pieces, (entry) => entry.filename || entry.id),
      styles: table(loaded.styles, (entry) => entry.name || entry.id),
    };
    fillPicker();
    drawChips();
  });
  section.append(chipsEl, emptyEl, picker);

  /* Motive switches */
  const switches = materialsElement("div");
  switches.appendChild(materialsElement("div", "", ThemesModel.SWITCH_TEXT));
  for (const entry of ThemesModel.MOTIVE_CATEGORIES) {
    const label = materialsElement("label", "filter-field", `${entry.label} `);
    const box = materialsElement("input");
    box.type = "checkbox";
    box.checked = state.switches[entry.category] !== false;
    box.addEventListener("change", () => {
      state.switches[entry.category] = box.checked;
    });
    label.prepend(box);
    switches.appendChild(label);
  }
  section.appendChild(switches);

  const options = materialsElement("div", "upload-controls");
  const weight = themeField(options, "Weight", themeSelect(ThemesModel.WEIGHTS, state.weight));
  weight.addEventListener("input", () => {
    state.weight = weight.value;
  });
  const tempo = themeField(options, "Tempo", themeInput("from the brief", state.tempo));
  tempo.addEventListener("input", () => {
    state.tempo = tempo.value;
  });
  const seed = themeField(options, "Seed", themeInput("random", state.seed));
  seed.addEventListener("input", () => {
    state.seed = seed.value;
  });
  const checkLabel = materialsElement("label", "filter-field", "Check with the Analyzer ");
  const check = materialsElement("input");
  check.type = "checkbox";
  check.checked = state.check;
  check.addEventListener("change", () => {
    state.check = check.checked;
  });
  checkLabel.prepend(check);
  options.appendChild(checkLabel);
  section.appendChild(options);
  return section;
}

/* A two-column table of label and text rows. */
function themeFacts(rows) {
  const list = materialsElement("dl", "materials-analysis-list");
  for (const [label, text] of rows) {
    if (text !== "" && text !== undefined) {
      list.appendChild(materialsElement("dt", "", label));
      list.appendChild(materialsElement("dd", "", text));
    }
  }
  return list;
}

/* Mounts the version's theme.musicxml in the score workspace, with playback from its theme.mid. */
async function themeMountScore(host, id, version, title) {
  const files = (name) => ThemesModel.versionFileUrl(id, version, name);
  let playbackUrl = null;
  try {
    const midi = await fetch(files("theme.mid"));
    const playback = midi.ok ? ThemesModel.playbackFromMidi(await midi.arrayBuffer()) : null;
    if (playback) {
      playbackUrl = URL.createObjectURL(new Blob([JSON.stringify(playback)], { type: "application/json" }));
    }
  } catch (error) {
    playbackUrl = null;
  }
  if (host.themeWorkspace) {
    host.themeWorkspace.destroy();
  }
  host.innerHTML = "";
  host.themeWorkspace = ScoreWorkspace.mount(host, {
    title,
    subtitle: `theme · v${version}`,
    musicxmlUrl: files("theme.musicxml"),
    playbackUrl,
  });
}

/*
 * The result of a theme version: the Composer's summary as rows, the score with playback and the MIDI download.
 * ctx: {id, version, theme (the version body's theme), nameOf, title}. Returns null when the version has no result.
 */
function buildThemeResult(ctx) {
  const result = ctx.theme && ctx.theme.result;
  if (!result || typeof result !== "object") {
    return null;
  }
  const state = ThemesModel.formFromBody({ uses: [], theme: ctx.theme }, ctx.id);
  const view = ThemesModel.resultView(result, {
    motiveNames: ThemesModel.motiveNames(state, ctx.nameOf),
    check: ctx.theme.check === true,
  });
  const section = materialsElement("section", "panel materials-analysis theme-result");
  section.appendChild(materialsElement("h2", "section-title", "Result"));
  section.appendChild(
    themeFacts([
      ["Seed", view.seed === null ? "" : String(view.seed)],
      ["Form heard", view.heard],
      ["Tempo", view.brief.tempo],
      ["Mode", view.brief.mode],
      ["Note density", view.brief.density],
      ["Read without", view.brief.without],
      ["Brief", view.pieces],
      ["Analyzer check", view.check],
      ...view.distance.map((entry) => [`Style distance, ${entry.label}`, entry.value]),
    ]),
  );
  const table = materialsElement("table", "library-table");
  const head = materialsElement("tr");
  for (const label of ["Idea", "Slot", "Motif", "Source", "Chain", "Connection"]) {
    head.appendChild(materialsElement("th", "", label));
  }
  table.appendChild(head);
  for (const row of view.rows) {
    const tr = materialsElement("tr");
    const source = row.item ? `${row.source}: ${ctx.nameOf(row.item)}` : row.source;
    for (const cell of [row.idea, row.slot, row.motive, source, row.chain, row.connection]) {
      tr.appendChild(materialsElement("td", "", cell));
    }
    table.appendChild(tr);
  }
  section.appendChild(table);
  const midi = materialsElement("a", "back-link clickable", "Download MIDI");
  midi.href = ThemesModel.versionFileUrl(ctx.id, ctx.version, "theme.mid");
  midi.download = "theme.mid";
  const scoreHost = materialsElement("div", "materials-score-mount");
  section.append(midi, scoreHost);
  themeMountScore(scoreHost, ctx.id, ctx.version, ctx.title);
  return section;
}

/* Shows an older version read only: its form and slots, then its result. */
function themeVersionView(viewEl, id, number, body, nameOf, title) {
  viewEl.innerHTML = "";
  viewEl.appendChild(materialsElement("h2", "section-title", `Version ${number} (read only)`));
  const theme = body && body.theme ? body.theme : {};
  const state = ThemesModel.formFromBody(body, id);
  viewEl.appendChild(
    themeFacts([
      ["Form", `${state.into}${state.into === "hybrid" ? ` (${state.hybrid})` : ""}, ${state.key} ${state.mode}, ${state.meter}, ${state.bars} bars, ${state.cadence}`],
      ...state.slots.map((entry) => [ThemesModel.SLOT_LABELS[entry.slot], `${nameOf(entry.item_id)} v${entry.version}`]),
      ["Pool", state.pool],
      ["Brief", ThemesModel.briefLabel(ThemesModel.briefFromPicks(state.picks))],
    ]),
  );
  const result = buildThemeResult({ id, version: number, theme, nameOf, title });
  viewEl.appendChild(result || materialsElement("div", "piece-list-status", "This version was not composed."));
}

/*
 * The theme editor. ctx: {id, body, items (all items), statusEl, saved(answer)}.
 * Returns {el, state, problems(), save(note), compose(options)}.
 */
function buildThemeEditor(ctx) {
  const state = ThemesModel.formFromBody(ctx.body, ctx.id);
  const initialJson = JSON.stringify(ThemesModel.bodyFromForm(state));
  const el = materialsElement("section", "panel materials-editor theme-editor");
  const problemsEl = materialsElement("div", "materials-field-error");
  const nameOf = (id) => {
    const found = ctx.items.find((item) => item.id === id);
    return found ? found.name : id;
  };
  const itemsOf = () => ctx.items;

  /* Form */
  el.appendChild(materialsElement("h2", "section-title", "Form"));
  const form = materialsElement("div", "upload-controls");
  const into = themeField(form, "Into", themeSelect(ThemesModel.INTOS, state.into));
  const [beginning, ending] = state.hybrid.split("+");
  const beginningSelect = themeField(form, "Beginning", themeSelect(ThemesModel.BEGINNINGS, beginning));
  const endingSelect = themeField(form, "Ending", themeSelect(ThemesModel.ENDINGS, ending));
  const key = themeField(form, "Key", themeInput("D or Bb", state.key));
  const mode = themeField(form, "Mode", themeSelect(ThemesModel.MODES, state.mode));
  const meter = themeField(form, "Meter", themeSelect(ThemesModel.METERS, state.meter));
  const bars = themeField(form, "Bars", themeSelect(ThemesModel.BARS, state.bars));
  const cadence = themeField(form, "Cadence", themeSelect(ThemesModel.CADENCES, state.cadence));
  const firstCadence = themeField(
    form,
    "First cadence",
    themeSelect([{ value: "", label: "Default" }, ...ThemesModel.FIRST_CADENCES], state.firstCadence),
  );
  function syncForm() {
    state.into = into.value;
    state.hybrid = `${beginningSelect.value}+${endingSelect.value}`;
    state.key = key.value;
    state.mode = mode.value;
    state.meter = meter.value;
    state.bars = Number(bars.value);
    state.cadence = cadence.value;
    state.firstCadence = firstCadence.value;
    beginningSelect.parentElement.hidden = state.into !== "hybrid";
    endingSelect.parentElement.hidden = state.into !== "hybrid";
    firstCadence.parentElement.hidden = state.into === "sentence";
  }
  for (const control of [into, beginningSelect, endingSelect, key, mode, meter, bars, cadence, firstCadence]) {
    control.addEventListener("input", syncForm);
  }
  syncForm();
  el.appendChild(form);

  /* Slots */
  el.appendChild(materialsElement("h2", "section-title", "Slots"));
  const pool = buildThemePoolSection(state, nameOf, (value) => {
    state.pool = value;
  });
  for (const slot of ThemesModel.SLOT_NAMES) {
    const row = materialsElement("div", "theme-slot");
    row.appendChild(materialsElement("h3", "section-title", `${ThemesModel.SLOT_LABELS[slot]}${slot === "basic" ? " (required)" : ""}`));
    const picker = buildThemeMotifPicker({
      role: "melodic",
      items: itemsOf,
      onChange: (value) => {
        state.slots = state.slots.filter((entry) => entry.slot !== slot);
        if (value) {
          state.slots.push({ slot, item_id: value.item_id, version: value.version });
        }
        pool.refresh();
      },
    });
    picker.setValue(state.slots.find((entry) => entry.slot === slot));
    row.appendChild(picker.el);
    el.appendChild(row);
  }

  el.appendChild(pool.el);
  pool.refresh();

  /* Accompaniment */
  el.appendChild(materialsElement("h2", "section-title", "Accompaniment"));
  const accompaniment = materialsElement("div", "upload-controls");
  const accKind = themeField(
    accompaniment,
    "Accompaniment",
    themeSelect(
      [
        { value: "none", label: "None" },
        { value: "pattern", label: "A pattern" },
        { value: "motive", label: "A motif" },
      ],
      state.accompaniment.kind,
    ),
  );
  const accPattern = themeField(accompaniment, "Pattern", themeSelect(ThemesModel.PATTERNS, state.accompaniment.pattern || "any"));
  const accPicker = buildThemeMotifPicker({
    role: "accompaniment",
    items: itemsOf,
    onChange: (value) => {
      state.accompaniment.item_id = value ? value.item_id : "";
      state.accompaniment.version = value ? value.version : 0;
    },
  });
  accPicker.setValue(state.accompaniment);
  function syncAccompaniment() {
    state.accompaniment.kind = accKind.value;
    state.accompaniment.pattern = accPattern.value;
    accPattern.parentElement.hidden = accKind.value !== "pattern";
    accPicker.el.hidden = accKind.value !== "motive";
  }
  accKind.addEventListener("input", syncAccompaniment);
  accPattern.addEventListener("input", syncAccompaniment);
  syncAccompaniment();
  el.append(accompaniment, accPicker.el);

  el.appendChild(buildThemeBriefSection(state));

  el.appendChild(problemsEl);

  function problems() {
    const list = ThemesModel.formProblems(state);
    problemsEl.textContent = list.join(" ");
    return list;
  }

  /* Saves the form as a new version; the answer or null when the form has problems. */
  async function save(note) {
    if (problems().length > 0) {
      return null;
    }
    const answer = await sendMaterialsRequest(ThemesModel.saveRequest(state, note));
    if (!answer.ok) {
      ctx.statusEl.classList.add("error");
      ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not save the theme");
    }
    return answer;
  }

  const saveRow = materialsElement("div", "upload-controls");
  const noteInput = themeInput("Version note (optional)", "");
  const saveButton = themeButton("Save as new version", async () => {
    const answer = await save(noteInput.value);
    if (answer && answer.ok) {
      ctx.saved(answer);
    }
  });
  const composeButton = themeButton("Compose", () => compose({}));
  const spinner = materialsElement("span", "theme-spinner");
  spinner.hidden = true;
  saveRow.append(noteInput, saveButton, composeButton, spinner);
  el.appendChild(saveRow);

  function setBusy(busy) {
    saveButton.disabled = busy;
    composeButton.disabled = busy;
    spinner.hidden = !busy;
    if (busy) {
      ctx.statusEl.textContent = "Composing...";
      ctx.statusEl.classList.remove("error");
    }
  }

  /* Saves the form first when it changed, then composes it into a new version. options: {update_uses}. */
  async function compose(options) {
    if (problems().length > 0) {
      return;
    }
    setBusy(true);
    const stored = ctx.body && ctx.body.theme ? ctx.body.theme.form : null;
    const changed =
      JSON.stringify(ThemesModel.bodyFromForm(state)) !== initialJson || !stored || typeof stored !== "object";
    if (changed) {
      const saved = await save("Edited before composing");
      if (!saved || !saved.ok) {
        setBusy(false);
        return;
      }
    }
    const answer = await sendMaterialsRequest(ThemesModel.composeRequest(state, options));
    setBusy(false);
    if (!answer.ok) {
      ctx.statusEl.style.whiteSpace = "pre-wrap";
      ctx.statusEl.classList.add("error");
      ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, "The theme could not be composed");
      return;
    }
    ctx.saved(answer);
  }
  return { el, state, problems, save, compose };
}
