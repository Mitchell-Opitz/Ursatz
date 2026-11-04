/* The Themes view's editor (P16, R-167): the form, slot motifs and
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
      options.onChange({ item_id: itemId, version: Number(version) }, current.body);
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

/* The pool section: the saved forms each motif has, by flag. entries() lists {label, item_id} of the motifs; it
 * defaults to the slots of the theme. */
function buildThemePoolSection(state, itemName, onPool, entries) {
  const motifs = entries || (() => state.slots.map((entry) => ({ label: ThemesModel.SLOT_LABELS[entry.slot], item_id: entry.item_id })));
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
    for (const entry of motifs()) {
      const li = materialsElement("li", "", `${entry.label}: ${itemName(entry.item_id)} · `);
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

/* The brief section: chips of what is picked, the picker, the motive switches, weight, tempo, check and seed.
 * options: {emptyText (shown without a brief), tempo (false when the tempo is set elsewhere)}. */
function buildThemeBriefSection(state, options) {
  const o = options || {};
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Brief"));
  const chipsEl = materialsElement("div", "theme-chips");
  const emptyEl = materialsElement("div", "piece-list-status", o.emptyText || "No brief: the theme is grown without a style.");
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

  const controls = materialsElement("div", "upload-controls");
  const weight = themeField(controls, "Weight", themeSelect(ThemesModel.WEIGHTS, state.weight));
  weight.addEventListener("input", () => {
    state.weight = weight.value;
  });
  if (o.tempo !== false) {
    const tempo = themeField(controls, "Tempo", themeInput("from the brief", state.tempo));
    tempo.addEventListener("input", () => {
      state.tempo = tempo.value;
    });
  }
  const seed = themeField(controls, "Seed", themeInput("random", state.seed));
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
  controls.appendChild(checkLabel);
  section.appendChild(controls);
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

/* Mounts a version's score in the score workspace, with the playback the backend builds from its MIDI file.
 * options: {playback, musicxml, subtitle} name the playback ("theme", "loop" or "ending"), the MusicXML file and the
 * subtitle (a theme's by default); {loop: {start, end}} repeats that region and {ending: {playback, musicxml,
 * bodyEnd}} adds Play ending with that version's files. */
async function themeMountScore(host, id, version, title, options) {
  const o = options || {};
  const files = (name) => ThemesModel.versionFileUrl(id, version, name);
  const playbackUrl = ThemesModel.versionPlaybackUrl(id, version, o.playback || "theme");
  if (host.themeWorkspace) {
    host.themeWorkspace.destroy();
  }
  let ending = null;
  if (o.ending) {
    try {
      const served = await fetch(ThemesModel.versionPlaybackUrl(id, version, o.ending.playback));
      const playback = served.ok ? await served.json() : null;
      if (playback) {
        ending = { musicxmlUrl: files(o.ending.musicxml), playback, bodyEnd: o.ending.bodyEnd };
      }
    } catch (error) {
      ending = null;
    }
  }
  host.innerHTML = "";
  host.themeWorkspace = ScoreWorkspace.mount(host, {
    title,
    subtitle: o.subtitle || `theme · v${version}`,
    musicxmlUrl: files(o.musicxml || "theme.musicxml"),
    playbackUrl,
    loop: o.loop || null,
    ending,
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
      ["Given chords", ThemesModel.chordsText(state)],
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
      ["Form", `${state.into}${state.into === "hybrid" ? ` (${state.hybrid})` : ""}, ${state.mode}, ${state.meter}, ${state.bars} bars, ${state.cadence}`],
      ...state.slots.map((entry) => [ThemesModel.SLOT_LABELS[entry.slot], `${nameOf(entry.item_id)} v${entry.version}`]),
      ["Given chords", ThemesModel.chordsText(state)],
      ["Brief", ThemesModel.briefLabel(ThemesModel.briefFromPicks(state.picks))],
    ]),
  );
  const result = buildThemeResult({ id, version: number, theme, nameOf, title });
  viewEl.appendChild(result || materialsElement("div", "piece-list-status", "This version was not composed."));
}

/* The accompaniment section: none, a pattern or (when options.motif is not false) a motif. state.accompaniment is
 * {kind, pattern, item_id, version}; itemsOf() lists the items for the motif picker. */
function buildThemeAccompanimentSection(state, itemsOf, options) {
  const withMotif = !options || options.motif !== false;
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", "Accompaniment"));
  const accompaniment = materialsElement("div", "upload-controls");
  const accKind = themeField(
    accompaniment,
    "Accompaniment",
    themeSelect(
      [
        { value: "none", label: "None" },
        { value: "pattern", label: "A pattern" },
        ...(withMotif ? [{ value: "motive", label: "A motif" }] : []),
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
  section.append(accompaniment, accPicker.el);
  return section;
}

/* A Use select beside a motif picker: "The motif" or one of its saved variants (rejected left out). itemsOf() lists the
 * items. onUse(value) gets {item_id, version} of the choice. Returns {el, setMotif(motif, keep)}: the motif the picker
 * holds ({item_id, version}, or null) and the {item_id, version} of the variant to select. */
function buildThemeUseSelect(itemsOf, onUse) {
  const select = materialsElement("select", "theme-use");
  select.title = "Use the motif or one of its saved variants";
  let motif = null;
  let choices = [];
  let loading = 0;

  function fill(keep) {
    select.innerHTML = "";
    const base = materialsElement("option", "", "The motif");
    base.value = "";
    select.appendChild(base);
    for (const choice of choices) {
      const option = materialsElement("option", "", choice.label);
      option.value = choice.value;
      option.title = choice.title;
      select.appendChild(option);
    }
    if (keep && !choices.some((choice) => choice.value === keep.item_id)) {
      const option = materialsElement("option", "", "Saved variant");
      option.value = keep.item_id;
      select.appendChild(option);
      choices = [...choices, { value: keep.item_id, version: Number(keep.version), label: "Saved variant", title: "" }];
    }
    select.value = keep ? keep.item_id : "";
  }

  async function setMotif(next, keep) {
    motif = next;
    choices = [];
    const ticket = ++loading;
    fill(keep);
    if (!next || !next.item_id) {
      return;
    }
    const answer = await sendMaterialsRequest(VariantTree.treeRequest(next.item_id));
    if (ticket !== loading || !answer.ok) {
      return;
    }
    choices = ThemesModel.variantChoices(VariantTree.treeRows(answer.data.tree, false, "", "").rows);
    fill(keep);
  }

  select.addEventListener("change", () => {
    const choice = choices.find((entry) => entry.value === select.value);
    onUse(choice ? { item_id: choice.value, version: choice.version } : motif);
  });
  return { el: select, setMotif };
}

/* A motif picker with its Use select. stored is {item_id, version} (a motif or a variant) or undefined; set(value) gets
 * {item_id, version} of the choice or null when cleared; picked(value, motifBody) is called when the picker announces a
 * motif. Returns the element. */
function buildThemeMotifUse(itemsOf, stored, set, picked) {
  const use = buildThemeUseSelect(itemsOf, set);
  const picker = buildThemeMotifPicker({
    role: "melodic",
    items: itemsOf,
    onChange: (value, motifBody) => {
      set(value);
      use.setMotif(value, null);
      if (picked) {
        picked(value, motifBody);
      }
    },
  });
  if (stored) {
    const root = ThemesModel.rootMotif(itemsOf(), stored.item_id);
    if (root === stored.item_id) {
      picker.setValue(stored);
      use.setMotif(stored, null);
    } else {
      const rootItem = itemsOf().find((item) => item.id === root);
      const motif = { item_id: root, version: rootItem ? Number(rootItem.current_version) : 1 };
      picker.setValue(motif);
      use.setMotif(motif, stored);
    }
  }
  const el = materialsElement("div", "theme-slot-row");
  el.append(picker.el, use.el);
  return el;
}

/* The slot rows of the Structure tab: one motif picker and Use select per slot, kept in state.slots (a variant stores
 * its own item id and version). basicSet(motifBody, hadBasic) is called when the basic slot gets a motif. Below them
 * More slots (closed) gives single ideas a motif of their own, kept in state.ideas. Returns the section element;
 * refreshSlots() shows the slots and ideas the form has. */
function buildThemeSlotsSection(state, itemsOf, basicSet) {
  const section = materialsElement("section", "panel materials-editor theme-editor");
  section.appendChild(materialsElement("h2", "section-title", "Slots"));
  const rows = {};
  for (const slot of ThemesModel.SLOT_NAMES) {
    const row = materialsElement("div", "theme-slot");
    row.appendChild(materialsElement("h3", "section-title", `${ThemesModel.SLOT_LABELS[slot]}${slot === "basic" ? " (required)" : ""}`));
    let hadBasic = false;
    const set = (value) => {
      hadBasic = state.slots.some((entry) => entry.slot === "basic");
      state.slots = state.slots.filter((entry) => entry.slot !== slot);
      if (value) {
        state.slots.push({ slot, item_id: value.item_id, version: value.version });
      }
    };
    row.appendChild(
      buildThemeMotifUse(itemsOf, state.slots.find((entry) => entry.slot === slot), set, (value, motifBody) => {
        if (slot === "basic" && value) {
          basicSet(motifBody, hadBasic);
        }
      }),
    );
    rows[slot] = row;
    section.appendChild(row);
  }
  const more = materialsElement("details", "theme-more-slots");
  more.appendChild(materialsElement("summary", "section-title", "More slots"));
  const ideasHost = materialsElement("div");
  more.appendChild(ideasHost);
  section.appendChild(more);

  /* One row per idea of the form except the basic idea; "From <slot>" until it has a motif of its own. */
  let drawn = "";
  function drawIdeas() {
    const key = ThemesModel.formIdeas(state).map((entry) => entry.idea).join("|");
    if (key === drawn) {
      return;
    }
    drawn = key;
    ideasHost.innerHTML = "";
    for (const { idea, slot } of ThemesModel.formIdeas(state)) {
      if (idea === "basic idea") {
        continue;
      }
      const row = materialsElement("div", "theme-slot");
      const heading = materialsElement("h3", "section-title");
      const own = () => state.ideas.find((entry) => entry.idea === idea);
      const title = () => {
        heading.textContent = `${idea}: ${own() ? "Own motif" : `From ${ThemesModel.SLOT_LABELS[slot].toLowerCase()}`}`;
      };
      const set = (value) => {
        state.ideas = state.ideas.filter((entry) => entry.idea !== idea);
        if (value) {
          state.ideas.push({ idea, item_id: value.item_id, version: value.version });
        }
        title();
      };
      title();
      row.append(heading, buildThemeMotifUse(itemsOf, own(), set, null));
      ideasHost.appendChild(row);
    }
  }
  /* Shows the rows of the slots the form has, and the ideas of its pairing. */
  section.refreshSlots = () => {
    const shown = ThemesModel.formSlots(state);
    for (const slot of ThemesModel.SLOT_NAMES) {
      rows[slot].hidden = !shown.includes(slot);
    }
    drawIdeas();
  };
  section.refreshSlots();
  return section;
}

/* The Advanced details of the Theme tab (closed by default): accompaniment, brief with weight and switches, seed
 * and check. The tempo is a field of the tab itself. */
/* The chord row: one text field per bar of the theme, empty bars left to the Composer. Returns {element, refresh()}. */
function buildThemeChordRow(state) {
  const element = materialsElement("div", "theme-chords");
  element.appendChild(materialsElement("h3", "section-title", "Chords"));
  const row = materialsElement("div", "upload-controls");
  const fields = [];
  for (let bar = 0; bar < ThemesModel.CHORD_BARS; bar++) {
    const input = themeInput("Composer", state.chords[bar]);
    input.addEventListener("input", () => {
      state.chords[bar] = input.value;
    });
    fields.push(themeField(row, `Bar ${bar + 1}`, input).parentElement);
  }
  element.appendChild(row);
  const hint = materialsElement("div", "materials-hint");
  element.appendChild(hint);
  function refresh() {
    fields.forEach((field, bar) => {
      field.hidden = bar >= state.bars;
      field.querySelector("input").value = state.chords[bar];
    });
    hint.textContent = ThemesModel.chordHint(state.meter);
  }
  refresh();
  return { element, refresh };
}

function buildThemeAdvancedSection(state, itemsOf) {
  const section = materialsElement("details", "panel materials-editor theme-editor theme-advanced");
  section.appendChild(materialsElement("summary", "section-title", "Advanced"));
  section.appendChild(buildThemeAccompanimentSection(state, itemsOf));
  section.appendChild(buildThemeBriefSection(state, { tempo: false }));
  return section;
}

/*
 * The theme editor, in the sections the tabs mount. ctx: {id, item, body, subjects, usesStatus, items (all items),
 * status (a tab status writer), nameInput, projectSelect, saved()}.
 * Returns {state, problems(), save(note), compose(options), sections: {notices, theme, structure, slots, result
 * (the save row)}}.
 */
function buildThemeEditor(ctx) {
  const state = ThemesModel.formFromBody(ctx.body, ctx.id);
  const initialJson = JSON.stringify(ThemesModel.bodyFromForm(state));
  const status = ctx.status;
  const problemsEl = materialsElement("div", "materials-field-error");
  const itemsOf = () => ctx.items;
  const sections = {};

  /* Sources with a newer version */
  const notices = materialsElement("div");
  for (const use of ThemesModel.updatedUses({ uses_status: ctx.usesStatus })) {
    const notice = materialsElement("div", "piece-list-status");
    notice.appendChild(materialsElement("span", "", `${use.label} `));
    notice.appendChild(themeButton("Update and compose", () => compose({ update_uses: true })));
    notice.appendChild(themeButton("Keep", () => notice.remove()));
    notices.appendChild(notice);
  }
  sections.notices = notices;

  /* Theme tab: mode, meter and tempo */
  const theme = materialsElement("section", "panel materials-editor theme-editor");
  theme.appendChild(materialsElement("h2", "section-title", "Theme"));
  const main = materialsElement("div", "upload-controls");
  const mode = themeField(main, "Mode", themeSelect(ThemesModel.MODES, state.mode));
  const meter = themeField(main, "Meter", themeSelect(ThemesModel.METERS, state.meter));
  const tempo = themeField(main, "Tempo", themeInput("from the brief", state.tempo));
  tempo.addEventListener("input", () => {
    state.tempo = tempo.value;
  });
  theme.appendChild(main);
  sections.theme = theme;

  /* Structure tab: the form */
  const structure = materialsElement("section", "panel materials-editor theme-editor");
  structure.appendChild(materialsElement("h2", "section-title", "Form"));
  const form = materialsElement("div", "upload-controls");
  const into = themeField(form, "Into", themeSelect(ThemesModel.INTOS, state.into));
  const [beginning, ending] = state.hybrid.split("+");
  const beginningSelect = themeField(form, "Beginning", themeSelect(ThemesModel.BEGINNINGS, beginning));
  const endingSelect = themeField(form, "Ending", themeSelect(ThemesModel.ENDINGS, ending));
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
    state.mode = mode.value;
    state.meter = meter.value;
    state.bars = Number(bars.value);
    const dropped = chordRow ? ThemesModel.dropChords(state) : [];
    if (dropped.length > 0) {
      status.show(`Bars changed: the chords of ${dropped.length === 1 ? "bar" : "bars"} ${dropped.join(", ")} were removed.`);
    }
    state.cadence = cadence.value;
    state.firstCadence = firstCadence.value;
    beginningSelect.parentElement.hidden = state.into !== "hybrid";
    endingSelect.parentElement.hidden = state.into !== "hybrid";
    firstCadence.parentElement.hidden = state.into === "sentence";
    if (sections.slots) {
      sections.slots.refreshSlots();
    }
    if (chordRow) {
      chordRow.refresh();
    }
  }
  let chordRow = null;
  for (const control of [into, beginningSelect, endingSelect, meter, bars, cadence, firstCadence]) {
    control.addEventListener("input", syncForm);
  }
  mode.addEventListener("input", () => {
    state.modeChosen = true;
    syncForm();
  });
  syncForm();
  structure.appendChild(form);
  sections.structure = structure;

  /* Structure tab: the slots. The first basic motif sets the mode (R-205). */
  sections.slots = buildThemeSlotsSection(state, itemsOf, (motifBody, hadBasic) => {
    if (ThemesModel.applyBasicMode(state, hadBasic, motifBody)) {
      mode.value = state.mode;
    }
  });

  /* Structure tab: the chords, under the slots. */
  chordRow = buildThemeChordRow(state);
  sections.slots.appendChild(chordRow.element);

  sections.advanced = buildThemeAdvancedSection(state, itemsOf);

  /* The Composer's checks of the values (meter, bars, hybrid, tempo, seed, chords...) come from the validate route. */
  const validation = attachFormValidation({
    roots: [theme, structure, sections.slots, sections.advanced],
    build: () => ThemesModel.validateRequest(state),
    field: ThemesModel.fieldForPath,
    fields: () => ({
      into,
      hybrid: beginningSelect,
      mode,
      meter,
      bars,
      cadence,
      firstCadence,
      tempo,
      chords: chordRow.element,
    }),
  });

  function problems() {
    const list = ThemesModel.formProblems(state);
    problemsEl.textContent = list.join(" ");
    return list;
  }

  /* Whether the form differs from the stored version, or the theme has no stored form yet. */
  function bodyChanged() {
    const stored = ctx.body && ctx.body.theme ? ctx.body.theme.form : null;
    return JSON.stringify(ThemesModel.bodyFromForm(state)) !== initialJson || !stored || typeof stored !== "object";
  }

  /* Saves the form as a new version; the answer or null when the form has problems. */
  async function save(note) {
    if (problems().length > 0) {
      return null;
    }
    const answer = await sendMaterialsRequest(ThemesModel.saveRequest(state, note));
    if (!answer.ok) {
      status.show(MaterialsModel.errorText(answer.data, "Could not save the theme"), true);
    }
    return answer;
  }

  /* The one Save row of the page: the note, Save, Compose. */
  const saveRow = materialsElement("div", "upload-controls button-row");
  const noteInput = themeInput("What changed? (required)", "");
  noteInput.size = 40;
  const saveButton = themeButton("Save", () => saveAll());
  saveButton.classList.add("button-primary");
  const composeButton = themeButton("Compose", () => compose({}));
  const spinner = materialsElement("span", "theme-spinner");
  spinner.hidden = true;
  saveRow.append(problemsEl, validation.element, noteInput, saveButton, composeButton, spinner);
  sections.result = saveRow;

  function setBusy(busy) {
    saveButton.disabled = busy;
    composeButton.disabled = busy;
    spinner.hidden = !busy;
  }

  /* Name and project go through the item update, a changed form writes a version with the note (R-210). */
  async function saveAll() {
    status.clear();
    const fields = {};
    const typedName = ctx.nameInput.value.trim();
    if (typedName !== "" && typedName !== ctx.item.name) {
      fields.name = typedName;
    }
    if (ctx.projectSelect.value !== ctx.item.project_id) {
      fields.project_id = ctx.projectSelect.value;
    }
    const changed = bodyChanged();
    const decision = MaterialsModel.motifSaveDecision(
      { name: "name" in fields, project: "project_id" in fields, body: changed },
      noteInput.value,
    );
    if (decision.kind !== "save") {
      status.show(decision.message, true);
      return;
    }
    if (changed && problems().length > 0) {
      status.show(problems().join(" "), true);
      return;
    }
    setBusy(true);
    status.show("Saving...");
    if (changed) {
      const saved = await save(noteInput.value);
      if (!saved || !saved.ok) {
        setBusy(false);
        return;
      }
    }
    if (Object.keys(fields).length > 0) {
      const renamed = await sendMaterialsRequest(MaterialsModel.buildUpdateItemRequest(ctx.id, fields));
      if (!renamed.ok) {
        setBusy(false);
        status.show(MaterialsModel.errorText(renamed.data, "Could not save"), true);
        return;
      }
    }
    ctx.saved();
  }

  /* Saves the form first when it changed (the note is required), then composes it into a new version.
   * options: {update_uses}. */
  async function compose(options) {
    status.clear();
    if (problems().length > 0) {
      status.show(problems().join(" "), true);
      return;
    }
    const changed = bodyChanged();
    if (changed && noteInput.value.trim() === "") {
      status.show("Give a version note.", true);
      return;
    }
    setBusy(true);
    status.show("Composing...");
    if (changed) {
      const saved = await save(noteInput.value);
      if (!saved || !saved.ok) {
        setBusy(false);
        return;
      }
    }
    const answer = await sendMaterialsRequest(ThemesModel.composeRequest(state, options));
    setBusy(false);
    if (!answer.ok) {
      status.show(MaterialsModel.errorText(answer.data, "The theme could not be composed"), true);
      return;
    }
    ctx.saved();
  }
  return { state, problems, save, compose, sections };
}
