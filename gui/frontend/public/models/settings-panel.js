/* The settings panel's model (R-671): how the answer of settings.resolve is worded for the theme page. Pure functions, no DOM access. The words are
 * the page's own labels of the sources the workshop names (workshop/docs/settings.md): no setting's name or value is known here, only how a source,
 * a value and an override are read aloud. */

/* The label of a source: "default", "style:<id>", "taste:all", "taste:<name>", "project:<id>", "item:<id>", "request" or "explore". */
function sourceLabel(from) {
  const text = typeof from === "string" ? from : "";
  if (text === "default") {
    return "default";
  }
  if (text === "taste:all") {
    return "taste profile all";
  }
  if (text.startsWith("taste:")) {
    return `taste profile ${text.slice("taste:".length)}`;
  }
  if (text.startsWith("style:")) {
    return "style";
  }
  if (text.startsWith("project:")) {
    return "project";
  }
  if (text.startsWith("item:")) {
    return "this theme";
  }
  if (text === "request") {
    return "this request";
  }
  if (text === "explore") {
    return "explore";
  }
  return text;
}

/* A value as text: a list joined by ", ", true and false as on and off, any other as given. */
function valueText(value) {
  if (Array.isArray(value)) {
    return value.map((item) => valueText(item)).join(", ");
  }
  if (value === true) {
    return "on";
  }
  if (value === false) {
    return "off";
  }
  if (value === null || value === undefined) {
    return "";
  }
  return String(value);
}

/* Rows {key, value, from} for the settings of a resolution that are not at their default (resolution.values). */
function settingsRows(resolution) {
  const values = resolution && Array.isArray(resolution.values) ? resolution.values : [];
  return values.map((row) => ({ key: row.key, value: valueText(row.value), from: sourceLabel(row.from) }));
}

/* Rows {text, from} for the overrides: "<rule> <action>" and, for a reweight, " <weight>". */
function overrideRows(resolution) {
  const overrides = resolution && Array.isArray(resolution.overrides) ? resolution.overrides : [];
  return overrides.map((entry) => ({
    text: `${entry.rule} ${entry.action}${typeof entry.weight === "number" ? ` ${entry.weight}` : ""}`,
    from: sourceLabel(entry.from),
  }));
}

/* Whether the resolution skipped the taste profiles (its off list names "taste"). */
function tasteIsOff(resolution) {
  return !!(resolution && Array.isArray(resolution.off) && resolution.off.includes("taste"));
}

/* "Taste profile: <name>", or "Taste profile: off for this theme" when the taste is switched off. */
function tasteLine(resolution) {
  if (tasteIsOff(resolution)) {
    return "Taste profile: off for this theme";
  }
  const name = resolution && typeof resolution.taste === "string" ? resolution.taste : "none";
  return `Taste profile: ${name}`;
}

/* The off list of an item scope with "taste" added or removed, its other words kept (in the workshop's order). */
function withTaste(off, on) {
  const kept = Array.isArray(off) ? off.filter((word) => typeof word === "string" && word !== "taste") : [];
  return on ? ["taste", ...kept] : kept;
}

export default { sourceLabel, valueText, settingsRows, overrideRows, tasteIsOff, tasteLine, withTaste };
