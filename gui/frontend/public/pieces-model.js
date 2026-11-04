/* Pieces view (P17): the piece form state, the version body it makes, its structure problems, the strip timeline,
 * the compose request and the rows of a result. No DOM; pieces-view.js renders from these. Needs themes-model.js for
 * the brief, the switches and the shared lists. The form state is
 *   {id, uses, key, mode, meter, tempo, sections: [{id, part, letter, beats, pickup, cadence, fill}],
 *    motive: {item_id, version} | null, pool, accompaniment: {kind, pattern}, picks: [...], switches: {...},
 *    weight, check, seed}
 * with tempo and seed as the text typed (blank for none), beats a whole number, pickup a string of beats ("0", "1/2"),
 * cadence null for the default and fill one of
 *   {kind: "empty"} | {kind: "motif", item_id, version, into, hybrid} | {kind: "theme", item_id, version}
 *   | {kind: "progression", numerals, cells}. */
(function (root) {
  const Themes = root.ThemesModel || (typeof require === "function" ? require("./themes-model.js") : null);

  const PARTS = [
    { value: "intro", label: "Intro" },
    { value: "body", label: "Loop body" },
    { value: "turnaround", label: "Turnaround" },
    { value: "ending", label: "Ending" },
  ];
  const PART_NAMES = PARTS.map((entry) => entry.value);
  const CADENCES = ["PAC", "IAC", "HC", "DC", "PC"];
  const FILL_KINDS = [
    { value: "empty", label: "Empty" },
    { value: "motif", label: "Motif" },
    { value: "theme", label: "Theme" },
    { value: "progression", label: "Progression" },
  ];
  const INTOS = Themes.INTOS;
  const MODES = Themes.MODES;
  const METERS = Themes.METERS;
  const POOLS = Themes.POOLS;
  const PATTERNS = Themes.PATTERNS;
  const WEIGHTS = Themes.WEIGHTS;
  const PICKUPS = ["0", "1/4", "1/2", "1", "3/2", "2", "3"];
  const MAX_SECTIONS = 32;
  const MAX_BEATS = 2048;
  const MAX_CELLS = 8;
  const THEME_BARS = [4, 8];
  const MAX_SEED = 4294967295;

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
      key: "C",
      mode: "major",
      meter: "4/4",
      tempo: "",
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

  function partIndex(part) {
    return PART_NAMES.indexOf(part);
  }

  /* The beats in a bar of a meter: 2/4 is 2, 6/8 is 6 (the eighth is the beat). */
  function barBeats(meter) {
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
  function insertIndex(sections, part) {
    const at = partIndex(part);
    let index = 0;
    sections.forEach((section, i) => {
      if (partIndex(section.part) <= at) {
        index = i + 1;
      }
    });
    return index;
  }

  function addSection(state, part) {
    const next = JSON.parse(JSON.stringify(state));
    const section = newSection(next, part);
    next.sections.splice(insertIndex(next.sections, part), 0, section);
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

  /* The numerals of a progression fill: words of the marks the Composer reads, 1 to 64 of them. */
  function numeralsOk(value) {
    const words = text(value).split(/\s+/);
    return text(value) !== "" && words.length <= 64 && words.every((word) => /^[A-Za-z0-9#+/-]{1,16}$/.test(word));
  }

  /* A fill as the body keeps it. A fill that names nothing yet is empty, so every fill may be saved unfinished.
   * A motif fill keeps `into` only for a theme length (4 or 8 whole bars, no pickup); any other is a free phrase. */
  function fillFromState(fill, free) {
    const f = fill && typeof fill === "object" ? fill : emptyFill();
    switch (f.kind) {
      case "motif": {
        if (!text(f.item_id)) {
          return emptyFill();
        }
        const out = { kind: "motif", item_id: f.item_id, version: Number(f.version) };
        if (!free && (f.into === "sentence" || f.into === "period" || f.into === "hybrid")) {
          out.into = f.into;
          out.hybrid = f.into === "hybrid" ? text(f.hybrid) : null;
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
      cadence: CADENCES.includes(section.cadence) ? section.cadence : null,
      fill: fillFromState(section.fill, motifIsFree(section, meter)),
    };
  }

  /* The version body of a form state: {uses, piece}. The result is left out; the backend fills the uses of the motif
   * fills. */
  function bodyFromState(state) {
    const s = state || emptyState();
    const acc = s.accompaniment || { kind: "none" };
    const piece = {
      structure: {
        key: text(s.key),
        mode: s.mode,
        meter: s.meter,
        sections: (Array.isArray(s.sections) ? s.sections : []).map((section) => sectionFromState(section, s.meter)),
      },
      motive: s.motive && text(s.motive.item_id) ? { item_id: s.motive.item_id, version: Number(s.motive.version) } : null,
      pool: s.pool,
      accompaniment: acc.kind === "pattern" ? { pattern: acc.pattern || "any" } : null,
      brief: Themes.briefFromPicks(s.picks),
      style_weight: s.weight,
      without: Themes.withoutFromSwitches(s.switches),
      tempo: wholeNumber(s.tempo, 20, 400),
      check: Boolean(s.check),
      seed: wholeNumber(s.seed, 0, MAX_SEED),
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
    if (typeof structure.key === "string" && structure.key !== "") {
      state.key = structure.key;
    }
    if (MODES.some((entry) => entry.value === structure.mode)) {
      state.mode = structure.mode;
    }
    if (METERS.includes(structure.meter)) {
      state.meter = structure.meter;
    }
    state.sections = (Array.isArray(structure.sections) ? structure.sections : [])
      .filter((section) => section && typeof section === "object" && PART_NAMES.includes(section.part))
      .map((section) => ({
        id: String(section.id),
        part: section.part,
        letter: typeof section.letter === "string" ? section.letter : "",
        beats: Number.isInteger(section.beats) ? section.beats : 0,
        pickup: typeof section.pickup === "string" ? section.pickup : "0",
        cadence: CADENCES.includes(section.cadence) ? section.cadence : null,
        fill: fillFromBody(section.fill),
      }));
    if (piece.motive && typeof piece.motive === "object" && text(piece.motive.item_id)) {
      state.motive = { item_id: piece.motive.item_id, version: Number(piece.motive.version) };
    }
    if (POOLS.some((entry) => entry.value === piece.pool)) {
      state.pool = piece.pool;
    }
    if (piece.accompaniment && typeof piece.accompaniment === "object" && typeof piece.accompaniment.pattern === "string") {
      state.accompaniment = { kind: "pattern", pattern: piece.accompaniment.pattern };
    }
    state.picks = Themes.picksFromBrief(piece.brief);
    state.switches = Themes.switchesFromWithout(piece.without);
    if (WEIGHTS.some((entry) => entry.value === piece.style_weight)) {
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

  /* The cadence a section gets when none is chosen (R-169): HC at the end of the intro and of the loop, PAC at the
   * end of the ending, IAC elsewhere. Returns the type for the section at index. */
  function defaultCadence(state, index) {
    const sections = Array.isArray(state && state.sections) ? state.sections : [];
    const section = sections[index];
    if (!section) {
      return "IAC";
    }
    const lastOf = (test) => {
      let last = -1;
      sections.forEach((other, i) => {
        if (test(other)) {
          last = i;
        }
      });
      return last;
    };
    if (section.part === "ending") {
      return index === lastOf((other) => other.part === "ending") ? "PAC" : "IAC";
    }
    if (section.part === "intro") {
      return index === lastOf((other) => other.part === "intro") ? "HC" : "IAC";
    }
    return index === lastOf((other) => other.part === "body" || other.part === "turnaround") ? "HC" : "IAC";
  }

  /* The cadence to show: the chosen one, else the default. */
  function cadenceOf(state, index) {
    const section = state.sections[index];
    return section && section.cadence ? section.cadence : defaultCadence(state, index);
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

  /* A motif fill takes its theme form (sentence, period, hybrid) at 4 or 8 whole bars without a pickup; any other length
   * is a free phrase. */
  function motifIsFree(section, meter) {
    const per = barBeats(meter);
    const bars = Number(section.beats) / per;
    const pickup = rational(section.pickup);
    return !(THEME_BARS.includes(bars) && (pickup === null || pickup === 0));
  }

  /* What stops Compose or Save, as a list of sentences. options.themeBeats(fill) gives the length in beats of the
   * theme a theme fill names, or null when it is not known. */
  function structureProblems(state, options) {
    const s = state || emptyState();
    const o = options || {};
    const problems = [];
    const sections = Array.isArray(s.sections) ? s.sections : [];
    if (sections.length > MAX_SECTIONS) {
      problems.push(`A piece has at most ${MAX_SECTIONS} sections.`);
    }
    if (!sections.some((section) => section.part === "body")) {
      problems.push("The piece needs a section in the loop body.");
    }
    let last = -1;
    let ordered = true;
    for (const section of sections) {
      const at = partIndex(section.part);
      if (at < last) {
        ordered = false;
      }
      last = Math.max(last, at);
    }
    if (!ordered) {
      problems.push("The parts must come in the order intro, loop body, turnaround, ending.");
    }
    const limit = barBeats(s.meter);
    for (const section of sections) {
      const name = section.letter ? `Section ${section.letter}` : `Section ${section.id}`;
      const beats = Number(section.beats);
      if (!Number.isInteger(beats) || beats < 1) {
        problems.push(`${name} must be at least a beat long.`);
      } else if (beats > MAX_BEATS) {
        problems.push(`${name} is at most ${MAX_BEATS} beats.`);
      }
      const pickup = rational(section.pickup);
      if (pickup === null || pickup >= limit) {
        problems.push(`${name} has a pickup shorter than a bar, such as 1 or 1/2 beat.`);
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

  /* What stops Compose beyond the structure: the key, the meter, the tempo, the seed, the mode and the numerals. */
  function formProblems(state, options) {
    const s = state || emptyState();
    const problems = structureProblems(s, options);
    if (!/^[A-G][#b]?( (major|minor))?$/.test(text(s.key)) || text(s.key).length > 16) {
      problems.push("The key must be a tonic such as D or Bb.");
    }
    if (!METERS.includes(s.meter)) {
      problems.push("The meter must be 2/4, 3/4, 4/4 or 6/8.");
    }
    if (text(s.tempo) !== "" && wholeNumber(s.tempo, 20, 400) === null) {
      problems.push("The tempo is 20 to 400 quarter notes per minute, or blank.");
    }
    if (text(s.seed) !== "" && wholeNumber(s.seed, 0, MAX_SEED) === null) {
      problems.push("The seed is a whole number, or blank.");
    }
    if (s.mode === "style" && Themes.briefFromPicks(s.picks) === null) {
      problems.push("The mode from the brief needs a brief.");
    }
    for (const section of Array.isArray(s.sections) ? s.sections : []) {
      const fill = section.fill || {};
      const name = section.letter ? `Section ${section.letter}` : `Section ${section.id}`;
      if (fill.kind === "progression") {
        if (text(fill.numerals) !== "" && !numeralsOk(fill.numerals)) {
          problems.push(`${name} needs numerals such as I vi IV V.`);
        }
        if (text(fill.cells) !== "" && wholeNumber(fill.cells, 1, MAX_CELLS) === null) {
          problems.push(`${name} has 1 to ${MAX_CELLS} cells per chord.`);
        }
      }
    }
    return problems;
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
      o.seed !== undefined && o.seed !== null && o.seed !== "" ? wholeNumber(o.seed, 0, MAX_SEED) : wholeNumber(state.seed, 0, MAX_SEED);
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

  /* The lanes of a section that can be written again (R-173): a section filled with a theme takes harmony and bass
   * only, an ending section none (the backend refuses both others). */
  function lanesFor(section) {
    if (!section || section.part === "ending") {
      return [];
    }
    return section.fill && section.fill.kind === "theme" ? ["harmony", "bass"] : LANES.slice();
  }

  /* The seed typed, blank for none: {value} (undefined for none) or {error}. */
  function optionalSeed(value) {
    if (text(String(value === undefined || value === null ? "" : value)) === "") {
      return { value: undefined };
    }
    const seed = wholeNumber(value, 0, MAX_SEED);
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
    const rows = (Array.isArray(v.sections) ? v.sections : []).map((section) => {
      const beats = beatsOfTime(section.start, meter);
      return {
        id: section.id,
        part: section.part,
        letter: section.letter || "",
        start: positionLabel(beats, meter),
        startBeats: beats,
        beats: section.beats,
        length: beatsLabel(section.beats, meter),
        cadence: section.cadence || "",
        heard: section.heard || "",
        fill: FILL_TEXT[section.fill] || section.fill || "",
        source: sourceText(section.source, options && options.sourceNames),
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

  /* What loop playback needs, in whole notes: the loop region and where the last body section ends (the ending takes
   * over there), or null when the summary has no loop or no body section. */
  function playbackLoop(summary) {
    const s = summary && typeof summary === "object" ? summary : {};
    const loop = s.loop && typeof s.loop === "object" ? s.loop : null;
    const start = loop ? wholeNotes(loop.start) : null;
    const end = loop ? wholeNotes(loop.end) : null;
    const version = s.versions && s.versions.loop && Array.isArray(s.versions.loop.sections) ? s.versions.loop : null;
    if (start === null || end === null || !(end > start) || !version) {
      return null;
    }
    const unit = beatUnit(s.meter);
    let bodyEnd = null;
    for (const section of version.sections) {
      const at = wholeNotes(section.start);
      if (section.part === "body" && at !== null && Number.isFinite(section.beats)) {
        bodyEnd = Math.max(bodyEnd === null ? 0 : bodyEnd, at + section.beats / unit);
      }
    }
    return bodyEnd === null ? null : { start, end, bodyEnd };
  }

  /* The Composer's summary of a piece version's result as rows to show. result is piece.result ({seed, summary, brief,
   * loop, files}); options carries sourceNames ({<section id>: "Theme One v2"}) for the sections filled with a theme. */
  function resultView(result, options) {
    const r = result && typeof result === "object" ? result : {};
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
      playLoop: playbackLoop(summary),
      loopLength: startBeats !== null && endBeats !== null ? beatsLabel(endBeats - startBeats, meter) : "",
      versions: { loop: versionView(versions.loop, meter, options), ending: versionView(versions.ending, meter, options) },
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
    PARTS,
    PART_NAMES,
    CADENCES,
    FILL_KINDS,
    INTOS,
    MODES,
    METERS,
    POOLS,
    PATTERNS,
    WEIGHTS,
    PICKUPS,
    MAX_SECTIONS,
    MAX_CELLS,
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
    defaultCadence,
    cadenceOf,
    motifIsFree,
    themeLengthBeats,
    themeFormText,
    themeComposed,
    fillKey,
    numeralsOk,
    structureProblems,
    formProblems,
    saveRequest,
    composeRequest,
    versionFileUrl,
    versionPlaybackUrl,
    LANES,
    MAX_VARIATIONS,
    lanesFor,
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
