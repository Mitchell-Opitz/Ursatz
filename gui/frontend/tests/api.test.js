/* Covers the page's JSON calls (public/core/api.js, R-647, R-671): a query is a POST of its params as JSON to its own url, a fetch of JSON is a GET
 * unless told otherwise, and a command that runs as a job answers not-ok when the job's answer says so. Plain Node script, run through ctest. */
import { queryResult, fetchJsonResult, commandJobResult } from "../public/core/api.js";
import { check, same, finish } from "./support/check.js";
import { fakeFetch } from "./support/dom.js";

/* A fetch that keeps what it was asked, headers included (fakeFetch's log has the method, the url and the body only). */
const sent = [];
globalThis.fetch = async (url, init) => {
  sent.push({ url: String(url), init: init || {} });
  return { ok: true, status: 200, json: async () => ({ ok: true, items: [] }) };
};

await queryResult("item.list", { a: 1 });
check(sent[0].url === "/api/workshop/query/item.list" && sent[0].init.method === "POST", "a query is a POST to /api/workshop/query/<name>");
check(sent[0].init.headers["Content-Type"] === "application/json" && sent[0].init.body === '{"a":1}', "its params go as JSON");
await queryResult("item.list");
await queryResult("item.list", null);
check(sent[1].init.body === "{}" && sent[2].init.body === "{}", "no params, and null, send an empty object");
await queryResult("a/b", {});
check(sent[3].url === "/api/workshop/query/a%2Fb", "the name of a query is percent-encoded");
const answer = await queryResult("item.list", {});
check(answer.ok && answer.status === 200 && same(answer.data.items, []), "a query resolves to {ok, status, data}");

await fetchJsonResult("/u");
check(sent[5].url === "/u" && sent[5].init.method === "GET", "a fetch of JSON is a GET");
await fetchJsonResult("/u", { method: "PUT", body: "{}" });
check(sent[6].init.method === "PUT" && sent[6].init.body === "{}", "its method and body are the caller's");

/* A command that runs as a job is followed to its end; a 200 whose answer says ok false is not ok. */
const calls = fakeFetch((request) => ({ ok: false, error: "refused" }));
const refused = await commandJobResult("item.compose", { target: "thm-1" });
check(calls.length >= 1 && calls[0].url === "/api/workshop/command", "the command is sent to the command route");
check(refused.status === 200 && refused.ok === false && refused.data.error === "refused", "a 200 whose answer has ok false is not ok");
fakeFetch(() => ({ ok: true, done: true }));
const accepted = await commandJobResult("item.compose", { target: "thm-1" });
check(accepted.ok === true && accepted.status === 200, "a 200 answer that is ok is ok");

finish();
