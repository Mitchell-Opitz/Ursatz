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

  /* One line per use: "Name v2" (Name vN, missing when the source is gone). */
  function useRows(uses) {
    return (Array.isArray(uses) ? uses : []).map((use) => ({
      item_id: use.item_id,
      name: use.name || use.item_id,
      label: `${use.name || use.item_id} v${use.version}`,
      missing: Boolean(use.missing),
    }));
  }


  const NOTE_HINT = "Pitch:duration, e.g. D4:q F4:e A4:e D5:h. r is a rest, ~ ties to the next note, | is ignored. Durations: w h q e s t, with up to two dots.";
  const MOTIF_ROLES = [
    { value: "melodic", label: "Melodic" },
    { value: "accompaniment", label: "Accompaniment" },
  ];
  /* Only the full form is editable in P12; the others arrive in P13. */
  const MOTIF_FORMS = [
    { value: "full", label: "Full", enabled: true },
    { value: "rhythm", label: "Rhythm (P13)", enabled: false },
    { value: "contour", label: "Contour (P13)", enabled: false },
  ];

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
    if (!/^\d+(\/\d+)?$/.test(String(values.pickup || "").trim())) {
      problems.push({ field: "pickup", message: "Pickup is beats as 0, 1 or 1/2." });
    }
    return problems;
  }

  /* The version body for the form values. `previous` is the body being
   * edited: its uses and derived data carry over. The backend fills in the
   * analysis. */
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
    NOTE_HINT,
    MOTIF_ROLES,
    MOTIF_FORMS,
    motifFormValues,
    motifFormProblems,
    buildMotifBody,
    buildSaveMotifRequest,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.MaterialsModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
