/* The one way the page changes anything (R-668): a command object sent to POST /api/workshop/command. The backend's routes translate; the commands and
 * their params are the workshop's (workshop/commands/*.json, the generated command reference). The version the page holds for the target (core/request.js
 * knownVersion) travels as base_version, so the workshop refuses a change made over a newer version ("changed since you read it", with current_version).
 * No DOM. */

import api from "./request.js";

const COMMAND_URL = "/api/workshop/command";

/* The command object: its name, the actor, the target and the version the page holds of it, the params ({} when none). */
function commandObject(name, target, params) {
  const command = { command: name, actor: "michelle" };
  if (typeof target === "string" && target !== "") {
    command.target = target;
    const version = api.knownVersion(target);
    if (version !== null) {
      command.base_version = version;
    }
  }
  command.params = params === undefined || params === null ? {} : params;
  return command;
}

/* The fetch request of a command: a JSON body, or, for a file, the file as the body with the command (and the file's name) in the headers the
 * backend reads (X-Ursatz-Command, X-Filename), percent-encoded as the command line sends them. */
function commandRequest(name, { target, params, file, filename } = {}) {
  const command = commandObject(name, target, params);
  if (file === undefined || file === null) {
    return { method: "POST", url: COMMAND_URL, headers: { "Content-Type": "application/json" }, body: JSON.stringify(command) };
  }
  return {
    method: "POST",
    url: COMMAND_URL,
    headers: {
      "Content-Type": "application/octet-stream",
      "X-Ursatz-Command": encodeURIComponent(JSON.stringify(command)),
      "X-Filename": encodeURIComponent(filename || (file && typeof file.name === "string" ? file.name : "")),
    },
    body: file,
  };
}

/*
 * Sends the command `name`: {target, params, file, filename}, all optional (target an id, params an object, file the bytes of an upload). Resolves like
 * sendRequest, {ok, status, data}: a refusal is ok false with the refused member at data.path, a job a command started is ok true, status 202 and
 * data.job; a network failure is status 0. fetchFn is the page's fetch.
 */
async function sendCommand(name, options, fetchFn) {
  return api.sendRequest(commandRequest(name, options || {}), fetchFn);
}

export { sendCommand, commandRequest, commandObject };
export default { sendCommand, commandRequest, commandObject };
