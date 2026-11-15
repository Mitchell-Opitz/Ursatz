/* Request helpers shared by the models and views: the fetch request object of a JSON call, the error text of a failed
 * answer, the form field a refused request member belongs to, and the validate request. No DOM. */
(function (root) {
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

  const api = { jsonRequest, errorText, fieldForPath, validateRequest };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ApiRequest = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
