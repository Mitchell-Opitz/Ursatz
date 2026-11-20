/* Covers the base versions the page sends (public/request.js, R-646): an answer that holds an item records its current version, and a write to
 * an item that has versions carries it as base_version (in the JSON body, in the query of a DELETE); every other request goes as it was. Plain
 * Node script, run through ctest. */
import Req from "../public/core/request.js";
import { check, same, finish } from "./support/check.js";

const eq = (actual, expected, what) => check(same(actual, expected), what);

Req.forgetVersions();
eq(Req.knownVersion("mot-1"), null, "no version is known before an answer");
Req.rememberVersions({ ok: true, item: { id: "mot-1", current_version: 3 }, body: {} });
Req.rememberVersions({ ok: true, items: [{ id: "thm-1", current_version: 2 }, { id: "pce-1", current_version: 5 }] });
eq(Req.knownVersion("mot-1"), 3, "an item answer records the version");
eq(Req.knownVersion("pce-1"), 5, "an item list records every version");

const save = Req.withBaseVersion(Req.jsonRequest("POST", "/api/materials/items/mot-1/versions", { body: { uses: [] }, note: "n" }));
eq(JSON.parse(save.body), { body: { uses: [] }, note: "n", base_version: 3 }, "a new version carries the base version");
const compose = Req.withBaseVersion({ ...Req.jsonRequest("POST", "/api/materials/items/thm-1/compose", { seed: 1 }), job: true });
eq(JSON.parse(compose.body).base_version, 2, "a compose carries it");
check(compose.job === true, "a job request stays a job");
eq(
  JSON.parse(Req.withBaseVersion(Req.jsonRequest("POST", "/api/materials/items/pce-1/versions/4/regenerate", { section_id: "a" })).body).base_version,
  5,
  "a regenerate carries it",
);
eq(JSON.parse(Req.withBaseVersion({ method: "POST", url: "/api/materials/items/mot-1/reroll" }).body), { base_version: 3 }, "a request with no body gets one");
eq(Req.withBaseVersion({ method: "DELETE", url: "/api/materials/items/mot-1" }).url, "/api/materials/items/mot-1?base_version=3", "a delete has it in the query");
eq(
  Req.withBaseVersion({ method: "DELETE", url: "/api/materials/items/mot-1?force=1" }).url,
  "/api/materials/items/mot-1?force=1&base_version=3",
  "a forced delete has it after force",
);
eq(JSON.parse(Req.withBaseVersion(Req.jsonRequest("PATCH", "/api/materials/items/mot-1", { name: "x" })).body).base_version, 3, "a rename carries it");

const read = { method: "GET", url: "/api/materials/items/mot-1" };
eq(Req.withBaseVersion(read), read, "a read goes as it was");
const create = Req.jsonRequest("POST", "/api/materials/items", { type: "motif" });
eq(Req.withBaseVersion(create), create, "a new item has no version");
const validate = Req.jsonRequest("POST", "/api/materials/items/validate", { type: "motif", body: {} });
eq(Req.withBaseVersion(validate), validate, "a validate is no write");
const variants = Req.jsonRequest("POST", "/api/materials/items/mot-1/variants", { count: 1 });
eq(Req.withBaseVersion(variants), variants, "variants change nothing");
const unknown = Req.jsonRequest("POST", "/api/materials/items/mot-9/versions", { body: {} });
eq(Req.withBaseVersion(unknown), unknown, "an item the page has not read goes as it was");
const given = Req.jsonRequest("POST", "/api/materials/items/mot-1/versions", { body: {}, base_version: 1 });
eq(Req.withBaseVersion(given), given, "a request that gives its own base version keeps it");

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
