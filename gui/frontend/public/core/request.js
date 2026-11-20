/* Request helpers shared by the models and views: the fetch request object of a JSON call, the error text of a failed
 * answer, the form field a refused request member belongs to, and the validate request. No DOM. */

/* The request object the page sends: method and url, plus a JSON body when one is given. */
function jsonRequest(method, url, body) {
  const request = { method, url };
  if (body !== undefined) {
    request.headers = { "Content-Type": "application/json" };
    request.body = JSON.stringify(body);
  }
  return request;
}

/* The error text of a failed answer, or the fallback. */
function errorText(data, fallback) {
  if (data && typeof data.error === "string" && data.error !== "") {
    return data.error;
  }
  return fallback || "Request failed";
}

/* The form field for a request member path (a JSON Pointer the Composer refused): the second entry of the first
 * [prefix, field] row of table whose prefix is the path or its parent; otherwise fallback (null when omitted). */
function fieldForPath(path, table, fallback) {
  const value = typeof path === "string" ? path : "";
  const hit = table.find(([prefix]) => value === prefix || value.startsWith(`${prefix}/`));
  return hit ? hit[1] : fallback === undefined ? null : fallback;
}

/* The request that asks the backend to check a motif, theme or piece body with the Composer
 * (POST /api/materials/items/validate). */
function validateRequest(type, body) {
  return jsonRequest("POST", "/api/materials/items/validate", { type, body });
}

/*
 * The versions the page has read (R-646). Every answer that holds an item (or a list of them) records its current version; a write to an item
 * that has versions carries the one the page holds as base_version (in the JSON body, or in the query of a DELETE), so the workshop refuses it
 * with the item's current version when another change got in since the page read it (409, "changed since you read it").
 */
const knownVersions = new Map();

function rememberVersions(data) {
  const items = [];
  if (data && typeof data === "object") {
    if (data.item && typeof data.item === "object") {
      items.push(data.item);
    }
    if (Array.isArray(data.items)) {
      items.push(...data.items);
    }
    if (typeof data.current_version === "number" && typeof data.id === "string") {
      items.push(data);
    }
  }
  for (const item of items) {
    if (item && typeof item.id === "string" && Number.isInteger(item.current_version)) {
      knownVersions.set(item.id, item.current_version);
    }
  }
}

function knownVersion(id) {
  return knownVersions.has(id) ? knownVersions.get(id) : null;
}

function forgetVersions() {
  knownVersions.clear();
}

/* The item a write changes the versions of, or null: PATCH and DELETE of an item, and the POSTs that save, compose, regenerate or choose a
 * version, reanalyze, reroll, flag, promote, link a form or update a use. */
function writtenItemId(method, url) {
  const match = /^\/api\/materials\/items\/([^/?]+)([^?]*)/.exec(String(url || ""));
  if (!match || !match[1].includes("-")) {
    return null;
  }
  const tail = match[2];
  const verb = String(method || "GET").toUpperCase();
  if (verb === "PATCH" || verb === "DELETE") {
    return tail === "" ? decodeURIComponent(match[1]) : null;
  }
  if (verb !== "POST") {
    return null;
  }
  const whole = ["/versions", "/compose", "/reanalyze", "/reroll", "/flag", "/promote", "/link-form"];
  if (
    whole.includes(tail) ||
    /^\/uses\/[^/]+\/update$/.test(tail) ||
    /^\/versions\/\d+\/regenerate$/.test(tail) ||
    /^\/versions\/\d+\/variations\/\d+\/choose$/.test(tail)
  ) {
    return decodeURIComponent(match[1]);
  }
  return null;
}

/* The request with the version the page holds for the item it writes; a request for any other item, a request that gives base_version already
 * and one for an item the page has not read go as they are. */
function withBaseVersion(request) {
  const id = writtenItemId(request.method, request.url);
  const version = id === null ? null : knownVersion(id);
  if (version === null || String(request.url).includes("base_version=")) {
    return request;
  }
  if (String(request.method).toUpperCase() === "DELETE") {
    const joiner = String(request.url).includes("?") ? "&" : "?";
    return { ...request, url: `${request.url}${joiner}base_version=${version}` };
  }
  let body = {};
  if (typeof request.body === "string" && request.body !== "") {
    try {
      body = JSON.parse(request.body);
    } catch (error) {
      return request;
    }
  }
  if (!body || typeof body !== "object" || Array.isArray(body) || "base_version" in body) {
    return request;
  }
  return { ...request, headers: { "Content-Type": "application/json" }, body: JSON.stringify({ ...body, base_version: version }) };
}

/*
 * The one fetch of JSON (R-647). Every route answers {"ok": true, ...} or {"ok": false, "error", "code", "path"?}: this resolves to {ok, status, data},
 * data null for an answer that is not JSON, ok false for an error status or {"ok": false}. A network failure is {ok: false, status: 0, data: {ok: false,
 * error, code: "unreachable"}}. request is {url, method, headers, body (text)}; fetchFn is the page's fetch.
 */
async function sendRequest(request, fetchFn) {
  const doFetch = fetchFn || ((url, init) => globalThis.fetch(url, init));
  try {
    const res = await doFetch(request.url, { method: request.method, headers: request.headers, body: request.body });
    let data = null;
    try {
      data = await res.json();
    } catch (error) {
      data = null;
    }
    return { ok: res.ok && !(data && data.ok === false), status: res.status, data };
  } catch (error) {
    return { ok: false, status: 0, data: { ok: false, error: `Could not reach backend: ${error}`, code: "unreachable" } };
  }
}

/* sendRequest for a method, a url and a body that is sent as JSON when given (undefined for none). */
function apiRequest(method, url, body, fetchFn) {
  return sendRequest(jsonRequest(method, url, body), fetchFn);
}

const api = { sendRequest, apiRequest, jsonRequest, errorText, fieldForPath, validateRequest, rememberVersions, knownVersion, forgetVersions, withBaseVersion };
export default api;
