/* Vocabulary (P27 B10): the words the Composer reads (meters, modes, keys, cadences, pickups, ideas, patterns,
 * limits...) come from GET /api/composer/vocabulary (composer_api_vocabulary), loaded once before the views mount.
 * The models read their lists from here and keep none of their own (R-270, R-276). No DOM. */
(function (root) {
  let current = null;

  /* Fetches the vocabulary once and keeps it. fetchFn defaults to fetch. Rejects when the backend cannot give it. */
  async function load(fetchFn) {
    const doFetch = fetchFn || ((url) => root.fetch(url));
    const res = await doFetch("/api/composer/vocabulary");
    if (res.status !== 200) {
      throw new Error(`The Composer vocabulary is unavailable (${res.status}).`);
    }
    current = await res.json();
    return current;
  }

  /* Sets the vocabulary directly (tests, a captured fixture). */
  function set(vocabulary) {
    current = vocabulary;
  }

  function loaded() {
    return current !== null;
  }

  function get() {
    if (!current) {
      throw new Error("The Composer vocabulary is not loaded.");
    }
    return current;
  }

  /* The list called name, as the Composer gives it ([{value, label, ...}]); [] for an unknown name. */
  function list(name) {
    const entries = get()[name];
    return Array.isArray(entries) ? entries : [];
  }

  /* The values of a list, as strings. */
  function values(name) {
    return list(name).map((entry) => String(entry.value));
  }

  /* The label of value in a list, or fallback (the value) when the list has none. */
  function labelOf(name, value, fallback) {
    const entry = list(name).find((item) => String(item.value) === String(value));
    return entry && entry.label !== undefined ? entry.label : fallback === undefined ? String(value) : fallback;
  }

  /* The entry of value in a list, or null. */
  function entry(name, value) {
    return list(name).find((item) => String(item.value) === String(value)) || null;
  }

  /* A number of the limits member (motive_notes, tempo_min...), or fallback when the Composer does not give it. */
  function limit(name, fallback) {
    const value = (get().limits || {})[name];
    return typeof value === "number" ? value : fallback;
  }

  /* The largest seed a request reads: kind is "variants", "random_motive", "theme" or "piece". */
  function seedMax(kind) {
    const table = (get().limits || {}).seed_max || {};
    return typeof table[kind] === "number" ? table[kind] : undefined;
  }

  const api = { load, set, loaded, get, list, values, labelOf, entry, limit, seedMax };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.Vocabulary = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
