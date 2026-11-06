/* Themes view (P16): the theme form state, the version body it makes, its
 * problems, the brief picks, the compose request and the rows of a result.
 * No DOM; themes-view.js renders from these. The form state is
 *   {id, uses, into, hybrid, mode, meter, bars, cadence, firstCadence,
 *    slots: [{slot, item_id, version}], ideas: [{idea, item_id, version}], accompaniment: {kind, pattern,
 *    item_id, version}, picks: [...], switches: {<category>: bool}, weight,
 *    tempo, check, seed}
 * with tempo and seed as the text typed (blank for none). */
(function (root) {
  const Req = root.ApiRequest || (typeof require === "function" ? require("./request.js") : null);
  const Materials = root.MaterialsModel || (typeof require === "function" ? require("./materials-model.js") : null);
  /* The Composer's words (R-270, R-276): every list below is read from the vocabulary when asked, so the model keeps no
   * table of music. The form state holds the words as the Composer spells them. */
  const Vocab = root.Vocabulary || (typeof require === "function" ? require("./vocabulary.js") : null);
  const slotNames = () => Vocab.values("theme_slots");
  const slotLabels = () => Object.fromEntries(Vocab.list("theme_slots").map((entry) => [entry.value, entry.label]));
  const intos = () => Vocab.list("piece_intos");
  const beginnings = () => Vocab.values("theme_beginnings");
  const endings = () => Vocab.values("theme_endings");
  const modes = () => Vocab.list("piece_modes");
  const meters = () => Vocab.values("meters");
  const barChoices = () => Vocab.values("theme_bars").map(Number);
  const cadences = () => Vocab.values("theme_cadences");
  const firstCadences = () => Vocab.values("theme_first_cadences");
  const pools = () => Vocab.list("theme_pools");
  const patterns = () => Vocab.list("accompaniment_patterns");
  const weights = () => Vocab.list("style_weights");
  const motiveCategories = () =>
    Vocab.list("theme_motive_categories").map((entry) => ({ category: entry.value, label: entry.label }));
  /* The beats in a bar, and the beat of the one cell a second chord may start on (absent when the bar takes one). */
  const meterBeats = (meter) => (Vocab.entry("meters", meter) || {}).beats;
  const secondChordBeat = (meter) => (Vocab.entry("meters", meter) || {}).second_chord_beat;
  /* The ideas of a half of a form (a beginning or an ending), each {idea, slot}, in order. */
  const halfIdeas = (list, half) =>
    ((Vocab.entry(list, half) || {}).ideas || []).map((idea) => ({ idea, slot: (Vocab.entry("theme_ideas", idea) || {}).slot }));
  const tempoMin = () => Vocab.limit("tempo_min");
  const tempoMax = () => Vocab.limit("tempo_max");
  const maxSeed = () => Vocab.seedMax("theme");
  /* UI limits the Composer does not hold (R-276). */
  const CHORD_BARS = 8;
  const CHORD_TEXT_MAX = 64;
  const SWITCH_TEXT = "Use the brief's motive degrees, intervals, rhythm, non-chord figures";
  const THEME_TABS = [
    { id: "theme", label: "Theme" },
    { id: "structure", label: "Structure" },
    { id: "used-by", label: "Used by" },
    { id: "other", label: "Other" },
  ];
  const THEME_TAB_KEY = "ursatz.theme.tab";
  const MAX_SLOTS = 4;

  function text(value) {
    return typeof value === "string" ? value.trim() : "";
  }

  function allSwitches(on) {
    return Object.fromEntries(motiveCategories().map((entry) => [entry.category, on]));
  }

  /* The categories whose switch is off, in the order of the four. */
  function withoutFromSwitches(switches) {
    const s = switches || {};
    return motiveCategories().filter((entry) => s[entry.category] === false).map((entry) => entry.category);
  }

  function switchesFromWithout(without) {
    const list = Array.isArray(without) ? without : [];
    return Object.fromEntries(motiveCategories().map((entry) => [entry.category, !list.includes(entry.category)]));
  }

  function emptyState(id) {
    return {
      id: id || "",
      uses: [],
      into: "sentence",
      hybrid: "presentation+continuation",
      mode: "major",
      modeChosen: false,
      meter: "4/4",
      bars: 8,
      cadence: "PAC",
      firstCadence: "",
      slots: [],
      ideas: [],
      chords: Array(CHORD_BARS).fill(""),
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

  /* The entries of ideas in the order of the form's ideas, those of other forms and `basic idea` left out. */
  function ideaEntries(ideas, allowed) {
    const list = Array.isArray(ideas) ? ideas : [];
    return allowed
      .map((name) => list.find((entry) => entry && entry.idea === name))
      .filter(Boolean)
      .map((entry) => ({ idea: entry.idea, item_id: entry.item_id, version: Number(entry.version) }));
  }

  function slotEntries(slots) {
    const list = Array.isArray(slots) ? slots : [];
    return slotNames().flatMap((name) => list.filter((entry) => entry && entry.slot === name)).map((entry) => ({
      slot: entry.slot,
      item_id: entry.item_id,
      version: Number(entry.version),
    }));
  }

  /* The chords given per bar as [{bar, text}], bar from 0, for the filled bars of the theme only. */
  function chordEntries(state) {
    const list = Array.isArray(state.chords) ? state.chords : [];
    const entries = [];
    for (let bar = 0; bar < Math.min(CHORD_BARS, Number(state.bars)); bar++) {
      const value = text(list[bar]);
      if (value !== "") {
        entries.push({ bar, text: value });
      }
    }
    return entries;
  }

  /* The chords given, one line for the facts of a composed theme (`Bar 1: I V@2; Bar 3: IV`), blank when none. */
  function chordsText(state) {
    return chordEntries(state)
      .map((entry) => `Bar ${entry.bar + 1}: ${entry.text}`)
      .join("; ");
  }

  /* Clears the chords of the bars the theme no longer has; the bar numbers (from 1) that held one. */
  function dropChords(state) {
    const dropped = [];
    for (let bar = Number(state.bars); bar < CHORD_BARS; bar++) {
      if (text(state.chords[bar]) !== "") {
        dropped.push(bar + 1);
      }
      state.chords[bar] = "";
    }
    return dropped;
  }

  /* The hint under the chord row, for the meter (the beat of the second chord is the Composer's). */
  function chordHint(meter) {
    const beat = secondChordBeat(meter);
    const second = beat
      ? `a second chord only at @${beat}: I V@${beat}`
      : "one chord a bar in " + meter;
    return `Roman numerals in C, at most 2 per bar; @ sets the beat a chord starts on, from 0 (${second}).`;
  }

  /* The version body of a form state: {uses, theme}. The result is left out;
   * the backend fills the uses of the slot and accompaniment motifs. */
  function bodyFromForm(state) {
    const s = state || emptyState();
    const hybrid = s.into === "hybrid";
    const firstCadence = s.into !== "sentence" && firstCadences().includes(s.firstCadence) ? s.firstCadence : null;
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
      slots: slotEntries(s.slots).filter((entry) => formSlots(s).includes(entry.slot)),
      ideas: ideaEntries(s.ideas, ownIdeaNames(s)),
      chords: chordEntries(s),
      accompaniment,
      brief,
      style_weight: s.weight,
      without: withoutFromSwitches(s.switches),
      tempo: wholeNumber(s.tempo, 0, Infinity),
      check: Boolean(s.check),
      seed: wholeNumber(s.seed, 0, Infinity),
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
    if (intos().some((entry) => entry.value === form.into)) {
      state.into = form.into;
    }
    if (typeof form.hybrid === "string" && form.hybrid !== "") {
      state.hybrid = form.hybrid;
    }
    if (modes().some((entry) => entry.value === form.mode)) {
      state.mode = form.mode;
    }
    if (meters().includes(form.meter)) {
      state.meter = form.meter;
    }
    if (barChoices().includes(form.bars)) {
      state.bars = form.bars;
    }
    if (cadences().includes(form.cadence)) {
      state.cadence = form.cadence;
    }
    if (firstCadences().includes(form.first_cadence)) {
      state.firstCadence = form.first_cadence;
    }
    state.slots = slotEntries((Array.isArray(theme.slots) ? theme.slots : []).filter((slot) => slot && typeof slot === "object"));
    state.ideas = (Array.isArray(theme.ideas) ? theme.ideas : [])
      .filter((entry) => entry && typeof entry === "object")
      .map((entry) => ({ idea: entry.idea, item_id: entry.item_id, version: Number(entry.version) }));
    for (const entry of Array.isArray(theme.chords) ? theme.chords : []) {
      if (entry && Number.isInteger(entry.bar) && entry.bar >= 0 && entry.bar < CHORD_BARS && typeof entry.text === "string") {
        state.chords[entry.bar] = entry.text;
      }
    }
    state.modeChosen = state.slots.some((entry) => entry.slot === "basic");
    const acc = theme.accompaniment;
    if (acc && typeof acc === "object") {
      state.accompaniment =
        acc.pattern === "motive"
          ? { kind: "motive", pattern: "any", item_id: acc.item_id, version: Number(acc.version) }
          : { kind: "pattern", pattern: acc.pattern, item_id: "", version: 0 };
    }
    state.picks = picksFromBrief(theme.brief);
    state.switches = switchesFromWithout(theme.without);
    if (weights().some((entry) => entry.value === theme.style_weight)) {
      state.weight = theme.style_weight;
    }
    state.tempo = Number.isInteger(theme.tempo) ? String(theme.tempo) : "";
    state.check = theme.check === true;
    state.seed = Number.isInteger(theme.seed) ? String(theme.seed) : "";
    return state;
  }

  /* "major" or "minor" from a motif version body's key (`C minor`), null when the body names neither. */
  function motifMode(body) {
    const key = body && body.motif && typeof body.motif.key === "string" ? body.motif.key.toLowerCase() : "";
    if (key.includes("minor")) {
      return "minor";
    }
    return key.includes("major") ? "major" : null;
  }

  /* R-205: the first time the basic slot is set, the mode takes the motif's mode unless the mode was changed by
   * hand. Sets state.mode and answers true when it did. */
  function applyBasicMode(state, hadBasic, motifBody) {
    const mode = motifMode(motifBody);
    if (hadBasic || state.modeChosen || mode === null) {
      return false;
    }
    state.mode = mode;
    return true;
  }

  /* The [beginning, ending] pair of the form: a sentence is presentation+continuation, a period antecedent+consequent. */
  function pairing(state) {
    const s = state || emptyState();
    if (s.into === "sentence") {
      return ["presentation", "continuation"];
    }
    if (s.into === "period") {
      return ["antecedent", "consequent"];
    }
    return text(s.hybrid).split("+");
  }

  /* The form's ideas in order, each {idea, slot} with the slot it takes its motif from. */
  function formIdeas(state) {
    const [beginning, ending] = pairing(state);
    return [...halfIdeas("theme_beginnings", beginning), ...halfIdeas("theme_endings", ending)];
  }

  /* The slots the form has, in order: those its ideas take their motif from (the vocabulary's theme_ideas). */
  function formSlots(state) {
    const used = new Set(formIdeas(state).map((entry) => entry.slot));
    return slotNames().filter((name) => used.has(name));
  }

  /* The ideas that can have a motif of their own: those of the form but the basic idea (the basic slot). */
  function ownIdeaNames(state) {
    return formIdeas(state).map((entry) => entry.idea).filter((name) => name !== "basic idea");
  }

  /* The motif a slot's item belongs to: the item itself, or the end of its variant_of chain in items (at most 32 hops). */
  function rootMotif(items, id) {
    const list = Array.isArray(items) ? items : [];
    let current = id;
    for (let hop = 0; hop < 32; hop++) {
      const item = list.find((entry) => entry.id === current);
      if (!item || !item.variant_of) {
        return current;
      }
      current = item.variant_of;
    }
    return current;
  }

  /* The Use choices of a motif from VariantTree.treeRows rows: its saved variants, the root left out. A choice is
   * {value (item id), version, label, title (the derivation line)}. */
  function variantChoices(rows) {
    return (Array.isArray(rows) ? rows : [])
      .filter((row) => !row.isRoot)
      .map((row) => ({
        value: row.id,
        version: Number(row.version),
        label: `${"- ".repeat(Math.max(0, row.depth - 1))}${row.name}`,
        title: row.derivation || "",
      }));
  }

  /* What stops Compose before the Composer is asked, as a list of sentences: what the form lacks to be a request. The
   * Composer's own checks of the values (meter, bars, hybrid, tempo, seed, chords) come from the validate route. */
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
    if (s.mode === "style" && briefFromPicks(s.picks) === null) {
      problems.push("The mode from the brief needs a brief.");
    }
    if ((Array.isArray(s.ideas) ? s.ideas : []).some((entry) => entry && !text(entry.item_id))) {
      problems.push("Each idea with its own motif needs one.");
    }
    const acc = s.accompaniment || {};
    if (acc.kind === "motive" && !text(acc.item_id)) {
      problems.push("The accompaniment needs a motif.");
    }
    return problems;
  }

  /* The form fields the Composer's request paths name (a prefix of the path), for marking the field a refusal is
   * about. The chords are checked as a whole: a path under /chords or /harmony marks the chord row. */
  const FIELD_PATHS = [
    ["/into", "into"],
    ["/hybrid", "hybrid"],
    ["/mode", "mode"],
    ["/meter", "meter"],
    ["/bars", "bars"],
    ["/first_cadence", "firstCadence"],
    ["/cadence", "cadence"],
    ["/chords", "chords"],
    ["/harmony", "chords"],
    ["/accompaniment", "accompaniment"],
    ["/voicing", "accompaniment"],
    ["/style_weight", "weight"],
    ["/tempo", "tempo"],
    ["/seed", "seed"],
    ["/slots", "slots"],
    ["/ideas", "ideas"],
    ["/motives", "slots"],
  ];

  /* A JSON Pointer segment as the name it stands for. */
  function pointerName(segment) {
    return segment.replace(/~1/g, "/").replace(/~0/g, "~");
  }

  /* The form field a request path is about, or null. A path under /slots/<slot> names `slot:<slot>` (the row of that
   * slot) and one under /ideas/<name> names `idea:<name>` (the row of that idea). */
  function fieldForPath(path) {
    const value = typeof path === "string" ? path : "";
    const hit = /^\/(slots|ideas)\/([^/]+)/.exec(value);
    if (hit) {
      const name = pointerName(hit[2]);
      if (hit[1] === "ideas") {
        return `idea:${name}`;
      }
      return slotNames().includes(name) ? `slot:${name}` : "slots";
    }
    return Req.fieldForPath(value, FIELD_PATHS);
  }

  /* The bar (from 0) of the chord a path under /chords/<index> names, or null. The index is the entry of the chords
   * the form sends, which are the filled bars in order (chordEntries). */
  function chordBarOfPath(state, path) {
    const hit = /^\/chords\/(\d+)(\/|$)/.exec(typeof path === "string" ? path : "");
    if (!hit) {
      return null;
    }
    const entry = chordEntries(state)[Number(hit[1])];
    return entry ? entry.bar : null;
  }

  /* The request that asks the backend to check the form with the Composer (POST /api/materials/items/validate). */
  function validateRequest(state) {
    return Req.validateRequest("theme", bodyFromForm(state));
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
    return Req.jsonRequest("POST", itemUrl(state.id, "/versions"), request);
  }

  /* POST compose for the form state. options: {seed, update_uses}; a seed in
   * options wins over the form's. */
  function composeRequest(state, options) {
    const o = options || {};
    const body = {};
    const seed = o.seed !== undefined && o.seed !== null && o.seed !== "" ? wholeNumber(o.seed, 0, Infinity) : wholeNumber(state.seed, 0, Infinity);
    if (seed !== null) {
      body.seed = seed;
    }
    if (o.update_uses) {
      body.update_uses = true;
    }
    return Object.assign(Req.jsonRequest("POST", itemUrl(state.id, "/compose"), body), { job: true });
  }

  function versionFileUrl(id, number, name) {
    return `${itemUrl(id, "")}/versions/${Number(number)}/files/${encodeURIComponent(name)}`;
  }

  /* The playback the backend builds from one of a version's MIDI files (R-275): file is "theme" (a theme version) or
   * "loop" or "ending" (a piece version). The answer is the playback shape the score workspace reads. */
  function versionPlaybackUrl(id, number, file) {
    return `${itemUrl(id, "")}/versions/${Number(number)}/playback?file=${encodeURIComponent(file)}`;
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
    idea: "From its own motif",
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
      slot: slotLabels()[entry.slot] || entry.slot,
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

  /* {M1: "Name"} for the motive ids the backend gives the form: one per distinct motif and version, the slots in
   * order, then the ideas; nameOf(item_id) gives a motif's name. */
  function motiveNames(state, nameOf) {
    const names = {};
    const seen = [];
    const entries = [...slotEntries(state.slots), ...ideaEntries(state.ideas, ownIdeaNames(state))];
    for (const entry of entries) {
      if (!seen.some((other) => other.item_id === entry.item_id && other.version === entry.version)) {
        seen.push(entry);
        names[`M${seen.length}`] = nameOf ? nameOf(entry.item_id) || entry.item_id : entry.item_id;
      }
    }
    return names;
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
    get SLOT_NAMES() {
      return slotNames();
    },
    get SLOT_LABELS() {
      return slotLabels();
    },
    get INTOS() {
      return intos();
    },
    get BEGINNINGS() {
      return beginnings();
    },
    get ENDINGS() {
      return endings();
    },
    get MODES() {
      return modes();
    },
    get METERS() {
      return meters();
    },
    get BARS() {
      return barChoices();
    },
    CHORD_BARS,
    chordHint,
    chordEntries,
    chordsText,
    dropChords,
    get CADENCES() {
      return cadences();
    },
    get FIRST_CADENCES() {
      return firstCadences();
    },
    get POOLS() {
      return pools();
    },
    get PATTERNS() {
      return patterns();
    },
    get WEIGHTS() {
      return weights();
    },
    get MOTIVE_CATEGORIES() {
      return motiveCategories();
    },
    fieldForPath,
    chordBarOfPath,
    validateRequest,
    SWITCH_TEXT,
    THEME_TABS,
    readThemeTab,
    writeThemeTab,
    emptyState,
    bodyFromForm,
    formFromBody,
    formProblems,
    formSlots,
    formIdeas,
    rootMotif,
    variantChoices,
    motifMode,
    applyBasicMode,
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
    versionPlaybackUrl,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ThemesModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
