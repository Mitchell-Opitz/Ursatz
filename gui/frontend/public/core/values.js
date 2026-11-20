/* The small value checks and readers the models share: a plain object, a claim (an item with a span or a location), a trimmed string, a whole number
 * inside limits and the url of a materials item. No DOM. */

export function isPlainObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

/* An analysis item that points at a span or a location of the piece. */
export function isClaim(item) {
  return isPlainObject(item) && ("span" in item || "location" in item);
}

export function isClaimArray(value) {
  return Array.isArray(value) && value.length > 0 && value.every(isClaim);
}

/* The string trimmed, or "" for a value that is not a string. */
export function text(value) {
  return typeof value === "string" ? value.trim() : "";
}

/* The whole number typed in value when it is between min and max, else null. */
export function wholeNumber(value, min, max) {
  const t = text(String(value === undefined || value === null ? "" : value));
  if (!/^\d+$/.test(t)) {
    return null;
  }
  const n = Number(t);
  return n >= min && n <= max ? n : null;
}

/* The url of a materials item and what follows it (tail starts with a slash or is empty). */
export function itemUrl(id, tail) {
  return `/api/materials/items/${encodeURIComponent(id)}${tail}`;
}
