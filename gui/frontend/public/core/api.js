import ApiRequest from "./request.js";
import BackendJobs from "./backend-jobs.js";

/* The page's JSON calls (R-647): every route answers one envelope; ApiRequest sends it, BackendJobs follows a request the backend runs as a job. */
/* Sends a request and resolves to {ok, status, data} (ApiRequest.sendRequest: the one fetch of JSON; every route answers one envelope, R-647). options
 * are fetch's: method, headers and a body that is text. Shared by the views that talk to the routes. */
export function fetchJsonResult(url, options) {
  const o = options || {};
  return ApiRequest.sendRequest({ url, method: o.method || "GET", headers: o.headers, body: o.body });
}

/* fetchJsonResult for a request the backend runs as a job (R-645: piece imports and re-analyses, style builds and rebuilds): runJob sends it,
 * polls the job and answers the status and body the route answered before jobs, so the callers keep their reply handling. */
export async function jobJsonResult(url, options) {
  const res = await BackendJobs.runJob({ url, method: options.method, headers: options.headers, body: options.body });
  const data = res.body || null;
  return { ok: res.status >= 200 && res.status < 300 && !(data && data.ok === false), status: res.status, data };
}

export function postJsonResult(url, body) {
  return ApiRequest.apiRequest("POST", url, body);
}
