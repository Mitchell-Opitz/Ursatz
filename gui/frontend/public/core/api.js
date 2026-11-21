import ApiRequest from "./request.js";
import BackendJobs from "./backend-jobs.js";
import { commandRequest, sendCommand } from "./commands.js";

/* The page's JSON calls (R-647): every route answers one envelope; ApiRequest sends it, BackendJobs follows a request the backend runs as a job. */
/* Sends a request and resolves to {ok, status, data} (ApiRequest.sendRequest: the one fetch of JSON; every route answers one envelope, R-647). options
 * are fetch's: method, headers and a body that is text. Shared by the views that talk to the routes. */
export function fetchJsonResult(url, options) {
  const o = options || {};
  return ApiRequest.sendRequest({ url, method: o.method || "GET", headers: o.headers, body: o.body });
}

/* Follows a request the backend runs as a job (R-645: piece imports and re-analyses, style builds and rebuilds): runJob sends it, polls the job and
 * answers {ok, status, data} with the body the job ended with. */
async function runJobResult(request) {
  const res = await BackendJobs.runJob(request);
  const data = res.body || null;
  return { ok: res.status >= 200 && res.status < 300 && !(data && data.ok === false), status: res.status, data };
}

/* A change (R-668): the command `name` ({target, params, file, filename}) sent to POST /api/workshop/command; resolves to {ok, status, data}. */
export function commandResult(name, options) {
  return sendCommand(name, options);
}

/* A query (R-671): the query `name` posted with its params as JSON to POST /api/workshop/query/<name>; resolves to {ok, status, data} as fetchJsonResult
 * does (data is the answer with its ok member). The views that read workshop state through the generic route use it. */
export function queryResult(name, params) {
  return fetchJsonResult(`/api/workshop/query/${encodeURIComponent(name)}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(params === undefined || params === null ? {} : params),
  });
}

/* commandResult for a command that runs as a job (a compose, an import, a style build): followed to its end. */
export function commandJobResult(name, options) {
  return runJobResult(commandRequest(name, options || {}));
}
