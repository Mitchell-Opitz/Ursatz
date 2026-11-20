/* Covers the base versions the page holds (public/core/request.js, R-646): an answer that holds an item records its current version; core/commands.js
 * sends it as the base_version of a command. The one fetch of JSON (R-647). Plain Node script, run through ctest. */
import Req from "../public/core/request.js";
import { check, same, finish } from "./support/check.js";

const eq = (actual, expected, what) => check(same(actual, expected), what);

Req.forgetVersions();
eq(Req.knownVersion("mot-1"), null, "no version is known before an answer");
Req.rememberVersions({ ok: true, item: { id: "mot-1", current_version: 3 }, body: {} });
Req.rememberVersions({ ok: true, items: [{ id: "thm-1", current_version: 2 }, { id: "pce-1", current_version: 5 }] });
eq(Req.knownVersion("mot-1"), 3, "an item answer records the version");
eq(Req.knownVersion("pce-1"), 5, "an item list records every version");

/* The one fetch of JSON (R-647): the envelope's ok, the status and the data; a network failure is an envelope too. */
const answered = (status, body) => async () => ({ ok: status >= 200 && status < 300, status, json: async () => body });
(async () => {
  let seen = null;
  let answer = await Req.apiRequest("POST", "/api/x", { a: 1 }, async (url, init) => {
    seen = { url, init };
    return { ok: true, status: 200, json: async () => ({ ok: true, item: 1 }) };
  });
  check(answer.ok && answer.status === 200 && answer.data.item === 1, "a success is {ok, status, data}");
  check(seen.url === "/api/x" && seen.init.method === "POST" && seen.init.body === '{"a":1}' && seen.init.headers["Content-Type"] === "application/json", "the body goes as JSON");
  answer = await Req.apiRequest("GET", "/api/x", undefined, answered(404, { ok: false, error: "not found", code: "not-found" }));
  check(!answer.ok && answer.status === 404 && Req.errorText(answer.data) === "not found", "a failure keeps its status and its error text");
  answer = await Req.apiRequest("GET", "/api/x", undefined, answered(200, { ok: false, error: "refused" }));
  check(!answer.ok, "an {ok: false} body is a failure whatever the status");
  answer = await Req.apiRequest("GET", "/api/x", undefined, async () => ({ ok: true, status: 200, json: async () => { throw new Error("not json"); } }));
  check(answer.ok && answer.data === null, "an answer that is not JSON has no data");
  answer = await Req.apiRequest("GET", "/api/x", undefined, async () => { throw new Error("down"); });
  check(!answer.ok && answer.status === 0 && answer.data.code === "unreachable" && Req.errorText(answer.data).includes("down"), "a network failure is an envelope");
  finish();
})();
