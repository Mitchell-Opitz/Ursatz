/* Pieces view (P17): the piece form state, the version body it makes, its structure problems, the strip timeline,
 * the compose request and the rows of a result. No DOM; pieces-view.js renders from these. Needs themes-model.js for
 * the brief, the switches and the shared lists. The form state is
 *   {id, uses, form, key, mode, meter, tempo, bars, count, modulate, sections: [{id, part, letter, beats, pickup, cadence, fill}],
 *    motive: {item_id, version} | null, pool, accompaniment: {kind, pattern}, picks: [...], switches: {...},
 *    weight, check, seed}
 * with form the name of a Composer form (game-loop when a body names none), bars and count the size of a sized form (0 and 0
 * for its default size; such a form has no sections here), modulate a boolean, tempo and seed as the text typed (blank for
 * none), beats a whole number, pickup a string of beats ("0", "1/2"),
 * cadence null for the default and fill one of
 *   {kind: "empty"} | {kind: "motif", item_id, version, into, hybrid} | {kind: "theme", item_id, version}
 *   | {kind: "progression", numerals, cells}. */
(function (root) {
  const Req = root.ApiRequest || (typeof require === "function" ? require("./request.js") : null);
  const Themes = root.ThemesModel || (typeof require === "function" ? require("./themes-model.js") : null);

  /* The Composer's words (R-270, R-276), read when asked: the vocabulary is loaded after the scripts. */
  const Vocab = root.Vocabulary || (typeof require === "function" ? require("./vocabulary.js") : null);
  const DEFAULT_FORM = "game-loop";
  const cadences = () => Vocab.values("cadences");
  const fillKinds = () => Vocab.list("fill_kinds");
  const modes = () => Themes.MODES;
  const meters = () => Themes.METERS;
  const pools = () => Themes.POOLS;
  const weights = () => Themes.WEIGHTS;
  /* The bars of a theme without a construction (4 and 8) and the bars a theme may have with one (3 to 16). */
  const themeBars = () => Themes.REGULAR_BARS;
  const constructedBars = () => Themes.BARS;
  /* The pickups offered in a meter, in beats (any other multiple of 1/32 shorter than a bar is accepted too). */
  const pickups = (meter) => (Vocab.get().pickups || {})[meter] || [];
  /* A limit of the GUI's own: the Composer holds none for the cells of a chord. */
  const MAX_CELLS = 8;

  function text(value) {
    return typeof value === "string" ? value.trim() : "";
  }

  function wholeNumber(value, min, max) {
    const t = text(String(value === undefined || value === null ? "" : value));
    if (!/^\d+$/.test(t)) {
      return null;
    }
    const n = Number(t);
    return n >= min && n <= max ? n : null;
  }

  function emptyFill() {
    return { kind: "empty" };
  }

  function emptyState(id) {
    return {
      id: id || "",
      uses: [],
      form: DEFAULT_FORM,
      key: "C",
      mode: "major",
      meter: "4/4",
      tempo: "",
      bars: 0,
      count: 0,
      modulate: false,
      sections: [],
      motive: null,
      pool: "approved-and-unflagged",
      accompaniment: { kind: "none", pattern: "any" },
      picks: [],
      switches: Themes.switchesFromWithout([]),
      weight: "1/2",
      check: false,
      seed: "",
    };
  }

  /* --- forms ------------------------------------------------------------ */

  /* The Composer's entry of a form (its sections, sizes, slots, versions), or null. */
  function formOf(name) {
    return Vocab.form(name || DEFAULT_FORM);
  }

  /* True for a form whose sections the Composer gives by size; false for one whose request lists its sections. */
  function isSized(form) {
    return Boolean(form) && form.sections === "sizes";
  }

  /* The roles a request-listed form allows, in order: [{value, label, required}]. */
  function rolesOf(form) {
    return form && Array.isArray(form.roles) ? form.roles : [];
  }

  function roleValues(form) {
    return rolesOf(form).map((role) => role.value);
  }

  function partIndex(part, form) {
    return roleValues(form === undefined ? formOf(DEFAULT_FORM) : form).indexOf(part);
  }

  /* The label of a role value in a form: a listed role, else a role of one of its sizes, else the value. */
  function roleLabel(form, value) {
    const listed = rolesOf(form).find((role) => role.value === value);
    if (listed) {
      return listed.label;
    }
    for (const size of form && Array.isArray(form.sizes) ? form.sizes : []) {
      const found = size.sections.find((section) => section.role && section.role.value === value);
      if (found) {
        return found.role.label;
      }
    }
    return value;
  }

  /* "3 variations, 32 bars" for a size with a count (the noun is the role of its last section), else "16 bars". */
  function sizeLabel(size) {
    const last = size.sections.length > 0 ? size.sections[size.sections.length - 1].role : null;
    const noun = last && last.label ? last.label.toLowerCase() : "section";
    return size.count > 0 ? `${size.count} ${noun}${size.count === 1 ? "" : "s"}, ${size.bars} bars` : `${size.bars} bars`;
  }

  /* The sizes of a sized form: [{value, bars, count, default, label}] (value "<bars>:<count>"). */
  function sizeOptions(form) {
    if (!isSized(form)) {
      return [];
    }
    return form.sizes.map((size) => ({
      value: `${size.bars}:${size.count}`,
      bars: size.bars,
      count: size.count,
      default: size.default === true,
      label: sizeLabel(size),
    }));
  }

  /* The size bars and count name (0 and 0, or nothing a size has, is the default size), or null for a form with none. */
  function sizeOf(form, bars, count) {
    if (!isSized(form)) {
      return null;
    }
    const b = Number(bars) || 0;
    const c = Number(count) || 0;
    if (b === 0 && c === 0) {
      return form.sizes.find((size) => size.default === true) || form.sizes[0] || null;
    }
    return form.sizes.find((size) => (c > 0 ? size.count === c : size.bars === b && size.count === 0)) || null;
  }

  /* The sections of a sized form at a size, as the Composer plans them (for display); [] for a form with none. */
  function sectionsOf(form, bars, count) {
    const size = sizeOf(form, bars, count);
    return size ? size.sections : [];
  }

  /* A state on another form: a request-listed form starts with one section of its first required role, a sized form
   * with its default size and no sections. */
  function withForm(state, name) {
    const next = JSON.parse(JSON.stringify(state));
    const form = formOf(name);
    next.form = form ? form.name : DEFAULT_FORM;
    next.bars = 0;
    next.count = 0;
    if (!form || !form.settings || form.settings.modulate !== true) {
      next.modulate = false;
    }
    next.sections = [];
    if (form && !isSized(form)) {
      next.sections = [newSection(next, defaultRole(form))];
    }
    return next;
  }

  /* The role a new section of a form starts as: its first required role, else its first. */
  function defaultRole(form) {
    const roles = rolesOf(form);
    const required = roles.find((role) => role.required === true) || roles[0];
    return required ? required.value : "";
  }

  /* The beats in a bar of a meter, as the Composer lists it: 2/4 is 2, 6/8 is 6 (the eighth is the beat). A meter the
   * Composer does not list (a piece saved with another) reads its numerator. */
  function barBeats(meter) {
    const listed = (Vocab.entry("meters", meter) || {}).beats;
    if (Number.isInteger(listed)) {
      return listed;
    }
    const n = parseInt(meter, 10);
    return Number.isInteger(n) && n > 0 ? n : 4;
  }

  /* The beat unit of a meter as the denominator of a whole note: 4/4 is 4, 6/8 is 8. */
  function beatUnit(meter) {
    const d = parseInt(String(meter || "").split("/")[1], 10);
    return Number.isInteger(d) && d > 0 ? d : 4;
  }

  /* n or n/d as a number of beats, or null when the text is not that. */
  function rational(value) {
    const m = /^(\d+)(?:\/(\d+))?$/.exec(text(String(value === undefined || value === null ? "" : value)));
    if (!m) {
      return null;
    }
    const bottom = m[2] === undefined ? 1 : Number(m[2]);
    return bottom > 0 ? Number(m[1]) / bottom : null;
  }

  /* --- section ids and letters ------------------------------------------ */

  /* The next free id: s0, s1, ... ; ending sections after the first take the same scheme (the id "end" is the
   * Composer's own, a piece made here never needs it). */
  function nextSectionId(sections) {
    const used = new Set((Array.isArray(sections) ? sections : []).map((section) => section.id));
    let n = 0;
    while (used.has(`s${n}`)) {
      n++;
    }
    return `s${n}`;
  }

  /* The first letter A to Z not used by a section, then A1, B1, ... */
  function nextLetter(sections) {
    const used = new Set((Array.isArray(sections) ? sections : []).map((section) => section.letter));
    for (let round = 0; round < 40; round++) {
      for (let i = 0; i < 26; i++) {
        const letter = String.fromCharCode(65 + i) + (round > 0 ? String(round) : "");
        if (!used.has(letter)) {
          return letter;
        }
      }
    }
    return "S";
  }

  /* A new section for a part, after the last section of that part. The default is 8 bars of the meter. */
  function newSection(state, part) {
    const sections = Array.isArray(state.sections) ? state.sections : [];
    return {
      id: nextSectionId(sections),
      part,
      letter: nextLetter(sections),
      beats: barBeats(state.meter) * 8,
      pickup: "0",
      cadence: null,
      fill: emptyFill(),
    };
  }

  /* The index to insert a section of part at: after the last section of that part or an earlier one. */
  function insertIndex(sections, part, form) {
    const at = partIndex(part, form);
    let index = 0;
    sections.forEach((section, i) => {
      if (partIndex(section.part, form) <= at) {
        index = i + 1;
      }
    });
    return index;
  }

  function addSection(state, part) {
    const next = JSON.parse(JSON.stringify(state));
    const section = newSection(next, part);
    next.sections.splice(insertIndex(next.sections, part, formOf(next.form)), 0, section);
    return next;
  }

  function removeSection(state, id) {
    const next = JSON.parse(JSON.stringify(state));
    next.sections = next.sections.filter((section) => section.id !== id);
    return next;
  }

  /* Moves a section one place left (-1) or right (1) among the sections of its part. */
  function moveSection(state, id, direction) {
    const next = JSON.parse(JSON.stringify(state));
    const index = next.sections.findIndex((section) => section.id === id);
    if (index < 0) {
      return next;
    }
    const target = index + (direction < 0 ? -1 : 1);
    if (target < 0 || target >= next.sections.length || next.sections[target].part !== next.sections[index].part) {
      return next;
    }
    const moved = next.sections[index];
    next.sections[index] = next.sections[target];
    next.sections[target] = moved;
    return next;
  }

  /* --- body and form --------------------------------------------------- */

  /* A fill as the body keeps it. A fill that names nothing yet is empty, so every fill may be saved unfinished.
   * A motif fill keeps `into` for a theme length (4 or 8 whole bars, no pickup) or a section of 3 to 16 whole bars with a
   * construction and no pickup; any other is a free phrase. The construction (P47, R-468) is "auto" (the Composer chooses
   * it) or the given {ideas, extensions}; a fill with none has no member. */
  function fillFromState(fill, free) {
    const f = fill && typeof fill === "object" ? fill : emptyFill();
    switch (f.kind) {
      case "motif": {
        if (!text(f.item_id)) {
          return emptyFill();
        }
        const out = { kind: "motif", item_id: f.item_id, version: Number(f.version) };
        if (!free && Vocab.values("piece_intos").includes(f.into)) {
          out.into = f.into;
          out.hybrid = f.into === "hybrid" ? text(f.hybrid) : null;
          if (f.construction === "auto") {
            out.construction = "auto";
          } else if (f.construction && typeof f.construction === "object") {
            out.construction = Themes.constructionBody(f.construction, Themes.pairingIdeaNames(Themes.pairing(out)));
          }
        }
        return out;
      }
      case "theme":
        return text(f.item_id) ? { kind: "theme", item_id: f.item_id, version: Number(f.version) } : emptyFill();
      case "progression": {
        if (text(f.numerals) === "") {
          return emptyFill();
        }
        const out = { kind: "progression", numerals: text(f.numerals).split(/\s+/).join(" ") };
        const cells = wholeNumber(f.cells, 1, MAX_CELLS);
        if (cells !== null) {
          out.cells = cells;
        }
        return out;
      }
      default:
        return emptyFill();
    }
  }

  function fillFromBody(fill) {
    if (!fill || typeof fill !== "object") {
      return emptyFill();
    }
    switch (fill.kind) {
      case "motif":
        return {
          kind: "motif",
          item_id: fill.item_id,
          version: Number(fill.version),
          into: typeof fill.into === "string" ? fill.into : "",
          hybrid: typeof fill.hybrid === "string" ? fill.hybrid : "",
          construction: fill.construction === "auto" ? "auto" : Themes.constructionFromBody(fill.construction),
        };
      case "theme":
        return { kind: "theme", item_id: fill.item_id, version: Number(fill.version) };
      case "progression":
        return {
          kind: "progression",
          numerals: typeof fill.numerals === "string" ? fill.numerals : "",
          cells: Number.isInteger(fill.cells) ? String(fill.cells) : "",
        };
      default:
        return emptyFill();
    }
  }

  function sectionFromState(section, meter) {
    return {
      id: section.id,
      part: section.part,
      letter: text(section.letter) || null,
      beats: Number(section.beats),
      pickup: text(section.pickup) || "0",
      cadence: cadences().includes(section.cadence) ? section.cadence : null,
      fill: fillFromState(section.fill, motifIsFree(section, meter)),
    };
  }

  /* The version body of a form state: {uses, piece}. The result is left out; the backend fills the uses of the motif
   * fills. */
  function bodyFromState(state) {
    const s = state || emptyState();
    const acc = s.accompaniment || { kind: "none" };
    const form = formOf(s.form);
    const structure = { form: form ? form.name : DEFAULT_FORM, key: text(s.key), mode: s.mode, meter: s.meter };
    if (isSized(form)) {
      structure.bars = Number(s.bars) || 0;
      structure.count = Number(s.count) || 0;
      structure.modulate = form.settings && form.settings.modulate === true && s.modulate === true;
    } else {
      structure.sections = (Array.isArray(s.sections) ? s.sections : []).map((section) => sectionFromState(section, s.meter));
    }
    const piece = {
      structure,
      motive: s.motive && text(s.motive.item_id) ? { item_id: s.motive.item_id, version: Number(s.motive.version) } : null,
      pool: s.pool,
      accompaniment: acc.kind === "pattern" ? { pattern: acc.pattern || "any" } : null,
      brief: Themes.briefFromPicks(s.picks),
      style_weight: s.weight,
      without: Themes.withoutFromSwitches(s.switches),
      tempo: wholeNumber(s.tempo, 0, Infinity),
      check: Boolean(s.check),
      seed: wholeNumber(s.seed, 0, Infinity),
    };
    return { uses: Array.isArray(s.uses) ? s.uses.slice() : [], piece };
  }

  /* The form state of a version body. A placeholder piece (structure null, a string, an array or an object with no
   * sections) gives the defaults with no sections. */
  function stateFromBody(body, id) {
    const state = emptyState(id);
    const piece = body && body.piece && typeof body.piece === "object" ? body.piece : {};
    state.uses = body && Array.isArray(body.uses) ? body.uses.slice() : [];
    const structure = piece.structure && typeof piece.structure === "object" && !Array.isArray(piece.structure) ? piece.structure : {};
    const form = typeof structure.form === "string" ? formOf(structure.form) : null;
    state.form = form ? form.name : DEFAULT_FORM;
    if (typeof structure.key === "string" && structure.key !== "") {
      state.key = structure.key;
    }
    if (modes().some((entry) => entry.value === structure.mode)) {
      state.mode = structure.mode;
    }
    if (meters().includes(structure.meter)) {
      state.meter = structure.meter;
    }
    const kept = formOf(state.form);
    if (isSized(kept)) {
      state.bars = Number.isInteger(structure.bars) ? structure.bars : 0;
      state.count = Number.isInteger(structure.count) ? structure.count : 0;
      state.modulate = structure.modulate === true;
    }
    state.sections = (isSized(kept) || !Array.isArray(structure.sections) ? [] : structure.sections)
      .filter((section) => section && typeof section === "object" && roleValues(kept).includes(section.part))
      .map((section) => ({
        id: String(section.id),
        part: section.part,
        letter: typeof section.letter === "string" ? section.letter : "",
        beats: Number.isInteger(section.beats) ? section.beats : 0,
        pickup: typeof section.pickup === "string" ? section.pickup : "0",
        cadence: cadences().includes(section.cadence) ? section.cadence : null,
        fill: fillFromBody(section.fill),
      }));
    if (piece.motive && typeof piece.motive === "object" && text(piece.motive.item_id)) {
      state.motive = { item_id: piece.motive.item_id, version: Number(piece.motive.version) };
    }
    if (pools().some((entry) => entry.value === piece.pool)) {
      state.pool = piece.pool;
    }
    if (piece.accompaniment && typeof piece.accompaniment === "object" && typeof piece.accompaniment.pattern === "string") {
      state.accompaniment = { kind: "pattern", pattern: piece.accompaniment.pattern };
    }
    state.picks = Themes.picksFromBrief(piece.brief);
    state.switches = Themes.switchesFromWithout(piece.without);
    if (weights().some((entry) => entry.value === piece.style_weight)) {
      state.weight = piece.style_weight;
    }
    state.tempo = Number.isInteger(piece.tempo) ? String(piece.tempo) : "";
    state.check = piece.check === true;
    state.seed = Number.isInteger(piece.seed) ? String(piece.seed) : "";
    return state;
  }

  /* --- beats ----------------------------------------------------------- */

  /* "2 bars", "6 beats", "1 bar 2 beats". */
  function beatsLabel(beats, meter) {
    const total = Number(beats);
    if (!Number.isFinite(total) || total < 0) {
      return "";
    }
    const per = barBeats(meter);
    const bars = Math.floor(total / per);
    const rest = total - bars * per;
    const parts = [];
    if (bars > 0) {
      parts.push(`${bars} ${bars === 1 ? "bar" : "bars"}`);
    }
    if (rest > 0 || bars === 0) {
      parts.push(`${rest} ${rest === 1 ? "beat" : "beats"}`);
    }
    return parts.join(" ");
  }

  /* Each section's start and length in beats, the pickup not counted: [{id, part, letter, start, beats}]. */
  function sectionTimeline(state) {
    let start = 0;
    return (Array.isArray(state && state.sections) ? state.sections : []).map((section) => {
      const beats = Number.isFinite(Number(section.beats)) ? Number(section.beats) : 0;
      const entry = { id: section.id, part: section.part, letter: section.letter, start, beats };
      start += beats;
      return entry;
    });
  }

  /* The cadence of every section in a validate reply's request, in section order (the Composer fills those a section
   * leaves unchosen); empty when the reply has none or its sections are not the form's. */
  function servedCadences(request, state) {
    const sections = request && Array.isArray(request.sections) ? request.sections : [];
    const count = Array.isArray(state && state.sections) ? state.sections.length : 0;
    if (sections.length !== count) {
      return [];
    }
    return sections.map((section) => (section && typeof section.cadence === "string" ? section.cadence : ""));
  }

  /* The cadence to show for the section at index: the chosen one, else the one the Composer served, else "" (not
   * asked yet). */
  function cadenceOf(state, index, served) {
    const section = state.sections[index];
    if (section && section.cadence) {
      return section.cadence;
    }
    return Array.isArray(served) && typeof served[index] === "string" ? served[index] : "";
  }

  /* --- themes ---------------------------------------------------------- */

  /* The length in beats of a composed theme version body, or null when it has no form. */
  function themeLengthBeats(body) {
    const form = body && body.theme && body.theme.form;
    if (!form || !Number.isInteger(form.bars)) {
      return null;
    }
    return form.bars * barBeats(form.meter);
  }

  /* One line for a theme version body: "sentence in D minor, 4/4, 8 bars". */
  function themeFormText(body) {
    const form = body && body.theme && body.theme.form;
    if (!form) {
      return "";
    }
    const into = form.into === "hybrid" && form.hybrid ? `hybrid (${form.hybrid})` : form.into;
    return `${into} in ${form.key} ${form.mode}, ${form.meter}, ${form.bars} bars`;
  }

  /* True when the theme version body was composed (it has a result). */
  function themeComposed(body) {
    return Boolean(body && body.theme && body.theme.result && typeof body.theme.result === "object");
  }

  /* The key of a theme fill in a themeBeats table. */
  function fillKey(fill) {
    return `${fill.item_id}@${fill.version}`;
  }

  /* --- problems -------------------------------------------------------- */

  /* True when the section is whole bars (3 to 16) with no pickup: it may be a theme with a construction. */
  function constructionPossible(section, meter) {
    const bars = Number(section.beats) / barBeats(meter);
    const pickup = rational(section.pickup);
    return constructedBars().includes(bars) && (pickup === null || pickup === 0);
  }

  /* A motif fill takes its theme form (sentence, period, hybrid) at 4 or 8 whole bars without a pickup, or at any whole
   * 3 to 16 bars with a construction chosen or given; any other length is a free phrase. */
  function motifIsFree(section, meter) {
    if (!constructionPossible(section, meter)) {
      return true;
    }
    const bars = Number(section.beats) / barBeats(meter);
    const construction = section.fill && section.fill.construction;
    return !(themeBars().includes(bars) || construction === "auto" || (construction && typeof construction === "object"));
  }

  /* The construction choice of a motif fill: "default" (a theme of 4 or 8 bars as today, else a free phrase), "auto" or
   * "given". */
  function constructionChoice(fill) {
    const construction = fill && fill.construction;
    if (construction === "auto") {
      return "auto";
    }
    return construction && typeof construction === "object" ? "given" : "default";
  }

  /* What stops Compose or Save, as a list of sentences. options.themeBeats(fill) gives the length in beats of the
   * theme a theme fill names, or null when it is not known. */
  function structureProblems(state, options) {
    const s = state || emptyState();
    const o = options || {};
    const problems = [];
    const form = formOf(s.form);
    if (!form) {
      return [`The Composer has no form called ${s.form}.`];
    }
    if (isSized(form)) {
      return problems;
    }
    const sections = Array.isArray(s.sections) ? s.sections : [];
    for (const role of rolesOf(form)) {
      if (role.required === true && !sections.some((section) => section.part === role.value)) {
        problems.push(`The piece needs a section in the ${role.label}.`);
      }
    }
    let last = -1;
    let ordered = true;
    for (const section of sections) {
      const at = partIndex(section.part, form);
      if (at < last) {
        ordered = false;
      }
      last = Math.max(last, at);
    }
    if (!ordered) {
      problems.push(`The parts must come in the order ${rolesOf(form).map((role) => role.label).join(", ")}.`);
    }
    for (const section of sections) {
      const name = section.letter ? `Section ${section.letter}` : `Section ${section.id}`;
      const beats = Number(section.beats);
      if (!Number.isInteger(beats) || beats < 1) {
        problems.push(`${name} must be at least a beat long.`);
      }
      const fill = section.fill || {};
      if (fill.kind === "theme" && typeof o.themeBeats === "function") {
        const themeBeats = o.themeBeats(fill);
        if (Number.isFinite(themeBeats) && themeBeats !== beats) {
          problems.push(`${name} is ${beatsLabel(beats, s.meter)} but its theme is ${beatsLabel(themeBeats, s.meter)}.`);
        }
      }
    }
    return problems;
  }

  /* What stops Compose beyond the structure: the mode from the brief needs a brief. The Composer's own checks of the
   * values (key, meter, tempo, seed, pickups, numerals, sizes) come from the validate route. */
  function formProblems(state, options) {
    const s = state || emptyState();
    const problems = structureProblems(s, options);
    if (s.mode === "style" && Themes.briefFromPicks(s.picks) === null) {
      problems.push("The mode from the brief needs a brief.");
    }
    return problems;
  }

  /* The form fields the Composer's request paths name (a prefix of the path), for marking the field a refusal is
   * about; anything under /sections marks the section list. */
  const FIELD_PATHS = [
    ["/key", "key"],
    ["/mode", "mode"],
    ["/meter", "meter"],
    ["/tempo", "tempo"],
    ["/seed", "seed"],
    ["/sections", "sections"],
    ["/bars", "size"],
    ["/count", "size"],
    ["/modulate", "modulate"],
    ["/motive", "motive"],
    ["/accompaniment", "accompaniment"],
    ["/style_weight", "weight"],
  ];

  /* The form field a request path is about, or null. */
  function fieldForPath(path) {
    return Req.fieldForPath(path, FIELD_PATHS);
  }

  /* The request that asks the backend to check the form with the Composer (POST /api/materials/items/validate). */
  function validateRequest(state) {
    return Req.validateRequest("piece", bodyFromState(state));
  }

  /* --- requests -------------------------------------------------------- */

  function itemUrl(id, tail) {
    return `/api/materials/items/${encodeURIComponent(id)}${tail}`;
  }

  /* A new version of the piece holding the form (no result). */
  function saveRequest(state, note) {
    const request = { body: bodyFromState(state) };
    if (text(note)) {
      request.note = text(note);
    }
    return { method: "POST", url: itemUrl(state.id, "/versions"), headers: { "Content-Type": "application/json" }, body: JSON.stringify(request) };
  }

  /* POST compose for the form state. options: {seed, update_uses}; a seed in options wins over the form's. */
  function composeRequest(state, options) {
    const o = options || {};
    const body = {};
    const seed =
      o.seed !== undefined && o.seed !== null && o.seed !== "" ? wholeNumber(o.seed, 0, Infinity) : wholeNumber(state.seed, 0, Infinity);
    if (seed !== null) {
      body.seed = seed;
    }
    if (o.update_uses) {
      body.update_uses = true;
    }
    return { method: "POST", url: itemUrl(state.id, "/compose"), headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), job: true };
  }

  function versionFileUrl(id, number, name) {
    return Themes.versionFileUrl(id, number, name);
  }

  function versionPlaybackUrl(id, number, file) {
    return Themes.versionPlaybackUrl(id, number, file);
  }

  /* --- regenerate and variations ---------------------------------------- */

  const LANES = ["harmony", "bass", "melody", "inner"];
  const MAX_VARIATIONS = 8;

  /* The lanes of a section that can be written again (R-173): only a section of the first version (rows, as versionView
   * gives them) can be; a section filled with a theme takes harmony and bass only (the backend refuses the others). With no
   * rows every section is asked. */
  function lanesFor(section, rows) {
    if (!section || (Array.isArray(rows) && !rows.some((row) => row.id === section.id))) {
      return [];
    }
    return section.fill && section.fill.kind === "theme" ? ["harmony", "bass"] : LANES.slice();
  }

  /* The sections that offer regenerate and variations, from the first version's rows (resultView `versions[0].view.rows`):
   * [{id, letter, length, lanes}]. A form whose request lists its sections takes them from the state (their fills limit the
   * lanes); a sized form has none in the state and takes the rows. */
  function actionSections(state, rows) {
    const list = Array.isArray(rows) ? rows : [];
    if (isSized(formOf(state.form))) {
      return list.map((row) => ({ id: row.id, letter: row.letter || row.id, length: row.length, lanes: lanesFor({ id: row.id }, list) }));
    }
    return (state.sections || [])
      .map((section) => ({ id: section.id, letter: section.letter || section.id, length: beatsLabel(section.beats, state.meter), lanes: lanesFor(section, list) }))
      .filter((entry) => entry.lanes.length > 0);
  }

  /* The seed typed, blank for none: {value} (undefined for none) or {error}. */
  function optionalSeed(value) {
    if (text(String(value === undefined || value === null ? "" : value)) === "") {
      return { value: undefined };
    }
    const seed = wholeNumber(value, 0, Infinity);
    return seed === null ? { error: "The seed is a whole number, or blank." } : { value: seed };
  }

  /* POST regenerate of a section of a composed version. options: {section_id, lane ("" or "all" for every lane), seed}.
   * Returns {request} or {error}. */
  function regenerateRequest(id, number, options) {
    const o = options || {};
    const seed = optionalSeed(o.seed);
    if (seed.error) {
      return { error: seed.error };
    }
    if (!text(o.section_id)) {
      return { error: "Choose a section." };
    }
    const body = { section_id: o.section_id };
    if (text(o.lane) !== "") {
      body.lane = o.lane;
    }
    if (seed.value !== undefined) {
      body.seed = seed.value;
    }
    return {
      request: { method: "POST", url: itemUrl(id, `/versions/${Number(number)}/regenerate`), headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), job: true },
    };
  }

  /* POST variations of a section. options: {section_id, lane, count, seed}; the lane is needed. */
  function variationsRequest(id, number, options) {
    const o = options || {};
    const seed = optionalSeed(o.seed);
    if (seed.error) {
      return { error: seed.error };
    }
    const count = wholeNumber(o.count, 1, MAX_VARIATIONS);
    if (count === null) {
      return { error: `The count is 1 to ${MAX_VARIATIONS}.` };
    }
    if (!text(o.section_id) || text(o.lane) === "") {
      return { error: "Choose a section and a lane." };
    }
    const body = { section_id: o.section_id, lane: o.lane, count };
    if (seed.value !== undefined) {
      body.seed = seed.value;
    }
    return {
      request: { method: "POST", url: itemUrl(id, `/versions/${Number(number)}/variations`), headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), job: true },
    };
  }

  /* POST choose: variation index of set becomes the next version. */
  function chooseRequest(id, number, set, index) {
    return {
      method: "POST",
      url: itemUrl(id, `/versions/${Number(number)}/variations/${Number(set)}/choose`),
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ index: Number(index) }),
      job: true,
    };
  }

  /* A file of a variations set: variation-<i>.musicxml, .mid, .json or result.json. */
  function variationFileUrl(id, number, set, name) {
    return itemUrl(id, `/versions/${Number(number)}/variations/${Number(set)}/${encodeURIComponent(name)}`);
  }

  /* The cards of a variations answer {set, variations: [{index, seed, score, seam, heard}]}. */
  function variationCards(answer) {
    const list = answer && Array.isArray(answer.variations) ? answer.variations : [];
    return list.map((entry) => {
      const score = entry.score && Number.isFinite(entry.score.numerator) ? `${entry.score.numerator} of ${entry.score.denominator}` : "";
      const seam = entry.seam && typeof entry.seam === "object" ? entry.seam.held : entry.seam;
      return {
        index: entry.index,
        title: `#${entry.index} · seed ${entry.seed}`,
        score,
        seam: Number.isFinite(seam) ? `${seam} of ${SEAM_RULES.length} seam rules` : "",
        heard: typeof entry.heard === "string" ? entry.heard : "",
      };
    });
  }

  /* --- the result ------------------------------------------------------ */

  const FROM_TEXT = { request: "set here", option: "set here", style: "from the brief", none: "not set" };
  const DISTANCE_LABELS = ["harmony", "rhythm", "melody", "form", "overall"];
  const FILL_TEXT = { empty: "empty", motif: "motif", theme: "theme", progression: "progression" };
  const SEAM_RULES = [
    { key: "progression", label: "The chords lead back" },
    { key: "no_parallels", label: "No parallel perfect intervals across the seam" },
    { key: "step", label: "The melody leaps no further than it can recover" },
    { key: "leading_tone", label: "The leading tone resolves" },
  ];

  /* A time "n/d" in whole notes as the beat count of a meter, or null. */
  function beatsOfTime(time, meter) {
    const m = /^(\d+)\/(\d+)$/.exec(String(time || ""));
    if (!m || Number(m[2]) === 0) {
      return null;
    }
    return (Number(m[1]) / Number(m[2])) * beatUnit(meter);
  }

  /* "bar 3" or "bar 3 beat 2" for a beat count. */
  function positionLabel(beats, meter) {
    if (!Number.isFinite(beats)) {
      return "";
    }
    const per = barBeats(meter);
    const bar = Math.floor(beats / per) + 1;
    const beat = beats - (bar - 1) * per;
    const shown = Number.isInteger(beat) ? String(beat + 1) : String(Math.round((beat + 1) * 100) / 100);
    return beat === 0 ? `bar ${bar}` : `bar ${bar} beat ${shown}`;
  }

  /* Where a section's material came from: the Composer's words, or for a theme fill (its plan's path) the name the
   * caller gives that section in names ({<section id>: "Theme One v2"}). */
  function sourceText(source, names) {
    if (typeof source !== "string") {
      return "";
    }
    const theme = /(?:^|\/)theme-(.+)\.plan\.json$/.exec(source);
    if (theme) {
      return (names && names[theme[1]]) || "theme";
    }
    return source;
  }

  function versionView(version, meter, options) {
    const v = version && typeof version === "object" ? version : null;
    if (!v) {
      return null;
    }
    const planned = options && options.form && isSized(options.form) ? sectionsOf(options.form, options.bars, options.count) : [];
    const rows = (Array.isArray(v.sections) ? v.sections : []).map((section) => {
      const beats = beatsOfTime(section.start, meter);
      const part = section.part || section.role || "";
      const entry = planned.find((item) => item.name === section.id);
      return {
        id: section.id,
        part,
        partLabel: roleLabel(options && options.form, part),
        letter: section.letter || "",
        start: positionLabel(beats, meter),
        startBeats: beats,
        beats: section.beats,
        length: beatsLabel(section.beats, meter),
        cadence: section.cadence || "",
        heard: section.heard || "",
        fill: FILL_TEXT[section.fill] || section.fill || "",
        source: sourceText(section.source, options && options.sourceNames),
        keyHeard: section.key_heard || "",
        pedal: entry && entry.pedal && entry.pedal.label ? entry.pedal.label : "",
        returns: section.returns || "",
        changes: (section.changes && section.changes.text) || "",
        /* A section with a construction (P47) carries the sentence that says why it is as long as it is; no other has the member. */
        construction: typeof section.construction_text === "string" ? section.construction_text : "",
      };
    });
    const seam = v.seam && typeof v.seam === "object" ? v.seam : null;
    const check = v.analyzer_check && typeof v.analyzer_check === "object" ? v.analyzer_check : null;
    const distance = v.style_distance && typeof v.style_distance === "object" ? v.style_distance : null;
    const score = v.score && Number.isFinite(v.score.numerator) && Number.isFinite(v.score.denominator) ? v.score : null;
    return {
      rows,
      seam: seam
        ? {
            held: seam.held,
            total: SEAM_RULES.length,
            label: `${seam.held} of ${SEAM_RULES.length} seam rules held`,
            rules: SEAM_RULES.map((rule) => ({ label: rule.label, held: seam[rule.key] === true })),
          }
        : null,
      score: score ? { numerator: score.numerator, denominator: score.denominator, label: `${score.numerator} of ${score.denominator}` } : null,
      check: check
        ? {
            planned: check.planned,
            agreed: check.agreed,
            contradicts: check.contradicts === true,
            label: `${check.agreed} of ${check.planned} planned items heard${check.contradicts ? ", contradicted" : ""}`,
          }
        : null,
      distance: distance
        ? DISTANCE_LABELS.filter((label) => Number.isFinite(distance[label])).map((label) => ({ label, value: distance[label].toFixed(4) }))
        : [],
    };
  }

  /* A time "n/d" as a number of whole notes, or null. */
  function wholeNotes(time) {
    const m = /^(\d+)\/(\d+)$/.exec(String(time || ""));
    return m && Number(m[2]) !== 0 ? Number(m[1]) / Number(m[2]) : null;
  }

  /* The version of a form that loops (the form's versions say which), or null. */
  function loopVersion(form) {
    const found = form && Array.isArray(form.versions) ? form.versions.find((version) => version.loop === true) : null;
    return found ? found.value : null;
  }

  /* What loop playback needs, in whole notes: the loop region and where the last body section ends (the ending takes
   * over there: the last section whose role is a loop role other than the last), or null when the form has no loop, the
   * summary has none or no body section. */
  function playbackLoop(summary, form) {
    const s = summary && typeof summary === "object" ? summary : {};
    const f = form === undefined ? formOf(DEFAULT_FORM) : form;
    const name = loopVersion(f);
    const loop = s.loop && typeof s.loop === "object" ? s.loop : null;
    const start = loop ? wholeNotes(loop.start) : null;
    const end = loop ? wholeNotes(loop.end) : null;
    const version = name && s.versions && s.versions[name] && Array.isArray(s.versions[name].sections) ? s.versions[name] : null;
    if (start === null || end === null || !(end > start) || !version) {
      return null;
    }
    const bodyRoles = (Array.isArray(f.loop_roles) ? f.loop_roles : []).slice(0, -1);
    const unit = beatUnit(s.meter);
    let bodyEnd = null;
    for (const section of version.sections) {
      const at = wholeNotes(section.start);
      if (bodyRoles.includes(section.part) && at !== null && Number.isFinite(section.beats)) {
        bodyEnd = Math.max(bodyEnd === null ? 0 : bodyEnd, at + section.beats / unit);
      }
    }
    return bodyEnd === null ? null : { start, end, bodyEnd };
  }

  /* The versions of a summary in the form's order, then any other: [{name, label, loop, after, view}]. */
  function versionViews(versions, form, meter, options) {
    const listed = form && Array.isArray(form.versions) ? form.versions : [];
    const names = listed.map((version) => version.value).filter((name) => versions[name]);
    for (const name of Object.keys(versions)) {
      if (!names.includes(name)) {
        names.push(name);
      }
    }
    return names
      .map((name) => {
        const entry = listed.find((version) => version.value === name) || {};
        return {
          name,
          label: entry.label || name,
          loop: entry.loop === true,
          after: typeof entry.after === "string" ? entry.after : null,
          view: versionView(versions[name], meter, options),
        };
      })
      .filter((entry) => entry.view !== null);
  }

  /* The Composer's summary of a piece version's result as rows to show. result is piece.result ({seed, summary, brief,
   * loop, files}); options carries sourceNames ({<section id>: "Theme One v2"}) for the sections filled with a theme and
   * form (the form's name, the game loop when absent) for the labels and the loop. */
  function resultView(result, options) {
    const r = result && typeof result === "object" ? result : {};
    const form = formOf(options && options.form);
    const rowOptions = Object.assign({}, options, { form });
    const summary = r.summary && typeof r.summary === "object" ? r.summary : {};
    const meter = typeof summary.meter === "string" ? summary.meter : "4/4";
    const versions = summary.versions && typeof summary.versions === "object" ? summary.versions : {};
    const brief = summary.brief || {};
    const tempo = brief.tempo || {};
    const mode = brief.mode || {};
    const without = Array.isArray(brief.without) ? brief.without : [];
    const loop = summary.loop && typeof summary.loop === "object" ? summary.loop : null;
    const pieces = r.brief && typeof r.brief === "object" ? r.brief : {};
    const startBeats = loop ? beatsOfTime(loop.start, meter) : null;
    const endBeats = loop ? beatsOfTime(loop.end, meter) : null;
    return {
      seed: Number.isInteger(r.seed) ? r.seed : null,
      key: typeof summary.key === "string" ? summary.key : "",
      meter,
      loop: loop && startBeats !== null && endBeats !== null ? `${positionLabel(startBeats, meter)} to ${positionLabel(endBeats, meter)}` : "",
      playLoop: playbackLoop(summary, form),
      loopLength: startBeats !== null && endBeats !== null ? beatsLabel(endBeats - startBeats, meter) : "",
      versions: versionViews(versions, form, meter, rowOptions),
      brief: {
        tempo: Number.isFinite(tempo.qpm) ? `${tempo.qpm} (${FROM_TEXT[tempo.from] || tempo.from})` : "not set",
        mode: mode.mode ? `${mode.mode} (${FROM_TEXT[mode.from] || mode.from})` : "",
        density: Number.isFinite(brief.density) ? `${brief.density} attacks per beat` : "not set",
        without: without.length > 0 ? without.join(", ") : "none",
      },
      pieces:
        Number.isInteger(pieces.piece_count) && pieces.piece_count > 0
          ? `${pieces.piece_count} ${pieces.piece_count === 1 ? "piece" : "pieces"} in the brief` +
            (Array.isArray(pieces.skipped) && pieces.skipped.length > 0 ? `, ${pieces.skipped.length} skipped` : "")
          : "",
      files: Array.isArray(r.files) ? r.files.slice() : [],
    };
  }

  /* --- sources --------------------------------------------------------- */

  /* The motifs and themes with a newer version, from the item answer's uses_status. */
  function updatedUses(item) {
    return Themes.updatedUses(item);
  }

  const api = {
    DEFAULT_FORM,
    formOf,
    isSized,
    rolesOf,
    roleLabel,
    defaultRole,
    sizeOptions,
    sizeLabel,
    sizeOf,
    sectionsOf,
    withForm,
    get CADENCES() {
      return cadences();
    },
    get FILL_KINDS() {
      return fillKinds();
    },
    get INTOS() {
      return Themes.INTOS;
    },
    get MODES() {
      return Themes.MODES;
    },
    get METERS() {
      return Themes.METERS;
    },
    get POOLS() {
      return Themes.POOLS;
    },
    get PATTERNS() {
      return Themes.PATTERNS;
    },
    get WEIGHTS() {
      return Themes.WEIGHTS;
    },
    pickups,
    MAX_CELLS,
    fieldForPath,
    validateRequest,
    emptyState,
    emptyFill,
    barBeats,
    newSection,
    addSection,
    removeSection,
    moveSection,
    bodyFromState,
    stateFromBody,
    beatsLabel,
    sectionTimeline,
    servedCadences,
    cadenceOf,
    motifIsFree,
    constructionPossible,
    constructionChoice,
    themeLengthBeats,
    themeFormText,
    themeComposed,
    fillKey,
    structureProblems,
    formProblems,
    saveRequest,
    composeRequest,
    versionFileUrl,
    versionPlaybackUrl,
    LANES,
    MAX_VARIATIONS,
    lanesFor,
    actionSections,
    optionalSeed,
    regenerateRequest,
    variationsRequest,
    chooseRequest,
    variationFileUrl,
    variationCards,
    resultView,
    positionLabel,
    updatedUses,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.PiecesModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
