/* Covers the one module the page changes anything through (public/core/commands.js, R-668): the command object it posts to /api/workshop/command (the
 * body, the actor, the target, the base version of a version the page holds and none without one), a file upload's headers, a refusal's path, a job answer, and
 * that every command name the page sends is defined by the workshop's schemas (workshop/commands/*.json, found through URSATZ_GUI_COMMANDS_DIR). Plain Node
 * script, run through ctest. */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import Commands from "../public/core/commands.js";
import Req from "../public/core/request.js";
import { check, same, finish } from "./support/check.js";

const eq = (actual, expected, what) => check(same(actual, expected), what);
const answered = (status, body) => async () => ({ ok: status >= 200 && status < 300, status, json: async () => body });

(async () => {
  Req.forgetVersions();

  /* the command object */
  eq(
    Commands.commandObject("project.create", undefined, { name: "P" }),
    { command: "project.create", actor: "michelle", params: { name: "P" } },
    "a command with no target is the name, the actor and the params",
  );
  eq(Commands.commandObject("subject.create", undefined, undefined), { command: "subject.create", actor: "michelle", params: {} }, "no params is {}");
  eq(
    Commands.commandObject("item.save", "mot-1", { body: {} }),
    { command: "item.save", actor: "michelle", target: "mot-1", params: { body: {} } },
    "a target the page has not read carries no base version",
  );
  Req.rememberVersions({ ok: true, item: { id: "mot-1", current_version: 3 } });
  eq(
    Commands.commandObject("item.save", "mot-1", { body: {}, note: "n" }),
    { command: "item.save", actor: "michelle", target: "mot-1", base_version: 3, params: { body: {}, note: "n" } },
    "the version the page holds for the target is the base version",
  );
  eq(Commands.commandObject("project.update", "prj-1", { name: "x" }).base_version, undefined, "a target that is not an item has none");

  /* the request that is sent */
  const json = Commands.commandRequest("item.compose", { target: "mot-1", params: { seed: 2 } });
  check(json.method === "POST" && json.url === "/api/workshop/command" && json.headers["Content-Type"] === "application/json", "a command is a JSON POST to the generic route");
  eq(JSON.parse(json.body), { command: "item.compose", actor: "michelle", target: "mot-1", base_version: 3, params: { seed: 2 } }, "its body is the command object");

  /* a file goes as the body, the command and the file's name in the headers, percent-encoded */
  const bytes = new Uint8Array([77, 84, 104, 100]);
  const upload = Commands.commandRequest("library.import", { params: { filename: "Bach 114.mid" }, file: bytes, filename: "Bach 114.mid" });
  check(upload.body === bytes && upload.headers["Content-Type"] === "application/octet-stream", "an upload sends the file as the body");
  eq(
    JSON.parse(decodeURIComponent(upload.headers["X-Ursatz-Command"])),
    { command: "library.import", actor: "michelle", params: { filename: "Bach 114.mid" } },
    "the command is in the X-Ursatz-Command header",
  );
  check(decodeURIComponent(upload.headers["X-Filename"]) === "Bach 114.mid" && !upload.headers["X-Filename"].includes(" "), "the file's name is in X-Filename, percent-encoded");
  const named = Commands.commandRequest("library.import", { file: { name: "from-file.mid" } });
  check(decodeURIComponent(named.headers["X-Filename"]) === "from-file.mid", "the name of the file itself is the default name");

  /* sendCommand resolves like sendRequest */
  let seen = null;
  let answer = await Commands.sendCommand("project.create", { params: { name: "P" } }, async (url, init) => {
    seen = { url, init };
    return { ok: true, status: 200, json: async () => ({ ok: true, project: { id: "prj-1" } }) };
  });
  check(answer.ok && answer.status === 200 && answer.data.project.id === "prj-1", "a success is {ok, status, data}");
  check(seen.url === "/api/workshop/command" && seen.init.method === "POST" && JSON.parse(seen.init.body).command === "project.create", "the command was posted");
  answer = await Commands.sendCommand("project.create", { params: {} }, answered(400, { ok: false, error: "name is required", code: "request", path: "/params/name" }));
  check(!answer.ok && answer.status === 400 && answer.data.path === "/params/name" && Req.errorText(answer.data) === "name is required", "a refusal keeps its status, its text and the member it refused");
  answer = await Commands.sendCommand("item.save", { target: "mot-1", params: { body: {} } }, answered(409, { ok: false, error: "changed since you read it", code: "conflict", path: "/base_version", current_version: 5 }));
  check(!answer.ok && answer.status === 409 && answer.data.current_version === 5, "a stale base version is a conflict with the current version");
  answer = await Commands.sendCommand("item.compose", { target: "mot-1" }, answered(202, { ok: true, job: { id: "job-1", kind: "theme", state: "queued" } }));
  check(answer.ok && answer.status === 202 && answer.data.job.id === "job-1", "a job a command started is ok, 202, with the job");
  answer = await Commands.sendCommand("project.create", { params: { name: "P" } }, async () => {
    throw new Error("down");
  });
  check(!answer.ok && answer.status === 0 && answer.data.code === "unreachable", "a network failure is an envelope");
  answer = await Commands.sendCommand("job.cancel", { target: "job-1" });
  check(answer && typeof answer.ok === "boolean", "sendCommand takes the page's fetch by default");

  /* every command the page sends is one the workshop defines */
  const commandsDir = process.env.URSATZ_GUI_COMMANDS_DIR;
  if (!commandsDir) {
    check(false, "URSATZ_GUI_COMMANDS_DIR names workshop/commands");
  } else {
    const known = new Set();
    for (const file of fs.readdirSync(commandsDir).filter((name) => name.endsWith(".json"))) {
      for (const command of JSON.parse(fs.readFileSync(path.join(commandsDir, file), "utf8")).commands) {
        known.add(command.name);
      }
    }
    check(known.size >= 56 && known.has("item.restore") && known.has("section.insert"), "the schemas define the workshop's commands");
    const publicDir = path.join(path.dirname(fileURLToPath(import.meta.url)), "..", "public");
    const sent = new Set();
    const walk = (dir) => {
      for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        if (entry.name === "vendor") {
          continue;
        }
        const full = path.join(dir, entry.name);
        if (entry.isDirectory()) {
          walk(full);
        } else if (entry.name.endsWith(".js")) {
          for (const match of fs.readFileSync(full, "utf8").matchAll(/(?:sendCommand|commandRequest|commandObject)\(\s*["']([a-z]+\.[a-z-]+)["']/g)) {
            sent.add(match[1]);
          }
        }
      }
    };
    walk(publicDir);
    check(sent.size >= 20, `the page sends ${sent.size} different commands`);
    const unknown = [...sent].filter((name) => !known.has(name));
    check(unknown.length === 0, `every command the page sends is defined (unknown: ${unknown.join(", ") || "none"})`);
  }
  finish();
})();
