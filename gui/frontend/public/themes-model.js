/* Themes view (P16): the theme form state, the version body it makes, its
 * problems, the brief picks, the compose request and the rows of a result.
 * No DOM; themes-view.js renders from these. The form state is
 *   {id, uses, into, hybrid, mode, meter, bars, cadence, firstCadence,
 *    slots: [{slot, item_id, version}], ideas: [{idea, item_id, version}], accompaniment: {kind, pattern,
 *    item_id, version}, picks: [...], switches: {<category>: bool}, weight,
 *    tempo, check, seed}
 * with tempo and seed as the text typed (blank for none). */
(function (root) {
  const Materials = root.MaterialsModel || (typeof require === "function" ? require("./materials-model.js") : null);
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
  const CHORD_BARS = 8;
  const CHORD_TEXT_MAX = 64;
  /* The beats in a bar, and the beat of the one cell a second chord may start on (the Composer writes chords on
   * the cells of the meter only: 2 a bar in 4/4 and 6/8, 1 in 2/4 and 3/4). */
  const METER_BEATS = { "2/4": 2, "3/4": 3, "4/4": 4, "6/8": 6 };
  const SECOND_CHORD_BEAT = { "4/4": 2, "6/8": 3 };
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
  /* The ideas each half of a form has, in order, with the slot each takes its motif from (names as the Composer's). */
  const BEGINNING_IDEAS = {
    presentation: [["basic idea", "basic"], ["repetition", "basic"]],
    antecedent: [["basic idea", "basic"], ["contrasting idea", "contrasting"]],
    cbi: [["basic idea", "basic"], ["contrasting idea", "contrasting"]],
  };
  const ENDING_IDEAS = {
    continuation: [["fragment 1", "continuation"], ["fragment 2", "continuation"], ["cadential", "cadential"]],
    consequent: [["consequent basic idea", "basic"], ["consequent contrasting idea", "contrasting"]],
    cadential: [["cadential 1", "cadential"], ["cadential 2", "cadential"]],
  };
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
    return SLOT_NAMES.flatMap((name) => list.filter((entry) => entry && entry.slot === name)).map((entry) => ({
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

  /* What is wrong with the chords typed for the bars of the theme, as sentences naming the bar. A bar takes at most
   * 2 chords, each a numeral the harmony field accepts; onsets rise, are halves of a beat, stay inside the bar and
   * fall on the cells of the meter (the Composer refuses the rest). */
  function chordProblems(state) {
    const s = state || emptyState();
    const beats = METER_BEATS[s.meter] || 4;
    const second = SECOND_CHORD_BEAT[s.meter];
    const parse = Materials.parseHarmonyToken;
    const problems = [];
    for (const entry of chordEntries(s)) {
      const name = `Bar ${entry.bar + 1}`;
      const tokens = entry.text.split(/\s+/);
      if (tokens.length > 2) {
        problems.push(`${name}: at most 2 chords.`);
        continue;
      }
      if (tokens.length === 2 && !second) {
        problems.push(`${name}: ${s.meter} takes one chord a bar.`);
        continue;
      }
      let last = -1;
      tokens.forEach((token, index) => {
        const chord = parse(token);
        if (!chord) {
          problems.push(`${name}: "${token}" is not a chord (a Roman numeral such as IV or V6, then @ and the beat).`);
          return;
        }
        const onset = chord.onset === null ? (index === 0 ? 0 : null) : chord.onset;
        if (onset === null) {
          problems.push(`${name}: "${token}" needs an onset, such as ${token}@${second || 2}.`);
        } else if (!Number.isFinite(onset) || Math.round(onset * 2) !== onset * 2) {
          problems.push(`${name}: the onset of "${token}" must be a whole or half beat.`);
        } else if (onset >= beats) {
          problems.push(`${name}: the onset of "${token}" is past the bar (${beats} beats, from 0).`);
        } else if (onset <= last) {
          problems.push(`${name}: the onsets must rise.`);
        } else if (onset !== 0 && onset !== second) {
          problems.push(`${name}: in ${s.meter} a chord starts only ${second ? `at @${second}` : "on the downbeat"}.`);
        }
        last = Number.isFinite(onset) ? onset : last;
      });
    }
    return problems;
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

  /* The hint under the chord row, for the meter. */
  function chordHint(meter) {
    const beat = SECOND_CHORD_BEAT[meter];
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
      slots: slotEntries(s.slots).filter((entry) => formSlots(s).includes(entry.slot)),
      ideas: ideaEntries(s.ideas, ownIdeaNames(s)),
      chords: chordEntries(s),
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
    if (WEIGHTS.some((entry) => entry.value === theme.style_weight)) {
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

  /* The slots the form has, in order: basic always; contrasting for an antecedent or cbi beginning or a consequent
   * ending; continuation for a continuation ending; cadential for a continuation or cadential ending. */
  function formSlots(state) {
    const [beginning, ending] = pairing(state);
    const has = {
      basic: true,
      contrasting: beginning === "antecedent" || beginning === "cbi" || ending === "consequent",
      continuation: ending === "continuation",
      cadential: ending === "continuation" || ending === "cadential",
    };
    return SLOT_NAMES.filter((name) => has[name]);
  }

  /* The form's ideas in order, each {idea, slot} with the slot it takes its motif from. */
  function formIdeas(state) {
    const [beginning, ending] = pairing(state);
    return [...(BEGINNING_IDEAS[beginning] || []), ...(ENDING_IDEAS[ending] || [])].map(([idea, slot]) => ({ idea, slot }));
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
    if ((Array.isArray(s.ideas) ? s.ideas : []).some((entry) => entry && !text(entry.item_id))) {
      problems.push("Each idea with its own motif needs one.");
    }
    problems.push(...chordProblems(s));
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
    return Object.assign(jsonRequest("POST", itemUrl(state.id, "/compose"), body), { job: true });
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
    SLOT_NAMES,
    SLOT_LABELS,
    INTOS,
    BEGINNINGS,
    ENDINGS,
    MODES,
    METERS,
    BARS,
    CHORD_BARS,
    chordHint,
    chordEntries,
    chordProblems,
    chordsText,
    dropChords,
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
