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
  /* The fetch of a text answer: the status, the content type and the text; a network failure has status 0. */
  const texted = (status, type, text) => async () => ({ ok: status >= 200 && status < 300, status, headers: { get: () => type }, text: async () => text });
  let text = await Req.sendText("/api/score.xml", texted(200, "application/vnd.recordare.musicxml+xml", "<score/>"));
  check(text.ok && text.status === 200 && text.contentType.includes("musicxml") && text.text === "<score/>", "a text answer is {ok, status, contentType, text}");
  text = await Req.sendText({ url: "/api/x", method: "POST", body: "b" }, async (url, init) => ({ ok: true, status: 200, headers: { get: () => null }, text: async () => `${url} ${init.method} ${init.body}` }));
  check(text.text === "/api/x POST b" && text.contentType === "", "a request object sends its method and body");
  text = await Req.sendText("/api/x", texted(404, "application/json", '{"error":"no"}'));
  check(!text.ok && text.status === 404 && text.text === '{"error":"no"}', "a failure status keeps its text");
  text = await Req.sendText("/api/x", async () => { throw new Error("down"); });
  check(!text.ok && text.status === 0 && text.text === "" && text.error.includes("down"), "a network failure of a text fetch has status 0");
  finish();
})();
