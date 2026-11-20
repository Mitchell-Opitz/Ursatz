#ifndef URSATZ_CLI_TOOLS_H
#define URSATZ_CLI_TOOLS_H

#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

/*
 * The workshop every command of this process works with (R-656, R-657): the program calls the workshop and nothing else.
 *
 * cli_tools() answers the workshop the command runs on: the data folder workshop when the session opened one locally, else a tools workshop
 * (workshop_tools_open) opened on the first call and closed when the process exits. When it cannot be opened the reason is printed once
 * ("Error: cannot read the knowledge: <message>") and NULL comes back, so every call refuses.
 */
Workshop *cli_tools(void);

/*
 * What a call of the composer tool answers (the members of the workshop's reply, as the Composer API's response had them): the json, the plan
 * file and the MusicXML as the Composer wrote them, the MIDI file, and, for a refusal, the error unchanged (its path points into the request).
 * Unused members are NULL (or 0).
 */
typedef struct {
    char *json;
    char *plan_json;
    uint8_t *midi;
    size_t midi_size;
    char *musicxml;
    UrsatzError error;
} CliResponse;

/* Frees and zeroes the response. Safe on a zeroed response and when called twice. */
void cli_response_destroy(CliResponse *response);

/*
 * Calls the composer tool: entry is the Composer entry's name ("progression", "motive_from_midi", "vocabulary"...), request the request object
 * (NULL for the entries that take none: versions, vocabulary, variant_operations; it stays the caller's), bytes the MIDI file of
 * "motive_from_midi" (NULL for the others). *out is zeroed first. The status is the Composer's; on a refusal out->error holds its message and
 * path.
 */
UrsatzStatus cli_composer(const char *entry, const cJSON *request, const WorkshopBytes *bytes, CliResponse *out);

/* The same for the entry "validate" of kind ("motive", "theme", "piece", "random_motive"...). */
UrsatzStatus cli_composer_validate(const char *kind, const cJSON *request, CliResponse *out);

#endif
