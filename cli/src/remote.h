#ifndef URSATZ_CLI_REMOTE_H
#define URSATZ_CLI_REMOTE_H

#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

/*
 * The client of the workshop routes of the Ursatz process that holds a data folder (R-659), over civetweb: one request per connection to
 * 127.0.0.1, with a 60 s timeout for the answer. The answer becomes the WorkshopReply a local call would have given.
 *
 * A JSON answer is the reply's json without "ok"; any other content type is the reply's bytes, with its content type and file name. A refusal
 * ({"ok": false, "error", "code", "path"}) is its status (ursatz_status_parse of "code"), message and path, with the other members ("current_version")
 * kept in the reply's json. A connection that fails is URSATZ_ERR_IO, "cannot reach the Ursatz process on port <p>: <civetweb's message>".
 *
 * extra_headers is "" or header lines each ending in "\r\n" (the file of a command goes with X-Ursatz-Command and X-Filename).
 */
UrsatzStatus cli_remote_request(int port, const char *method, const char *path, const char *content_type, const char *extra_headers,
                                const uint8_t *body, size_t size, WorkshopReply *reply, UrsatzError *error);

/* GET /api/workshop/status of the process on port (the reply's json holds its members). */
UrsatzStatus cli_remote_request_status(int port, WorkshopReply *reply, UrsatzError *error);

/* The text percent-encoded (every byte but a letter, a digit, "-", "_", "." and "~"); a malloc'ed string, NULL when out of memory. */
char *cli_percent_encode(const char *text);

#endif
