/* Materials views (P12): request builders and response shaping for projects
 * and items, item rows, version labels and the current project filter. No
 * DOM; materials-view.js renders from these. */
(function (root) {
  const ALL_PROJECTS = "all";
  const FILTER_KEY = "ursatz.materials.project";
  const TYPES = [
    { type: "motif", label: "Motifs", singular: "motif" },
    { type: "theme", label: "Themes", singular: "theme" },
    { type: "piece", label: "Pieces", singular: "piece" },
  ];

  function typeInfo(type) {
    return TYPES.find((entry) => entry.type === type) || null;
  }

  function jsonRequest(method, url, body) {
    const request = { method, url };
    if (body !== undefined) {
      request.headers = { "Content-Type": "application/json" };
      request.body = JSON.stringify(body);
    }
    return request;
  }

  function buildProjectListRequest() {
    return jsonRequest("GET", "/api/materials/projects");
  }

  function buildCreateProjectRequest(name, description) {
    const body = { name: String(name || "").trim() };
    if (description) {
      body.description = String(description);
    }
    return jsonRequest("POST", "/api/materials/projects", body);
  }

  function buildRenameProjectRequest(id, name) {
    return jsonRequest("PATCH", `/api/materials/projects/${encodeURIComponent(id)}`, {
      name: String(name || "").trim(),
    });
  }

  function buildDeleteProjectRequest(id) {
    return jsonRequest("DELETE", `/api/materials/projects/${encodeURIComponent(id)}`);
  }

  /* The items of one type, narrowed to a project unless the filter is "all". */
  function buildItemListRequest(type, projectFilter) {
    let url = `/api/materials/items?type=${encodeURIComponent(type)}`;
    if (projectFilter && projectFilter !== ALL_PROJECTS) {
      url += `&project=${encodeURIComponent(projectFilter)}`;
    }
    return jsonRequest("GET", url);
  }

  function buildItemGetRequest(id) {
    return jsonRequest("GET", `/api/materials/items/${encodeURIComponent(id)}`);
  }

  function buildVersionGetRequest(id, number) {
    return jsonRequest("GET", `/api/materials/items/${encodeURIComponent(id)}/versions/${Number(number)}`);
  }

  /* The version 1 body of a new item; the editors (motif B7, later themes and
   * pieces) replace it. A motif needs a non-empty note list, so it starts
   * with one quarter note. */
  function newItemBody(type) {
    if (type === "motif") {
      return {
        uses: [],
        motif: {
          role: "melodic",
          form: "full",
          notes: "C4:q",
          meter: "4/4",
          key: null,
          pickup: "0",
          harmony: null,
          features: [],
          analysis: null,
          derived: null,
        },
      };
    }
    if (type === "theme") {
      return { uses: [], theme: { form: null, slots: [] } };
    }
    return { uses: [], piece: { structure: null } };
  }

  function buildCreateItemRequest(type, name, projectId) {
    return jsonRequest("POST", "/api/materials/items", {
      type,
      name: String(name || "").trim(),
      project_id: projectId,
      body: newItemBody(type),
    });
  }

  /* name and/or project_id; empty values are left out. */
  function buildUpdateItemRequest(id, fields) {
    const body = {};
    const f = fields || {};
    if (typeof f.name === "string" && f.name.trim() !== "") {
      body.name = f.name.trim();
    }
    if (typeof f.project_id === "string" && f.project_id !== "") {
      body.project_id = f.project_id;
    }
    return jsonRequest("PATCH", `/api/materials/items/${encodeURIComponent(id)}`, body);
  }

  /* The error text of a failed answer, or the fallback. */
  function errorText(data, fallback) {
    if (data && typeof data.error === "string" && data.error !== "") {
      return data.error;
    }
    return fallback || "Request failed";
  }

  function projectsFrom(data) {
    return data && Array.isArray(data.projects) ? data.projects : [];
  }

  function projectName(projects, id) {
    const found = (projects || []).find((project) => project.id === id);
    return found ? found.name : "Unknown project";
  }

  /* "All projects" first, then each project in the order given. */
  function projectOptions(projects) {
    const options = [{ value: ALL_PROJECTS, label: "All projects" }];
    for (const project of projects || []) {
      options.push({ value: project.id, label: project.name });
    }
    return options;
  }

  /* The remembered filter, or "all" when it is unset or names a project that
   * no longer exists. */
  function resolveFilter(saved, projects) {
    if (saved && (projects || []).some((project) => project.id === saved)) {
      return saved;
    }
    return ALL_PROJECTS;
  }

  function readFilter() {
    try {
      return localStorage.getItem(FILTER_KEY) || ALL_PROJECTS;
    } catch (err) {
      return ALL_PROJECTS;
    }
  }

  function writeFilter(value) {
    try {
      localStorage.setItem(FILTER_KEY, value);
    } catch (err) {
      // Storage unavailable: the filter just is not remembered.
    }
  }

  /* The project a New item goes into: the filtered project, else the first. */
  function defaultProject(filter, projects) {
    if (filter !== ALL_PROJECTS && (projects || []).some((project) => project.id === filter)) {
      return filter;
    }
    return projects && projects.length > 0 ? projects[0].id : "";
  }

  function versionLabel(item) {
    const current = Number(item && item.current_version) || 0;
    return current > 0 ? `v${current}` : "-";
  }

  function dateLabel(stamp) {
    const text = String(stamp || "");
    return text.length >= 10 ? text.slice(0, 10) : text || "-";
  }

  /* Table rows {id, name, project, version, updated} newest first. */
  function itemRows(items, projects) {
    return (Array.isArray(items) ? items : [])
      .map((item) => ({
        id: item.id,
        name: item.name,
        project: projectName(projects, item.project_id),
        version: versionLabel(item),
        updated: dateLabel(item.updated_at),
        stamp: String(item.updated_at || ""),
      }))
      .sort((a, b) => (a.stamp < b.stamp ? 1 : a.stamp > b.stamp ? -1 : 0))
      .map(({ stamp, ...row }) => row);
  }

  /* Rows {number, date, note, current} for the versions list, newest first. */
  function versionRows(item) {
    const versions = item && Array.isArray(item.versions) ? item.versions : [];
    return versions
      .map((version) => ({
        number: version.number,
        date: dateLabel(version.created_at),
        note: version.note || "",
        current: version.number === item.current_version,
      }))
      .sort((a, b) => b.number - a.number);
  }

  /* Rows for the Uses section from the item answer's uses_status. */
  function useRows(statuses, projects) {
    return (Array.isArray(statuses) ? statuses : []).map((use) => {
      const name = use.name || use.item_id;
      return {
        item_id: use.item_id,
        name,
        type: use.type || "",
        project: use.project_id ? projectName(projects, use.project_id) : "",
        version: Number(use.version),
        current: Number(use.current_version),
        label: `${name} v${use.version}`,
        missing: Boolean(use.missing),
        stale: !use.missing && Number(use.current_version) > Number(use.version),
        note: use.missing
          ? "source deleted"
          : Number(use.current_version) > Number(use.version)
            ? `source updated to v${use.current_version}`
            : "",
      };
    });
  }

  /* True when any use of an item has a newer source version. */
  function hasUpdatedUse(statuses) {
    return useRows(statuses, []).some((row) => row.stale);
  }

  /* Moves a use to the source's current version (writes a new version). */
  function buildUpdateUseRequest(id, sourceId) {
    return jsonRequest(
      "POST",
      `/api/materials/items/${encodeURIComponent(id)}/uses/${encodeURIComponent(sourceId)}/update`,
      {},
    );
  }

  /* Rows for the Used by section from the item answer's used_by: the items
   * whose current version uses this one, and the version they use. */
  function usedByRows(users, projects) {
    return (Array.isArray(users) ? users : []).map((user) => ({
      id: user.id,
      name: user.name || user.id,
      type: user.type || "",
      project: user.project_id ? projectName(projects, user.project_id) : "",
      version: Number(user.version),
      label: `${user.name || user.id} (${user.type || "item"}) uses v${user.version}`,
    }));
  }

  /* force true adds ?force=1, which deletes an item other items use. */
  function buildDeleteItemRequest(id, force) {
    return jsonRequest("DELETE", `/api/materials/items/${encodeURIComponent(id)}${force ? "?force=1" : ""}`);
  }

  /* The warning for a refused delete (status 409 with used_by), or null when
   * the answer is not that refusal. */
  function deleteWarning(name, status, data) {
    if (status !== 409 || !data || !Array.isArray(data.used_by)) {
      return null;
    }
    const users = data.used_by.map((user) => user.name || user.id);
    const count = users.length;
    return {
      users,
      message: `${name} is used by ${count} item${count === 1 ? "" : "s"}: ${users.join(", ")}. Their copies stay, and their uses will report the source as deleted. Delete anyway?`,
    };
  }

  function buildAllItemsRequest() {
    return jsonRequest("GET", "/api/materials/items");
  }

  /* Items to pick from, by name (case-insensitive part), type and project
   * ("all" or empty for any); excluded ids are left out. */
  function pickerItems(items, projects, filter) {
    const f = filter || {};
    const query = String(f.query || "").trim().toLowerCase();
    const skip = new Set(f.exclude || []);
    return (Array.isArray(items) ? items : [])
      .filter(
        (item) =>
          !skip.has(item.id) &&
          (!f.type || f.type === "all" || item.type === f.type) &&
          (!f.project || f.project === ALL_PROJECTS || item.project_id === f.project) &&
          (query === "" || String(item.name).toLowerCase().includes(query)),
      )
      .map((item) => ({
        id: item.id,
        name: item.name,
        type: item.type,
        project: projectName(projects, item.project_id),
        version: Number(item.current_version),
        label: `${item.name} (${item.type}, ${projectName(projects, item.project_id)}) v${item.current_version}`,
      }))
      .sort((a, b) => a.name.localeCompare(b.name));
  }

  /* A copy of the version body with a use added (at the item's current
   * version; an existing use of it is replaced) or removed. The backend
   * fills in each use's copy. */
  function bodyWithUse(body, sourceId, version, add) {
    const next = JSON.parse(JSON.stringify(body || { uses: [] }));
    const uses = (Array.isArray(next.uses) ? next.uses : []).filter((use) => use.item_id !== sourceId);
    if (add) {
      uses.push({ item_id: sourceId, version: Number(version) });
    }
    next.uses = uses;
    return next;
  }

  /* A new version of an item with the given body and an optional note. */
  function buildAddVersionRequest(id, body, note) {
    const request = { body };
    if (note && String(note).trim() !== "") {
      request.note = String(note).trim();
    }
    return jsonRequest("POST", `/api/materials/items/${encodeURIComponent(id)}/versions`, request);
  }

  const MOTIF_TABS = [
    { id: "motif", label: "Motif" },
    { id: "variants", label: "Variants" },
    { id: "used-by", label: "Used by" },
    { id: "other", label: "Other" },
  ];
  const MOTIF_TAB_KEY = "ursatz.motif.tab";

  /* The remembered motif tab id, "motif" when unset, unknown or storage is unavailable. */
  function readMotifTab() {
    try {
      const saved = localStorage.getItem(MOTIF_TAB_KEY);
      return MOTIF_TABS.some((tab) => tab.id === saved) ? saved : MOTIF_TABS[0].id;
    } catch (err) {
      return MOTIF_TABS[0].id;
    }
  }

  function writeMotifTab(id) {
    try {
      localStorage.setItem(MOTIF_TAB_KEY, id);
    } catch (err) {
      // Storage unavailable: the tab just is not remembered.
    }
  }

  const COMPOSER_MISSING_TEXT =
    "The Composer is not available: motifs save without analysis, notation or variants. Rebuild the GUI (README, Building).";

  /* Whether the Composer answered the status request as available. */
  function composerAvailable(answer) {
    return Boolean(answer && answer.ok && answer.data && answer.data.available);
  }

  /* Analyzes the current version of a motif again; the answer is the new version. */
  function buildReanalyzeRequest(id) {
    return jsonRequest("POST", `/api/materials/items/${encodeURIComponent(id)}/reanalyze`, {});
  }

  const NOTE_HINT = "Pitch:duration, e.g. D4:q F4:e A4:e D5:h. r is a rest, ~ ties to the next note, | is ignored. Durations: w h q e s t, with up to two dots.";
  const MOTIF_ROLES = [
    { value: "melodic", label: "Melodic" },
    { value: "accompaniment", label: "Accompaniment" },
  ];
  const RHYTHM_HINT = "Rhythm only, e.g. x:q x:e x:e x:h. x is a note, r a rest, ~ ties to the next note, | is ignored. Durations: w h q e s t, with up to two dots.";
  const CONTOUR_HINT = "Contour only: ranks separated by spaces, e.g. 0 2 1 3. 0 is the lowest, equal ranks are the same height, no durations.";
  const MOTIF_FORMS = [
    { value: "full", label: "Full", enabled: true },
    { value: "rhythm", label: "Rhythm only", enabled: true },
    { value: "contour", label: "Contour only", enabled: true },
  ];

  const SEGMENTS_HINT = "Named note ranges, for example head and tail. Liquidation and motive-forms keep or drop them.";
  const HARMONY_HINT = "Own chords, e.g. I V6@2 I@3: upper case Roman numerals (b or # before for an altered root; figures 6, 64, 7, 65, 43, 42) and @ the onset in beats. The first chord needs no onset. Up to 8 chords; empty means none.";
  const HARMONY_MAX_CHARS = 200;

  /* The syntax hint under the note list for a motif form. */
  function formHint(form) {
    return form === "rhythm" ? RHYTHM_HINT : form === "contour" ? CONTOUR_HINT : NOTE_HINT;
  }

  const MOTIF_MODES = ["major", "minor"];
  const TONIC_HINT = "Write the notes with C as the tonic. A piece sets the real key.";

  /* The mode of a stored key text ("D minor" gives "minor"); "major" when there is none. */
  function keyMode(key) {
    const match = /^[A-Ga-g][#b]? (major|minor)$/i.exec(String(key || "").trim());
    return match ? match[1].toLowerCase() : "major";
  }

  /* The editor's field values for a motif body (null for a new motif). legacyKey is the stored key when its tonic is
   * not C (a motif written before motifs moved to C, R-179), else null. */
  function motifFormValues(body) {
    const motif = body && body.motif ? body.motif : {};
    const storedKey = typeof motif.key === "string" ? motif.key.trim() : "";
    return {
      role: motif.role === "accompaniment" ? "accompaniment" : "melodic",
      form: motif.form || "full",
      notes: typeof motif.notes === "string" ? motif.notes : "",
      meter: typeof motif.meter === "string" ? motif.meter : "4/4",
      mode: keyMode(storedKey),
      legacyKey: storedKey !== "" && !/^C (major|minor)$/.test(storedKey) ? storedKey : null,
      pickup: typeof motif.pickup === "string" ? motif.pickup : "0",
      harmony: typeof motif.harmony === "string" ? motif.harmony : "",
      features: Array.isArray(motif.features)
        ? motif.features.map((feature) => ({
            label: String(feature.label),
            first: Number(feature.first) + 1,
            last: Number(feature.last) + 1,
          }))
        : [],
    };
  }

  /* The MIDI import route (POST, the file as the body). */
  function buildImportMidiRequest(file) {
    return {
      method: "POST",
      url: "/api/materials/motifs/import",
      headers: { "Content-Type": "audio/midi" },
      body: file,
    };
  }

  /* The editor's field values for an import answer {notes_text, meter, key, moved_from, pickup}: a full motif. The
   * backend has already moved the notes to C. */
  function importToFormValues(answer) {
    const a = answer || {};
    return {
      form: "full",
      notes: typeof a.notes_text === "string" ? a.notes_text : "",
      meter: typeof a.meter === "string" ? a.meter : "4/4",
      mode: keyMode(a.key),
      pickup: typeof a.pickup === "string" ? a.pickup : "0",
    };
  }

  /* The to-c POST for a stored motif body: its degrees written in C in the stored mode. Null when the motif holds no
   * degrees (it was never analyzed). */
  function toCRequest(body, mode) {
    const motif = body && body.motif ? body.motif : {};
    if (typeof motif.degrees !== "string" || motif.degrees === "") {
      return null;
    }
    return jsonRequest("POST", "/api/materials/motifs/to-c", {
      degrees: motif.degrees,
      mode: MOTIF_MODES.includes(mode) ? mode : "major",
      meter: typeof motif.meter === "string" ? motif.meter : "4/4",
      pickup: typeof motif.pickup === "string" && motif.pickup !== "" ? motif.pickup : "0",
    });
  }

  /* The notice for a motif stored on another tonic. */
  function legacyNotice(legacyKey) {
    return `Written in ${legacyKey} before motifs moved to C.`;
  }

  /* The line under the upload for an import answer. */
  function importNotice(answer) {
    const a = answer || {};
    if (typeof a.moved_from === "string" && a.moved_from !== "") {
      return `Moved from ${a.moved_from} to C ${keyMode(a.key)}.`;
    }
    return a.key_found === false ? "No key found: read as C major." : "Filled from the MIDI file.";
  }

  /* Problems with the form values as {field, message}; empty when it can be
   * sent. The Composer judges the note list itself. */
  function motifFormProblems(values) {
    const problems = [];
    if (values.legacyKey) {
      problems.push({ field: "notes", message: "Rewrite the motif in C before saving." });
    } else if (String(values.notes || "").trim() === "") {
      problems.push({ field: "notes", message: "Enter at least one note." });
    }
    if (!/^\d{1,2}\/\d{1,2}$/.test(String(values.meter || "").trim())) {
      problems.push({ field: "meter", message: "Meter looks like 4/4." });
    }
    if (!/^\d{1,2}(\/\d{1,2})?$/.test(String(values.pickup || "").trim())) {
      problems.push({ field: "pickup", message: "Pickup is beats as 0, 1 or 1/2." });
    }
    if (Array.from(String(values.harmony || "")).length > HARMONY_MAX_CHARS) {
      problems.push({ field: "harmony", message: `Harmony is at most ${HARMONY_MAX_CHARS} characters.` });
    }
    for (const message of featureProblems(values.features)) {
      problems.push({ field: "features", message });
    }
    return problems;
  }

  /* The editor field a Composer refusal belongs to: harmony for a chord problem, features for a feature one, else the
   * note list. */
  function errorField(message) {
    const text = String(message || "");
    if (/harmony|chord|numeral|token \d+ \(column/i.test(text)) {
      return "harmony";
    }
    return /feature|segment/i.test(text) ? "features" : "notes";
  }

  /* Messages for the feature rows (1-based first and last note). */
  function featureProblems(features) {
    const messages = [];
    const seen = new Set();
    (features || []).forEach((feature, index) => {
      const row = `Segment ${index + 1}`;
      const label = String(feature.label || "").trim();
      if (label === "" || Array.from(label).length > 8) {
        messages.push(`${row}: the label is 1 to 8 characters.`);
      } else if (seen.has(label)) {
        messages.push(`${row}: the label ${label} is used twice.`);
      }
      seen.add(label);
      const first = Number(feature.first);
      const last = Number(feature.last);
      if (!Number.isInteger(first) || !Number.isInteger(last) || first < 1 || last < first) {
        messages.push(`${row}: first and last are note numbers from 1, first not after last.`);
      }
    });
    return messages;
  }

  /* An empty feature row to add. */
  function newFeatureRow(features) {
    const used = new Set((features || []).map((feature) => feature.label));
    let label = "a";
    for (let code = 97; code <= 122 && used.has(label); code++) {
      label = String.fromCharCode(code + 1);
    }
    return { label, first: 1, last: 1 };
  }

  /* The version body for the form values. `previous` is the body being
   * edited: its uses and derived data carry over. The backend fills in the
   * analysis and the degrees (they are sent back as they were read). */
  function buildMotifBody(values, previous) {
    const before = previous || {};
    const motif = before.motif || {};
    return {
      uses: Array.isArray(before.uses) ? before.uses : [],
      motif: {
        role: values.role,
        form: values.form,
        notes: String(values.notes).trim(),
        meter: String(values.meter).trim(),
        key: `C ${MOTIF_MODES.includes(values.mode) ? values.mode : "major"}`,
        pickup: String(values.pickup || "").trim() === "" ? "0" : String(values.pickup).trim(),
        harmony: String(values.harmony || "").trim() === "" ? null : String(values.harmony).trim(),
        features: (values.features || []).map((feature) => ({
          label: String(feature.label).trim(),
          first: Number(feature.first) - 1,
          last: Number(feature.last) - 1,
        })),
        analysis: null,
        degrees: typeof motif.degrees === "string" ? motif.degrees : null,
        derived: motif.derived || null,
      },
    };
  }

  const KEY_TONICS = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];

  /* The POST that renders a motif body to MusicXML and playback in the motif's own key. */
  function renderRequest(body) {
    const motif = body && body.motif ? body.motif : {};
    const request = {
      form: motif.form || "full",
      degrees: typeof motif.degrees === "string" ? motif.degrees : "",
      meter: typeof motif.meter === "string" ? motif.meter : "4/4",
      pickup: typeof motif.pickup === "string" && motif.pickup !== "" ? motif.pickup : "0",
      role: motif.role === "accompaniment" ? "accompaniment" : "melodic",
    };
    if (typeof motif.key === "string" && motif.key !== "") {
      request.key = motif.key;
    }
    if (typeof motif.harmony === "string" && motif.harmony !== "") {
      request.harmony = motif.harmony;
    }
    if (Array.isArray(motif.features) && motif.features.length > 0) {
      request.features = motif.features.map((feature) => ({ label: feature.label, first: feature.first, last: feature.last }));
    }
    return jsonRequest("POST", "/api/materials/render", request);
  }

  const RANDOM_MODES = ["major", "minor"];
  const RANDOM_DEFAULT_COUNT = 6;
  const RANDOM_MAX_COUNT = 16;

  /* The random motifs POST for the dialog values {mode, meter, form, style_id, count, seed}. The key is always C in
   * the chosen mode (R-179; without a key the Composer gives A minor for minor). Empty style and seed are left out;
   * count defaults to 6. */
  function randomRequest(values) {
    const v = values || {};
    const mode = v.mode === "minor" ? "minor" : "major";
    const request = {
      mode,
      key: `C ${mode}`,
      meter: String(v.meter || "").trim(),
      form: v.form === "rhythm" || v.form === "contour" ? v.form : "full",
      count: Number.isInteger(Number(v.count)) && Number(v.count) >= 1 ? Math.min(Number(v.count), RANDOM_MAX_COUNT) : RANDOM_DEFAULT_COUNT,
    };
    if (v.role === "accompaniment") {
      request.role = "accompaniment";
    }
    if (String(v.style_id || "") !== "") {
      request.style_id = String(v.style_id);
    }
    if (String(v.seed === undefined || v.seed === null ? "" : v.seed).trim() !== "") {
      request.seed = Number(v.seed);
    }
    return jsonRequest("POST", "/api/materials/motifs/random", request);
  }

  /* Problems with the dialog values as {field, message}; empty when it can be sent. */
  function randomProblems(values) {
    const v = values || {};
    const problems = [];
    if (!/^\d{1,2}\/\d{1,2}$/.test(String(v.meter || "").trim())) {
      problems.push({ field: "meter", message: "Meter looks like 4/4." });
    }
    const seed = String(v.seed === undefined || v.seed === null ? "" : v.seed).trim();
    if (seed !== "" && !/^\d{1,10}$/.test(seed)) {
      problems.push({ field: "seed", message: "Seed is a whole number from 0, or leave it empty." });
    }
    const count = String(v.count === undefined || v.count === null ? "" : v.count).trim();
    if (count !== "" && !(/^\d{1,2}$/.test(count) && Number(count) >= 1 && Number(count) <= RANDOM_MAX_COUNT)) {
      problems.push({ field: "count", message: "Count is 1 to 16." });
    }
    return problems;
  }

  /* The key text of a random candidate's motive ("D minor"), or "" when it has none. */
  function candidateKeyText(candidate) {
    const key = candidate && candidate.motive && candidate.motive.motive ? candidate.motive.motive.key : null;
    const tonic = key && Number.isInteger(key.tonic_pitch_class) ? KEY_TONICS[key.tonic_pitch_class] : null;
    return tonic && typeof key.mode === "string" ? `${tonic} ${key.mode}` : "";
  }

  /* The motif body to store for a random candidate (an entry of the random route's `motives`). values: the dialog
   * values {form, meter, key, role}. The backend fills in the analysis and the degrees on save. */
  function randomCandidateToBody(candidate, values) {
    const v = values || {};
    const analysis = candidate && candidate.motive && candidate.motive.analysis ? candidate.motive.analysis : {};
    const form = v.form === "rhythm" || v.form === "contour" ? v.form : "full";
    const textName = form === "rhythm" ? "rhythm_text" : form === "contour" ? "contour_text" : "notes_text";
    const key = candidateKeyText(candidate);
    return {
      uses: [],
      motif: {
        role: v.role === "accompaniment" ? "accompaniment" : "melodic",
        form,
        notes: typeof analysis[textName] === "string" ? analysis[textName] : "",
        meter: String(v.meter || "").trim() || "4/4",
        key: key === "" ? null : key,
        pickup: typeof analysis.pickup === "string" && analysis.pickup !== "" ? analysis.pickup : "0",
        harmony: null,
        features: [],
        analysis: null,
        degrees: null,
        derived: null,
      },
    };
  }

  /* The next seed for rerolling one candidate: the candidate's own plus one. */
  function rerollSeed(candidateSeed) {
    return Number.isInteger(candidateSeed) ? candidateSeed + 1 : 0;
  }

  /* The score and playback URLs of one stored version, sounded in its stored key. */
  function versionScoreUrls(id, number) {
    const base = `/api/materials/items/${encodeURIComponent(id)}/versions/${Number(number)}`;
    return { musicxmlUrl: `${base}/score`, playbackUrl: `${base}/playback` };
  }

  /* Saving a motif: a new item when id is empty, else a new version. */
  /* True when the editor's values differ from the stored version body: the body they build is compared with the body
   * the stored values build, so fields the editor does not own do not count. Rewriting an older motif in C counts. */
  function motifBodyChanged(values, body) {
    const stored = motifFormValues(body);
    if (stored.legacyKey && !values.legacyKey) {
      return true;
    }
    return JSON.stringify(buildMotifBody(values, body)) !== JSON.stringify(buildMotifBody(stored, body));
  }

  const SAVE_NOTE_TEXT = "Give a version note.";
  const SAVE_NOTHING_TEXT = "Nothing to save.";

  /* What the single Save of a motif does. changes is {name, project, body} booleans; note the version note field.
   * Answers {kind: "none" | "blocked" | "save", message}: a body change needs a note and nothing is sent without one. */
  function motifSaveDecision(changes, note) {
    const c = changes || {};
    if (!c.name && !c.project && !c.body) {
      return { kind: "none", message: SAVE_NOTHING_TEXT };
    }
    if (c.body && String(note || "").trim() === "") {
      return { kind: "blocked", message: SAVE_NOTE_TEXT };
    }
    return { kind: "save", message: "" };
  }

  function buildSaveMotifRequest(options) {
    const o = options || {};
    if (!o.id) {
      return jsonRequest("POST", "/api/materials/items", {
        type: "motif",
        name: String(o.name || "").trim(),
        project_id: o.project_id,
        body: o.body,
        ...(o.note && String(o.note).trim() !== "" ? { note: String(o.note).trim() } : {}),
      });
    }
    const request = { body: o.body };
    if (o.note && String(o.note).trim() !== "") {
      request.note = String(o.note).trim();
    }
    return jsonRequest("POST", `/api/materials/items/${encodeURIComponent(o.id)}/versions`, request);
  }

  function timeText(time) {
    if (!time || typeof time.num !== "number" || !time.den) {
      return "?";
    }
    return time.den === 1 ? String(time.num) : `${time.num}/${time.den}`;
  }

  /* "2" or "2+1/2": a beat position counted from 1. */
  function beatText(time) {
    if (!time || typeof time.num !== "number" || !time.den) {
      return "?";
    }
    const whole = Math.floor(time.num / time.den);
    const rest = time.num - whole * time.den;
    return rest === 0 ? String(whole + 1) : `${whole + 1}+${rest}/${time.den}`;
  }

  function signed(n) {
    return n > 0 ? `+${n}` : String(n);
  }

  function degreeText(degree) {
    return `${degree.alteration ? "#" : ""}${degree.degree}`;
  }

  const DEGREES_LABELS = { full: "Degree list", rhythm: "Rhythm list", contour: "Contour list" };

  /* What the Composer heard, as rows {label, text} for the analysis panel.
   * `motif` is the body's motif object. A motif without an analysis gives
   * {analyzed: false, message} (message names the reason: analysisError when
   * given, else the motif's stored analysis_error); otherwise
   * {analyzed: true, message: "Analyzed", rows}. */
  function analysisRows(motif, analysisError) {
    const outer = motif && motif.analysis;
    const a = outer && outer.analysis;
    if (!a || typeof a !== "object") {
      const stored = analysisError === undefined && motif && typeof motif.analysis_error === "string" ? motif.analysis_error : "";
      const given = typeof analysisError === "string" ? analysisError : stored;
      const reason = given ? `: ${given}` : "";
      return { analyzed: false, message: `Not analyzed${reason}`, rows: [] };
    }
    const rows = [];
    const listText = typeof motif.degrees === "string" ? motif.degrees : "";
    if (listText !== "") {
      rows.push({ label: DEGREES_LABELS[motif.form] || DEGREES_LABELS.full, text: listText });
    }
    const keyed = Array.isArray(a.scale_degrees && a.scale_degrees.all);
    if (keyed) {
      rows.push({ label: "Scale degrees", text: a.scale_degrees.all.map(degreeText).join(" ") });
    }
    const intervals = Array.isArray(a.intervals) ? a.intervals : [];
    if (Array.isArray(a.intervals)) {
      rows.push({
        label: "Intervals",
        text:
          intervals.length === 0
            ? "none"
            : intervals
                .map((i) => (typeof i.steps === "number" ? `${signed(i.semitones)} (${signed(i.steps)} steps)` : signed(i.semitones)))
                .join(", "),
      });
    }
    if (a.contour) {
      const directions = Array.isArray(a.contour.directions) ? a.contour.directions : [];
      rows.push({
        label: "Contour",
        text: directions.length > 0 ? `${a.contour.shape} (${directions.join(", ")})` : String(a.contour.shape),
      });
    }
    if (Array.isArray(a.rhythm)) {
      rows.push({
        label: "Rhythm",
        text: a.rhythm
          .map((r) => `bar ${r.bar + 1} beat ${beatText(r.beat)}, length ${timeText(r.duration)}${r.on_beat ? "" : " (off the beat)"}`)
          .join("; "),
      });
    }
    if (!a.implied_harmony || !Array.isArray(a.intervals)) {
      return { analyzed: true, message: "Analyzed", rows };
    }
    const candidates = a.implied_harmony && Array.isArray(a.implied_harmony.candidates) ? a.implied_harmony.candidates : [];
    rows.push({
      label: "Implied harmony",
      text:
        candidates.length === 0
          ? keyed
            ? "no chord fits"
            : "needs a key"
          : candidates.map((c) => `${c.roman_numeral} (${c.function}) ${Math.round(c.fit * 100)}%`).join(", "),
    });
    return { analyzed: true, message: "Analyzed", rows };
  }

  const api = {
    ALL_PROJECTS,
    TYPES,
    typeInfo,
    buildProjectListRequest,
    buildCreateProjectRequest,
    buildRenameProjectRequest,
    buildDeleteProjectRequest,
    buildItemListRequest,
    buildItemGetRequest,
    buildVersionGetRequest,
    newItemBody,
    buildCreateItemRequest,
    buildUpdateItemRequest,
    errorText,
    projectsFrom,
    projectName,
    projectOptions,
    resolveFilter,
    readFilter,
    writeFilter,
    defaultProject,
    versionLabel,
    dateLabel,
    itemRows,
    versionRows,
    useRows,
    buildDeleteItemRequest,
    deleteWarning,
    usedByRows,
    hasUpdatedUse,
    buildUpdateUseRequest,
    buildAllItemsRequest,
    pickerItems,
    bodyWithUse,
    buildAddVersionRequest,
    buildReanalyzeRequest,
    MOTIF_TABS,
    motifBodyChanged,
    motifSaveDecision,
    readMotifTab,
    writeMotifTab,
    COMPOSER_MISSING_TEXT,
    composerAvailable,
    NOTE_HINT,
    formHint,
    errorField,
    HARMONY_HINT,
    SEGMENTS_HINT,
    buildImportMidiRequest,
    importToFormValues,
    importNotice,
    toCRequest,
    legacyNotice,
    MOTIF_MODES,
    TONIC_HINT,
    keyMode,
    MOTIF_ROLES,
    MOTIF_FORMS,
    motifFormValues,
    motifFormProblems,
    featureProblems,
    newFeatureRow,
    buildMotifBody,
    buildSaveMotifRequest,
    analysisRows,
    renderRequest,
    versionScoreUrls,
    RANDOM_MODES,
    randomRequest,
    randomProblems,
    randomCandidateToBody,
    candidateKeyText,
    rerollSeed,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.MaterialsModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
