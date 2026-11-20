/* The command a request carries (core/commands.js commandRequest): the command object of its JSON body, its params, the check that it is a command. */
export const command = (request) => JSON.parse(request.body);
export const params = (request) => command(request).params;
export const isCommand = (request, name, target) => {
  const c = command(request);
  return request.method === "POST" && request.url === "/api/workshop/command" && c.command === name && c.actor === "michelle" && c.target === target;
};
