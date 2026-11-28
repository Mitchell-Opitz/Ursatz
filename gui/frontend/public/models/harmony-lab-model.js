import Vocabulary from "../core/vocabulary.js";
import { commandRequest } from "../core/commands.js";
import { text } from "../core/values.js";

/* The state of the Harmony Lab page (R-769) and the requests it sends: the progression request of R-766 built from the page's fields, the palette
 * query's params, the commands (item.create, item.save, item.compose, knowledge.keep-schema, item.apply-harmony), the refusal paths mapped to the
 * fields they are about. Every list (materials, meters, patterns, voicings) comes from the Composer's vocabulary or from the palette and list_schemas
 * answers; this module holds no music tables (R-218, R-270). No DOM. */

const SHAPES = ["rise", "fall", "hold", "peak", "step"];
const POOLS = ["any", "first", "second", "shared", "color"];
const SEED_MAX = 4294967295;

function emptyArc() {
  return { shape: "", at: 1 };
}

/* A new page: the key the Composer's defaults name (a minor key on C), one chord a bar, 8 bars, a cadence of none. */
function initialState() {
  const defaults = Vocabulary.defaults();
  const first = Vocabulary.list("materials")[0];
  return {
    tonic: 0,
    material: first ? String(first.value) : "",
    withMaterial: "",
    meter: defaults.meter,
    bars: 8,
    chordsPerBar: 1,
    units: new Array(8).fill(""),
    spans: [],
    brightness: emptyArc(),
    tension: emptyArc(),
    cadence: "none",
    seed: "",
    accompaniment: { pattern: "", voicing: "" },
    name: "",
    item: null,
  };
}

/* The name of a pitch class for a material ("Eb" for 3 in minor), from the vocabulary's pitch_class_names. */
function tonicName(pitchClass, material) {
  const entry = Vocabulary.list("pitch_class_names").find((row) => Number(row.value) === Number(pitchClass));
  return entry && entry.names ? entry.names[material] || Object.values(entry.names)[0] || "" : "";
}

/* The tonic choices for a material: [{value: pitch class, label: name}] in the vocabulary's order. */
function tonicChoices(material) {
  return Vocabulary.list("pitch_class_names").map((row) => ({ value: row.value, label: (row.names && (row.names[material] || Object.values(row.names)[0])) || String(row.value) }));
}

/* The key as the request writes it: "C minor". */
function keyText(state) {
  return `${tonicName(state.tonic, state.material)} ${Vocabulary.keyWord(state.material)}`;
}

/* Beats a unit is long: the meter's beats over the chords a bar. */
function beatsPerUnit(state) {
  return Vocabulary.meterBeats(state.meter) / state.chordsPerBar;
}

/* The chords a bar may have for a meter, from the vocabulary's meter entry: 1, its two halves where they are whole beats, its beats up to chords_max. */
function chordsPerBarChoices(meter) {
  const entry = Vocabulary.entry("meters", meter) || Vocabulary.entry("meters", Vocabulary.defaults().meter);
  const beats = entry.beats;
  const max = Number.isInteger(entry.chords_max) ? entry.chords_max : 1;
  const choices = [1];
  if (beats % 2 === 0 || entry.beat_unit === 8) {
    choices.push(2);
  }
  if (beats > 2 && beats <= max && entry.beat_unit !== 8) {
    choices.push(beats);
  }
  return [...new Set(choices)].sort((a, b) => a - b);
}

/* The state with its units resized to bars times chords a bar, what is typed kept where its unit stays. */
function resize(state, changes) {
  const next = { ...state, ...changes };
  const count = next.bars * next.chordsPerBar;
  const units = next.units.slice(0, count);
  while (units.length < count) {
    units.push("");
  }
  next.units = units;
  if (!chordsPerBarChoices(next.meter).includes(next.chordsPerBar)) {
    next.chordsPerBar = 1;
    next.units = next.units.slice(0, next.bars);
    while (next.units.length < next.bars) {
      next.units.push("");
    }
  }
  return next;
}

/* The chord a chip puts on a unit: the chip's text (the Composer's own text for the chord). */
function applyChip(state, unit, chip) {
  if (!chip || !text(chip.text) || unit < 0 || unit >= state.units.length) {
    return state;
  }
  const units = state.units.slice();
  units[unit] = chip.text;
  return { ...state, units };
}

/* The request's `chords`: one {bar, text} for each bar with a typed unit, its units joined ("I V@2"); the bars (from 1) in the order sent, which
 * the paths of a refusal index. */
function chordEntries(state) {
  const per = state.chordsPerBar;
  const beats = beatsPerUnit(state);
  const entries = [];
  for (let bar = 0; bar < state.bars; bar++) {
    const parts = [];
    for (let u = 0; u < per; u++) {
      const chord = text(state.units[bar * per + u]);
      if (chord) {
        parts.push(u === 0 ? chord : `${chord}@${u * beats}`);
      }
    }
    if (parts.length > 0) {
      entries.push({ bar: bar + 1, text: parts.join(" ") });
    }
  }
  return entries;
}

