/* Themes view (P16): the theme form state, the version body it makes, its
 * problems, the brief picks, the compose request and the rows of a result.
 * No DOM; themes-view.js renders from these. The form state is
 *   {id, uses, into, hybrid, mode, meter, bars, cadence, firstCadence,
 *    slots: [{slot, item_id, version}], accompaniment: {kind, pattern,
 *    item_id, version}, picks: [...], switches: {<category>: bool}, weight,
 *    tempo, check, seed}
 * with tempo and seed as the text typed (blank for none). */
(function (root) {
  const SLOT_NAMES = ["basic", "contrasting", "continuation", "cadential"];
  const SLOT_LABELS = {
    basic: "Basic idea",
    contrasting: "Contrasting idea",
    continuation: "Continuation",
    cadential: "Cadential",
  };
  const INTOS = [
    { value: "sentence", label: "Sentence" },
    { value: "period", label: "Period" },
    { value: "hybrid", label: "Hybrid" },
  ];
  const BEGINNINGS = ["presentation", "antecedent", "cbi"];
  const ENDINGS = ["continuation", "consequent", "cadential"];
  const MODES = [
    { value: "major", label: "Major" },
    { value: "minor", label: "Minor" },
    { value: "style", label: "From the brief" },
  ];
  const METERS = ["2/4", "3/4", "4/4", "6/8"];
  const BARS = [4, 8];
  const CADENCES = ["PAC", "IAC", "HC"];
  const FIRST_CADENCES = ["HC", "IAC"];
  const POOLS = [
    { value: "approved-and-unflagged", label: "Approved and unflagged" },
    { value: "approved", label: "Approved only" },
    { value: "none", label: "None" },
  ];
  const PATTERNS = [
    { value: "any", label: "Any pattern" },
    { value: "block", label: "Block" },
    { value: "arpeggiated", label: "Arpeggiated" },
    { value: "alberti", label: "Alberti" },
    { value: "oom-pah", label: "Oom-pah" },
  ];
  const WEIGHTS = [
    { value: "1/4", label: "Light" },
    { value: "1/2", label: "Normal" },
    { value: "1", label: "Strong" },
  ];
  const MOTIVE_CATEGORIES = [
    { category: "motive-degrees", label: "degrees" },
    { category: "motive-intervals", label: "intervals" },
    { category: "motive-rhythm", label: "rhythm" },
    { category: "motive-nct-figure", label: "non-chord figures" },
  ];
  const SWITCH_TEXT = "Use the brief's motive degrees, intervals, rhythm, non-chord figures";
  const THEME_TABS = [
    { id: "theme", label: "Theme" },
    { id: "structure", label: "Structure" },
    { id: "used-by", label: "Used by" },
    { id: "other", label: "Other" },
  ];
  const THEME_TAB_KEY = "ursatz.theme.tab";
  const MAX_SLOTS = 4;
  const TEMPO_MIN = 20;
  const TEMPO_MAX = 400;
  const MAX_SEED = 4294967295;

  function jsonRequest(method, url, body) {
    const request = { method, url };
    if (body !== undefined) {
      request.headers = { "Content-Type": "application/json" };
      request.body = JSON.stringify(body);
    }
    return request;
  }

  function text(value) {
    return typeof value === "string" ? value.trim() : "";
  }

  function allSwitches(on) {
    return Object.fromEntries(MOTIVE_CATEGORIES.map((entry) => [entry.category, on]));
  }

  /* The categories whose switch is off, in the order of the four. */
  function withoutFromSwitches(switches) {
    const s = switches || {};
    return MOTIVE_CATEGORIES.filter((entry) => s[entry.category] === false).map((entry) => entry.category);
  }

  function switchesFromWithout(without) {
    const list = Array.isArray(without) ? without : [];
    return Object.fromEntries(MOTIVE_CATEGORIES.map((entry) => [entry.category, !list.includes(entry.category)]));
  }

  function emptyState(id) {
    return {
      id: id || "",
      uses: [],
      into: "sentence",
      hybrid: "presentation+continuation",
      mode: "major",
      meter: "4/4",
      bars: 8,
      cadence: "PAC",
      firstCadence: "",
      slots: [],
      accompaniment: { kind: "none", pattern: "any", item_id: "", version: 0 },
      picks: [],
      switches: allSwitches(true),
      weight: "1/2",
      tempo: "",
      check: false,
      seed: "",
    };
  }

  /* The remembered theme tab id, "theme" when unset, unknown or storage is unavailable. */
  function readThemeTab() {
    try {
      const saved = root.localStorage.getItem(THEME_TAB_KEY);
      return THEME_TABS.some((tab) => tab.id === saved) ? saved : THEME_TABS[0].id;
    } catch (err) {
      return THEME_TABS[0].id;
    }
  }

  function writeThemeTab(id) {
    try {
      root.localStorage.setItem(THEME_TAB_KEY, id);
    } catch (err) {
      // Storage unavailable: the tab just is not remembered.
    }
  }

  /* --- the brief ------------------------------------------------------- */

  function sameList(a, b) {
    return JSON.stringify(a) === JSON.stringify(b);
  }

  /* A scope from a pick, or null when the pick names nothing. */
  function scopeFromPick(pick) {
    switch (pick.kind) {
      case "collection":
        return text(pick.collectionId) ? { kind: "collection", collection_id: pick.collectionId } : null;
      case "filter": {
        const filter = {};
        for (const key of ["composer", "source", "tag"]) {
          if (text(pick[key])) {
            filter[key] = pick[key].trim();
          }
        }
        return Object.keys(filter).length > 0 ? { kind: "filter", filter } : null;
      }
      case "subject":
        return text(pick.subjectId) ? { kind: "subject", subject_id: pick.subjectId } : null;
      case "project":
        return text(pick.projectId) ? { kind: "project", project_id: pick.projectId } : null;
      default:
        return null;
    }
  }

  /* The brief {scopes, piece_ids, style_ids} of a list of picks, without
   * repeats, or null when nothing is picked. A pick is {kind: collection |
   * filter | subject | project | piece | style, ...}. */
  function briefFromPicks(picks) {
    const scopes = [];
    const pieceIds = [];
    const styleIds = [];
    for (const pick of Array.isArray(picks) ? picks : []) {
      if (!pick) {
        continue;
      }
      if (pick.kind === "piece") {
        if (text(pick.id) && !pieceIds.includes(pick.id)) {
          pieceIds.push(pick.id);
        }
      } else if (pick.kind === "style") {
        if (text(pick.id) && !styleIds.includes(pick.id)) {
          styleIds.push(pick.id);
        }
      } else {
        const scope = scopeFromPick(pick);
        if (scope && !scopes.some((other) => sameList(other, scope))) {
          scopes.push(scope);
        }
      }
    }
    if (scopes.length + pieceIds.length + styleIds.length === 0) {
      return null;
    }
    return { scopes, piece_ids: pieceIds, style_ids: styleIds };
  }

  function picksFromBrief(brief) {
    const picks = [];
    if (!brief || typeof brief !== "object") {
      return picks;
    }
    for (const scope of Array.isArray(brief.scopes) ? brief.scopes : []) {
      if (scope.kind === "collection") {
        picks.push({ kind: "collection", collectionId: scope.collection_id });
      } else if (scope.kind === "subject") {
        picks.push({ kind: "subject", subjectId: scope.subject_id });
      } else if (scope.kind === "project") {
        picks.push({ kind: "project", projectId: scope.project_id });
      } else if (scope.kind === "filter") {
        picks.push({ kind: "filter", ...(scope.filter || {}) });
      }
    }
    for (const id of Array.isArray(brief.piece_ids) ? brief.piece_ids : []) {
      picks.push({ kind: "piece", id });
    }
    for (const id of Array.isArray(brief.style_ids) ? brief.style_ids : []) {
      picks.push({ kind: "style", id });
    }
    return picks;
  }

  /* The chip text of one pick; names is {collections, subjects, projects,
   * pieces, styles}, each {id: name}, for the ids. */
  function pickLabel(pick, names) {
    const n = names || {};
    const named = (table, id) => (n[table] && n[table][id]) || id;
    switch (pick.kind) {
      case "collection":
        return `Collection: ${named("collections", pick.collectionId)}`;
      case "subject":
        return `Subject: ${named("subjects", pick.subjectId)}`;
      case "project":
        return `Project: ${named("projects", pick.projectId)}`;
      case "piece":
        return `Piece: ${named("pieces", pick.id)}`;
      case "style":
        return `Style: ${named("styles", pick.id)}`;
      case "filter":
        return ["composer", "source", "tag"]
          .filter((key) => text(pick[key]))
          .map((key) => `${key[0].toUpperCase()}${key.slice(1)}: ${pick[key]}`)
          .join(", ");
      default:
        return "";
    }
  }

  function countText(count, one, many) {
    return `${count} ${count === 1 ? one : many}`;
  }

  /* One line for a brief: "2 scopes, 3 pieces, 1 style" or "No brief". */
  function briefLabel(brief) {
    if (!brief) {
      return "No brief";
    }
    const parts = [];
    const scopes = Array.isArray(brief.scopes) ? brief.scopes.length : 0;
    const pieces = Array.isArray(brief.piece_ids) ? brief.piece_ids.length : 0;
    const styles = Array.isArray(brief.style_ids) ? brief.style_ids.length : 0;
    if (scopes > 0) {
      parts.push(countText(scopes, "scope", "scopes"));
    }
    if (pieces > 0) {
      parts.push(countText(pieces, "piece", "pieces"));
    }
    if (styles > 0) {
      parts.push(countText(styles, "style", "styles"));
    }
    return parts.length > 0 ? parts.join(", ") : "No brief";
  }

  /* --- body and form --------------------------------------------------- */

  function wholeNumber(value, min, max) {
    const t = text(String(value === undefined || value === null ? "" : value));
    if (!/^\d+$/.test(t)) {
      return null;
    }
    const n = Number(t);
    return n >= min && n <= max ? n : null;
  }

  function slotEntries(slots) {
    const list = Array.isArray(slots) ? slots : [];
    return SLOT_NAMES.flatMap((name) => list.filter((entry) => entry && entry.slot === name)).map((entry) => ({
      slot: entry.slot,
      item_id: entry.item_id,
      version: Number(entry.version),
    }));
  }

  /* The version body of a form state: {uses, theme}. The result is left out;
   * the backend fills the uses of the slot and accompaniment motifs. */
  function bodyFromForm(state) {
    const s = state || emptyState();
    const hybrid = s.into === "hybrid";
    const firstCadence = s.into !== "sentence" && FIRST_CADENCES.includes(s.firstCadence) ? s.firstCadence : null;
    const acc = s.accompaniment || { kind: "none" };
    let accompaniment = null;
    if (acc.kind === "pattern") {
      accompaniment = { pattern: acc.pattern || "any" };
    } else if (acc.kind === "motive") {
      accompaniment = { pattern: "motive", item_id: acc.item_id, version: Number(acc.version) };
    }
    const brief = briefFromPicks(s.picks);
    const theme = {
      form: {
        into: s.into,
        hybrid: hybrid ? text(s.hybrid) : null,
        mode: s.mode,
        meter: s.meter,
        bars: Number(s.bars),
        cadence: s.cadence,
        first_cadence: firstCadence,
      },
      slots: slotEntries(s.slots),
      accompaniment,
      brief,
      style_weight: s.weight,
      without: withoutFromSwitches(s.switches),
      tempo: wholeNumber(s.tempo, TEMPO_MIN, TEMPO_MAX),
      check: Boolean(s.check),
      seed: wholeNumber(s.seed, 0, MAX_SEED),
    };
    return { uses: Array.isArray(s.uses) ? s.uses.slice() : [], theme };
  }

  /* The form state of a version body. A placeholder theme (form null or a
   * string) gives the defaults. */
  function formFromBody(body, id) {
    const state = emptyState(id);
    const theme = body && body.theme && typeof body.theme === "object" ? body.theme : {};
    state.uses = body && Array.isArray(body.uses) ? body.uses.slice() : [];
    const form = theme.form && typeof theme.form === "object" ? theme.form : {};
    if (INTOS.some((entry) => entry.value === form.into)) {
      state.into = form.into;
    }
    if (typeof form.hybrid === "string" && form.hybrid !== "") {
      state.hybrid = form.hybrid;
    }
    if (MODES.some((entry) => entry.value === form.mode)) {
      state.mode = form.mode;
    }
    if (METERS.includes(form.meter)) {
      state.meter = form.meter;
    }
    if (BARS.includes(form.bars)) {
      state.bars = form.bars;
    }
    if (CADENCES.includes(form.cadence)) {
      state.cadence = form.cadence;
    }
    if (FIRST_CADENCES.includes(form.first_cadence)) {
      state.firstCadence = form.first_cadence;
    }
    state.slots = slotEntries((Array.isArray(theme.slots) ? theme.slots : []).filter((slot) => slot && typeof slot === "object"));
    const acc = theme.accompaniment;
    if (acc && typeof acc === "object") {
      state.accompaniment =
        acc.pattern === "motive"
          ? { kind: "motive", pattern: "any", item_id: acc.item_id, version: Number(acc.version) }
          : { kind: "pattern", pattern: acc.pattern, item_id: "", version: 0 };
    }
    state.picks = picksFromBrief(theme.brief);
    state.switches = switchesFromWithout(theme.without);
    if (WEIGHTS.some((entry) => entry.value === theme.style_weight)) {
      state.weight = theme.style_weight;
    }
    state.tempo = Number.isInteger(theme.tempo) ? String(theme.tempo) : "";
    state.check = theme.check === true;
    state.seed = Number.isInteger(theme.seed) ? String(theme.seed) : "";
    return state;
  }

  function hybridOk(pairing) {
    const parts = text(pairing).split("+");
    return parts.length === 2 && BEGINNINGS.includes(parts[0]) && ENDINGS.includes(parts[1]);
  }

  /* What stops Compose, as a list of sentences. */
  function formProblems(state) {
    const s = state || emptyState();
    const problems = [];
    const slots = Array.isArray(s.slots) ? s.slots : [];
    if (slots.length > MAX_SLOTS) {
      problems.push("A theme has at most 4 slot motifs.");
    }
    if (!slots.some((entry) => entry && entry.slot === "basic" && text(entry.item_id))) {
      problems.push("The basic idea needs a motif.");
    }
    if (!METERS.includes(s.meter)) {
      problems.push("The meter must be 2/4, 3/4, 4/4 or 6/8.");
    }
    if (!BARS.includes(Number(s.bars))) {
      problems.push("The theme has 4 or 8 bars.");
    }
    if (s.into === "hybrid" && !hybridOk(s.hybrid)) {
      problems.push("A hybrid needs a pairing such as presentation+continuation.");
    }
    if (text(s.tempo) !== "" && wholeNumber(s.tempo, TEMPO_MIN, TEMPO_MAX) === null) {
      problems.push(`The tempo is ${TEMPO_MIN} to ${TEMPO_MAX} quarter notes per minute, or blank.`);
    }
    if (text(s.seed) !== "" && wholeNumber(s.seed, 0, MAX_SEED) === null) {
      problems.push("The seed is a whole number, or blank.");
    }
    if (s.mode === "style" && briefFromPicks(s.picks) === null) {
      problems.push("The mode from the brief needs a brief.");
    }
    const acc = s.accompaniment || {};
    if (acc.kind === "motive" && !text(acc.item_id)) {
      problems.push("The accompaniment needs a motif.");
    }
    return problems;
  }

  /* --- requests -------------------------------------------------------- */

  function itemUrl(id, tail) {
    return `/api/materials/items/${encodeURIComponent(id)}${tail}`;
  }

  /* A new version of the theme holding the form (no result). */
  function saveRequest(state, note) {
    const request = { body: bodyFromForm(state) };
    if (text(note)) {
      request.note = text(note);
    }
    return jsonRequest("POST", itemUrl(state.id, "/versions"), request);
  }

  /* POST compose for the form state. options: {seed, update_uses}; a seed in
   * options wins over the form's. */
  function composeRequest(state, options) {
    const o = options || {};
    const body = {};
    const seed = o.seed !== undefined && o.seed !== null && o.seed !== "" ? wholeNumber(o.seed, 0, MAX_SEED) : wholeNumber(state.seed, 0, MAX_SEED);
    if (seed !== null) {
      body.seed = seed;
    }
    if (o.update_uses) {
      body.update_uses = true;
    }
    return jsonRequest("POST", itemUrl(state.id, "/compose"), body);
  }

  function versionFileUrl(id, number, name) {
    return `${itemUrl(id, "")}/versions/${Number(number)}/files/${encodeURIComponent(name)}`;
  }

  /* --- the pool -------------------------------------------------------- */

  /* Counts {approved, unflagged, rejected} of the saved forms below a motif
   * item in the variant tree answer (the node of item_id, its descendants). */
  function poolCounts(tree, itemId) {
    const counts = { approved: 0, unflagged: 0, rejected: 0 };
    function find(node) {
      if (!node) {
        return null;
      }
      if (node.id === itemId) {
        return node;
      }
      for (const child of node.children || []) {
        const found = find(child);
        if (found) {
          return found;
        }
      }
      return null;
    }
    function walk(node) {
      for (const child of node.children || []) {
        const flag = child.derived && counts[child.derived.flag] !== undefined ? child.derived.flag : "unflagged";
        counts[flag]++;
        walk(child);
      }
    }
    const start = find(tree);
    if (start) {
      walk(start);
    }
    return counts;
  }

  function poolText(counts) {
    const c = counts || { approved: 0, unflagged: 0 };
    return `${c.approved} approved, ${c.unflagged} unflagged`;
  }

  /* --- the result ------------------------------------------------------ */

  const FROM_TEXT = { request: "set here", option: "set here", style: "from the brief", none: "not set" };
  const SOURCE_TEXT = {
    motive: "slot motif, exact",
    pool: "saved form",
    registry: "registry form",
    operator: "operator",
  };
  const DISTANCE_LABELS = ["harmony", "rhythm", "melody", "form", "overall"];

  function firstTheme(summary) {
    return summary && Array.isArray(summary.themes) && summary.themes.length > 0 ? summary.themes[0] : null;
  }

  function percent(value) {
    return Number.isFinite(value) ? `${Math.round(value)}%` : "";
  }

  function motiveName(names, id) {
    return (names && names[id]) || id || "";
  }

  /* The Composer's summary of a theme version's result as rows to show.
   * result is theme.result ({seed, summary, brief, files}); options carries
   * motiveNames ({A: "Name"}) and check (the Analyzer check was asked for). */
  function resultView(result, options) {
    const o = options || {};
    const r = result && typeof result === "object" ? result : {};
    const summary = r.summary || {};
    const theme = firstTheme(summary) || {};
    const rows = (Array.isArray(theme.slots) ? theme.slots : []).map((entry) => ({
      idea: entry.idea,
      slot: SLOT_LABELS[entry.slot] || entry.slot,
      motive: motiveName(o.motiveNames, entry.motive),
      source: SOURCE_TEXT[entry.source] || entry.source || "",
      item: entry.item || "",
      chain: entry.chain || "",
      connection: percent(entry.connection),
    }));
    const brief = summary.brief || {};
    const tempo = brief.tempo || {};
    const mode = brief.mode || {};
    const without = Array.isArray(brief.without) ? brief.without : [];
    const distance = theme.style_distance && typeof theme.style_distance === "object" ? theme.style_distance : null;
    const pieces = r.brief && typeof r.brief === "object" ? r.brief : {};
    return {
      seed: Number.isInteger(r.seed) ? r.seed : null,
      rows,
      brief: {
        tempo: Number.isFinite(tempo.qpm) ? `${tempo.qpm} (${FROM_TEXT[tempo.from] || tempo.from})` : "not set",
        mode: mode.mode ? `${mode.mode} (${FROM_TEXT[mode.from] || mode.from})` : "",
        density: Number.isFinite(brief.density) ? `${brief.density} attacks per beat` : "not set",
        without: without.length > 0 ? without.join(", ") : "none",
      },
      pieces:
        Number.isInteger(pieces.piece_count) && pieces.piece_count > 0
          ? `${countText(pieces.piece_count, "piece", "pieces")} in the brief` +
            (Array.isArray(pieces.skipped) && pieces.skipped.length > 0 ? `, ${pieces.skipped.length} skipped` : "")
          : "",
      distance: distance
        ? DISTANCE_LABELS.filter((label) => Number.isFinite(distance[label])).map((label) => ({
            label,
            value: distance[label].toFixed(4),
          }))
        : [],
      heard: theme.heard_text || "",
      heardAsRequested: theme.heard_as_requested === true,
      check: o.check ? "no contradiction found" : "not run",
      files: Array.isArray(r.files) ? r.files.slice() : [],
    };
  }

  /* {A: "Name"} for the motive ids the backend gives the slots, in slot
   * order; nameOf(item_id) gives a motif's name. */
  function motiveNames(state, nameOf) {
    const names = {};
    const ids = ["A", "B", "C", "D"];
    slotEntries(state.slots).forEach((entry, index) => {
      names[ids[index]] = nameOf ? nameOf(entry.item_id) || entry.item_id : entry.item_id;
    });
    return names;
  }

  /* --- playback from the MIDI file -------------------------------------- */

  /* The playback data of a Standard MIDI File (ArrayBuffer or Uint8Array): the shape the score workspace reads
   * ({ok, tempo, parts, end, notes}, positions in whole notes as [ticks, 4 * ticks per quarter]), or null when the
   * file is not a MIDI file with ticks per quarter. */
  function playbackFromMidi(data) {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    let at = 0;
    const u8 = () => bytes[at++];
    const u16 = () => (u8() << 8) | u8();
    const u32 = () => u16() * 65536 + u16();
    const tag = () => String.fromCharCode(u8(), u8(), u8(), u8());
    const varlen = () => {
      let value = 0;
      for (let i = 0; i < 4; i++) {
        const byte = u8();
        value = value * 128 + (byte & 0x7f);
        if (byte < 0x80) {
          break;
        }
      }
      return value;
    };
    if (bytes.length < 14 || tag() !== "MThd" || u32() < 6) {
      return null;
    }
    u16();
    const trackCount = u16();
    const division = u16();
    if (division === 0 || division >= 0x8000) {
      return null;
    }
    at = 14;
    const whole = 4 * division;
    const tempo = [];
    const parts = [];
    const notes = [];
    let end = 0;
    for (let t = 0; t < trackCount && at + 8 <= bytes.length; t++) {
      if (tag() !== "MTrk") {
        return null;
      }
      const length = u32();
      const stop = Math.min(bytes.length, at + length);
      let tick = 0;
      let status = 0;
      let name = "";
      let program = 0;
      const open = new Map();
      const trackNotes = [];
      while (at < stop) {
        tick += varlen();
        let first = u8();
        if (first === 0xff) {
          const type = u8();
          const length = varlen();
          if (type === 0x51 && length === 3) {
            const micros = (u8() << 16) | (u8() << 8) | u8();
            tempo.push({ at: tick, qpm: Math.round((60000000 / micros) * 100) / 100 });
          } else if (type === 0x03) {
            name = String.fromCharCode(...bytes.slice(at, at + length));
            at += length;
          } else {
            at += length;
          }
          continue;
        }
        if (first === 0xf0 || first === 0xf7) {
          at += varlen();
          continue;
        }
        if (first >= 0x80) {
          status = first;
          first = u8();
        }
        const kind = status & 0xf0;
        if (kind === 0xc0) {
          program = first;
        } else if (kind === 0xd0) {
          continue;
        } else {
          const second = u8();
          const key = `${status & 0x0f}:${first}`;
          if (kind === 0x90 && second > 0) {
            open.set(key, [...(open.get(key) || []), tick]);
          } else if (kind === 0x80 || kind === 0x90) {
            const starts = open.get(key) || [];
            if (starts.length > 0) {
              const on = starts.shift();
              trackNotes.push({ on, len: tick - on, pitch: first });
            }
          }
        }
      }
      at = stop;
      if (trackNotes.length > 0) {
        const index = parts.length;
        parts.push({ index, name: name || `Part ${index + 1}`, program });
        for (const note of trackNotes) {
          notes.push({ ...note, part: index });
          end = Math.max(end, note.on + note.len);
        }
      }
    }
    notes.sort((a, b) => a.on - b.on || a.pitch - b.pitch || a.part - b.part);
    tempo.sort((a, b) => a.at - b.at);
    if (tempo.length === 0 || tempo[0].at > 0) {
      tempo.unshift({ at: 0, qpm: tempo.length > 0 ? tempo[0].qpm : 120 });
    }
    return {
      ok: true,
      tempo_source: "given",
      tempo: tempo.map((region) => ({ at: [region.at, whole], qpm: region.qpm })),
      parts,
      end: [end, whole],
      notes: notes.map((note, i) => ({ i, on: [note.on, whole], len: [note.len, whole], pitch: note.pitch, part: note.part })),
    };
  }

  /* --- sources --------------------------------------------------------- */

  /* The slot and accompaniment motifs with a newer version, from the item
   * answer's uses_status: [{item_id, name, version, current, label}]. */
  function updatedUses(item) {
    const statuses = item && Array.isArray(item.uses_status) ? item.uses_status : [];
    return statuses
      .filter((use) => !use.missing && Number(use.current_version) > Number(use.version))
      .map((use) => ({
        item_id: use.item_id,
        name: use.name || use.item_id,
        version: Number(use.version),
        current: Number(use.current_version),
        label: `Source updated: ${use.name || use.item_id} v${use.current_version}`,
      }));
  }

  /* A copy of the form with every slot and accompaniment motif at its source's current version. */
  function formWithUpdates(state, updates) {
    const next = JSON.parse(JSON.stringify(state));
    for (const update of updates) {
      for (const entry of next.slots) {
        if (entry.item_id === update.item_id) {
          entry.version = update.current;
        }
      }
      if (next.accompaniment.kind === "motive" && next.accompaniment.item_id === update.item_id) {
        next.accompaniment.version = update.current;
      }
    }
    return next;
  }

  const api = {
    SLOT_NAMES,
    SLOT_LABELS,
    INTOS,
    BEGINNINGS,
    ENDINGS,
    MODES,
    METERS,
    BARS,
    CADENCES,
    FIRST_CADENCES,
    POOLS,
    PATTERNS,
    WEIGHTS,
    MOTIVE_CATEGORIES,
    SWITCH_TEXT,
    THEME_TABS,
    readThemeTab,
    writeThemeTab,
    emptyState,
    bodyFromForm,
    formFromBody,
    formProblems,
    withoutFromSwitches,
    switchesFromWithout,
    briefFromPicks,
    picksFromBrief,
    pickLabel,
    briefLabel,
    saveRequest,
    composeRequest,
    versionFileUrl,
    poolCounts,
    poolText,
    resultView,
    motiveNames,
    updatedUses,
    formWithUpdates,
    playbackFromMidi,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ThemesModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
