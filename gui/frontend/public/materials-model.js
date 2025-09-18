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

  /* The syntax hint under the note list for a motif form. */
  function formHint(form) {
    return form === "rhythm" ? RHYTHM_HINT : form === "contour" ? CONTOUR_HINT : NOTE_HINT;
  }

  /* The editor's field values for a motif body (null for a new motif). */
  function motifFormValues(body) {
    const motif = body && body.motif ? body.motif : {};
    return {
      role: motif.role === "accompaniment" ? "accompaniment" : "melodic",
      form: motif.form || "full",
      notes: typeof motif.notes === "string" ? motif.notes : "",
      meter: typeof motif.meter === "string" ? motif.meter : "4/4",
      key: typeof motif.key === "string" ? motif.key : "",
      pickup: typeof motif.pickup === "string" ? motif.pickup : "0",
      features: Array.isArray(motif.features)
        ? motif.features.map((feature) => ({
            label: String(feature.label),
            first: Number(feature.first) + 1,
            last: Number(feature.last) + 1,
          }))
        : [],
    };
  }

  /* Problems with the form values as {field, message}; empty when it can be
   * sent. The Composer judges the note list itself. */
  function motifFormProblems(values) {
    const problems = [];
    if (String(values.notes || "").trim() === "") {
      problems.push({ field: "notes", message: "Enter at least one note." });
    }
    if (!/^\d{1,2}\/\d{1,2}$/.test(String(values.meter || "").trim())) {
      problems.push({ field: "meter", message: "Meter looks like 4/4." });
    }
    const key = String(values.key || "").trim();
    if (key !== "" && !/^[A-Ga-g][#b]? (major|minor)$/i.test(key)) {
      problems.push({ field: "key", message: "Key looks like D minor, or leave it empty." });
    }
    if (!/^\d{1,2}(\/\d{1,2})?$/.test(String(values.pickup || "").trim())) {
      problems.push({ field: "pickup", message: "Pickup is beats as 0, 1 or 1/2." });
    }
    for (const message of featureProblems(values.features)) {
      problems.push({ field: "features", message });
    }
    return problems;
  }

  /* Messages for the feature rows (1-based first and last note). */
  function featureProblems(features) {
    const messages = [];
    const seen = new Set();
    (features || []).forEach((feature, index) => {
      const row = `Feature ${index + 1}`;
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
        key: String(values.key || "").trim() === "" ? null : String(values.key).trim(),
        pickup: String(values.pickup || "").trim() === "" ? "0" : String(values.pickup).trim(),
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

  /* Saving a motif: a new item when id is empty, else a new version. */
  function buildSaveMotifRequest(options) {
    const o = options || {};
    if (!o.id) {
      return jsonRequest("POST", "/api/materials/items", {
        type: "motif",
        name: String(o.name || "").trim(),
        project_id: o.project_id,
        body: o.body,
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

  /* What the Composer heard, as rows {label, text} for the analysis panel.
   * `motif` is the body's motif object. A motif without an analysis gives
   * {analyzed: false, message} (message names analysisError when given);
   * otherwise {analyzed: true, rows}. */
  function analysisRows(motif, analysisError) {
    const outer = motif && motif.analysis;
    const a = outer && outer.analysis;
    if (!a || typeof a !== "object") {
      const reason = analysisError ? `: ${analysisError}` : "";
      return { analyzed: false, message: `Not analyzed${reason}`, rows: [] };
    }
    const rows = [];
    const keyed = Array.isArray(a.scale_degrees && a.scale_degrees.all);
    if (keyed) {
      rows.push({ label: "Scale degrees", text: a.scale_degrees.all.map(degreeText).join(" ") });
    }
    const intervals = Array.isArray(a.intervals) ? a.intervals : [];
    rows.push({
      label: "Intervals",
      text:
        intervals.length === 0
          ? "none"
          : intervals
              .map((i) => (typeof i.steps === "number" ? `${signed(i.semitones)} (${signed(i.steps)} steps)` : signed(i.semitones)))
              .join(", "),
    });
    if (a.contour) {
      const directions = Array.isArray(a.contour.directions) ? a.contour.directions : [];
      rows.push({
        label: "Contour",
        text: directions.length > 0 ? `${a.contour.shape} (${directions.join(", ")})` : String(a.contour.shape),
      });
    }
    const rhythm = Array.isArray(a.rhythm) ? a.rhythm : [];
    rows.push({
      label: "Rhythm",
      text: rhythm
        .map((r) => `bar ${r.bar + 1} beat ${beatText(r.beat)}, length ${timeText(r.duration)}${r.on_beat ? "" : " (off the beat)"}`)
        .join("; "),
    });
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
    return { analyzed: true, message: "", rows };
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
    NOTE_HINT,
    formHint,
    MOTIF_ROLES,
    MOTIF_FORMS,
    motifFormValues,
    motifFormProblems,
    featureProblems,
    newFeatureRow,
    buildMotifBody,
    buildSaveMotifRequest,
    analysisRows,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.MaterialsModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