function arcMember(arc) {
  if (!SHAPES.includes(arc.shape)) {
    return null;
  }
  return arc.shape === "peak" || arc.shape === "step" ? { shape: arc.shape, at: arc.at } : { shape: arc.shape };
}

/* The spans of the request: a row is {from, to, chords, schema, loop} (loop null: the schema's own). Rows with no bars are left out. */
function spanMembers(state) {
  const spans = [];
  for (const row of state.spans) {
    if (!(row.from >= 1 && row.to >= row.from)) {
      continue;
    }
    const span = { bars: [row.from, row.to] };
    if (POOLS.includes(row.chords)) {
      span.chords = row.chords;
    }
    if (text(row.schema)) {
      span.schema = row.schema;
    }
    if (row.loop === true || row.loop === false) {
      span.loop = row.loop;
    }
    spans.push(span);
  }
  return spans;
}

/* The progression request of R-766 for the page: key, meter, bars, cadence, the palette (chords only for the units typed), the accompaniment. */
function requestFromState(state, options) {
  const o = options || {};
  const palette = {};
  if (text(state.withMaterial)) {
    palette.with = Vocabulary.keyWord(state.withMaterial);
  }
  if (state.chordsPerBar !== 1) {
    palette.chords_per_bar = state.chordsPerBar;
  }
  const spans = spanMembers(state);
  if (spans.length > 0) {
    palette.spans = spans;
  }
  const chords = chordEntries(state);
  if (chords.length > 0) {
    palette.chords = chords;
  }
  const brightness = arcMember(state.brightness);
  const tension = arcMember(state.tension);
  if (brightness) {
    palette.brightness = brightness;
  }
  if (tension) {
    palette.tension = tension;
  }
  const request = { key: keyText(state), meter: state.meter, bars: state.bars, cadence: state.cadence, palette };
  const seed = Number(text(String(state.seed)));
  if (text(String(state.seed)) !== "" && Number.isInteger(seed) && seed >= 0 && seed <= SEED_MAX) {
    request.seed = String(seed);
  }
  if (text(state.accompaniment.pattern)) {
    request.accompaniment = { pattern: state.accompaniment.pattern };
    if (text(state.accompaniment.voicing)) {
      request.accompaniment.voicing = state.accompaniment.voicing;
    }
  }
  if (o.suggest) {
    request.suggest = o.suggest;
  }
  return request;
}

/* The params of the palette query for the page's key and second mode. */
function paletteParams(state) {
  const params = { key: keyText(state) };
  if (text(state.withMaterial)) {
    params.with = Vocabulary.keyWord(state.withMaterial);
  }
  return params;
}

/* The page's state for a stored request (the inverse of requestFromState); fields the request does not name keep the new page's. */
function stateFromRequest(request, item) {
  const base = initialState();
  const req = request && typeof request === "object" ? request : {};
  const [tonicWord, ...rest] = text(req.key).split(/\s+/);
  const word = rest.join(" ");
  const material = Vocabulary.list("materials").find((entry) => String(entry.label) === word);
  const state = { ...base, item: item || null };
  if (material) {
    state.material = String(material.value);
    const tonic = Vocabulary.list("pitch_class_names").find((row) => row.names && Object.values(row.names).includes(tonicWord) && row.names[material.value] === tonicWord);
    state.tonic = tonic ? Number(tonic.value) : 0;
  }
  state.meter = text(req.meter) || base.meter;
  state.bars = Number.isInteger(req.bars) ? req.bars : base.bars;
  state.cadence = text(req.cadence) || "none";
  state.seed = req.seed === undefined ? "" : String(req.seed);
  const palette = req.palette && typeof req.palette === "object" ? req.palette : {};
  const withMaterial = Vocabulary.list("materials").find((entry) => String(entry.label) === text(palette.with));
  state.withMaterial = withMaterial ? String(withMaterial.value) : "";
  state.chordsPerBar = Number.isInteger(palette.chords_per_bar) ? palette.chords_per_bar : 1;
  state.units = new Array(state.bars * state.chordsPerBar).fill("");
  const perUnit = Vocabulary.meterBeats(state.meter) / state.chordsPerBar;
  for (const entry of Array.isArray(palette.chords) ? palette.chords : []) {
    const parts = text(entry.text).split(/\s+/).filter(Boolean);
    let unit = 0;
    for (const part of parts) {
      const at = part.split("@");
      unit = at.length > 1 ? Math.round(Number(at[1]) / perUnit) : unit;
      state.units[(entry.bar - 1) * state.chordsPerBar + unit] = at[0];
      unit++;
    }
  }
  state.spans = (Array.isArray(palette.spans) ? palette.spans : []).map((span) => ({
    from: span.bars[0],
    to: span.bars[1],
    chords: span.chords || "any",
    schema: span.schema || "",
    loop: typeof span.loop === "boolean" ? span.loop : null,
  }));
  state.brightness = palette.brightness ? { shape: palette.brightness.shape, at: palette.brightness.at || 1 } : emptyArc();
  state.tension = palette.tension ? { shape: palette.tension.shape, at: palette.tension.at || 1 } : emptyArc();
  const accompaniment = req.accompaniment && typeof req.accompaniment === "object" ? req.accompaniment : {};
  state.accompaniment = { pattern: text(accompaniment.pattern), voicing: text(accompaniment.voicing) };
  return state;
}

/* The body of a harmony item: its uses (none) and the request. */
function itemBody(state) {
  return { uses: [], request: requestFromState(state) };
}

/* The commands the page sends, as the page's request objects (core/commands.js). */
function createCommand(state, projectId) {
  return commandRequest("item.create", { params: { type: "harmony", name: text(state.name) || "Harmony plan", project_id: projectId, body: itemBody(state) } });
}

function saveCommand(state, note) {
  return commandRequest("item.save", { target: state.item.id, params: { body: itemBody(state), note: note || "Edited in the Harmony Lab" } });
}

function composeCommand(itemId, seed) {
  const params = {};
  const value = text(String(seed === undefined || seed === null ? "" : seed));
  if (value !== "" && Number.isInteger(Number(value)) && Number(value) >= 0 && Number(value) <= SEED_MAX) {
    params.seed = Number(value);
  }
  return Object.assign(commandRequest("item.compose", { target: itemId, params }), { job: true });
}

function keepSchemaCommand(itemId, version, name, slug, loop) {
  return commandRequest("knowledge.keep-schema", { params: { item: itemId, version, name: text(name), slug: text(slug), loop: loop !== false } });
}

function applyCommand(target, harmonyId, version, section) {
  const params = { harmony: harmonyId, version };
  if (text(section)) {
    params.section = section;
  }
  return commandRequest("item.apply-harmony", { target, params });
}

/* The slug a name suggests: lower-case words and digits joined by hyphens. */
function slugOf(name) {
  return text(name).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "").slice(0, 60);
}

/* The page field a refusal's request path is about: {field: "unit", bar} (a chord of the palette's chords: the index counts the typed bars), {field: "span",
 * index}, or {field: <name>}; null for a path the page has no field for. */
function fieldForPath(path, state) {
  const value = typeof path === "string" ? path : "";
  let hit = /^\/palette\/chords\/(\d+)/.exec(value);
  if (hit) {
    const entry = chordEntries(state)[Number(hit[1])];
    return entry ? { field: "unit", bar: entry.bar } : { field: "units" };
  }
  hit = /^\/palette\/spans\/(\d+)/.exec(value);
  if (hit) {
    return { field: "span", index: Number(hit[1]) };
  }
  const names = [
    ["/palette/with", "with"],
    ["/palette/chords_per_bar", "chordsPerBar"],
    ["/palette/brightness", "brightness"],
    ["/palette/tension", "tension"],
    ["/accompaniment", "accompaniment"],
    ["/cadence", "cadence"],
    ["/meter", "meter"],
    ["/bars", "bars"],
    ["/key", "key"],
    ["/seed", "seed"],
  ];
  const found = names.find(([prefix]) => value === prefix || value.startsWith(`${prefix}/`));
  return found ? { field: found[1] } : null;
}

/* The line a refused answer shows: its message, and where it points when the page has a field for it. */
function errorText(data, fallback) {
  const message = data && typeof data.error === "string" ? data.error : fallback;
  return message;
}

/* The chips of a palette answer: every chord of the key's material, then the second's, as {text, numeral, name, kind, shared, color, material}; kind is
 * "shared", "color" or "own". A chord the Composer has no text for is left out (it cannot be typed). */
function chipsFrom(palette) {
  const chips = [];
  for (const material of (palette && palette.materials) || []) {
    for (const chord of material.chords || []) {
      if (!text(chord.text)) {
        continue;
      }
      chips.push({
        text: chord.text,
        numeral: chord.numeral,
        name: chord.name,
        shared: Boolean(chord.shared),
        color: Boolean(chord.color),
        kind: chord.shared ? "shared" : chord.color ? "color" : "own",
        material: material.material,
        word: material.word,
      });
    }
  }
  return chips;
}

/* The numbers under the timeline: [{bar, tension, brightness}] with "-" for a bar that has none. */
function measureRows(measures) {
  return (Array.isArray(measures) ? measures : []).map((row) => ({ bar: row.bar, tension: row.tension || "-", brightness: row.brightness || "-" }));
}

const HarmonyLabModel = {
  SHAPES,
  POOLS,
  initialState,
  tonicName,
  tonicChoices,
  keyText,
  chordsPerBarChoices,
  resize,
  applyChip,
  chordEntries,
  requestFromState,
  paletteParams,
  stateFromRequest,
  itemBody,
  createCommand,
  saveCommand,
  composeCommand,
  keepSchemaCommand,
  applyCommand,
  slugOf,
  fieldForPath,
  errorText,
  chipsFrom,
  measureRows,
};

export default HarmonyLabModel;
