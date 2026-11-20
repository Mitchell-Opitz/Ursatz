#ifndef URSATZ_WORKSHOP_COMPOSITION_SECTIONS_H
#define URSATZ_WORKSHOP_COMPOSITION_SECTIONS_H

#include <cJSON.h>

#include <common/status.h>

/*
 * The edits of the section commands (R-666) on a piece request: pure functions on the Composer's piece request (cJSON in, cJSON out), with no
 * data folder and no Composer call. `command` is section.insert, section.delete, section.move, section.resize or section.set and `params` the
 * command's params, already checked against its schema (types, limits, exactly one of after and before, of bars and beats).
 *
 * A game loop request (`schema_version` 1, or `form` "game-loop") takes every command on its `sections`. A form piece (`schema_version` 2 with
 * another form) takes its sections from its form: only section.set of `region`, `harmony` and `fill` is allowed, written into the entry of its
 * `sections` overlay that has `name` equal to the id (made when missing); anything else is URSATZ_ERR_REQUEST at "/target", "a form piece takes
 * its sections from its form". An unknown id, after or before is URSATZ_ERR_NOT_FOUND at its param; an id taken is URSATZ_ERR_CONFLICT at
 * "/params/id"; deleting the last section is URSATZ_ERR_REQUEST, "a piece keeps at least one section". An inserted section is written
 * {id, role, letter?, bars|beats, pickup?, cadence?, function?, fill?, region?, harmony?}; a section that says `part` keeps it (set role writes
 * it there); resize writes the unit given and removes the other; the words `none` (function, cadence, harmony) and `home` (region) remove the
 * member.
 *
 * *edited is a new request (the caller deletes it); *note is the version note the command gives when its params give none ("insert section T0
 * after s0"), written into the buffer.
 */
UrsatzStatus workshop_sections_edit(const char *command, const cJSON *request, const cJSON *params, cJSON **edited, char *note, size_t note_size,
                                    UrsatzError *error);

/* The ids of the request's sections in order (a loop's ids, a form piece's overlay names) as an array of strings; a new array. */
cJSON *workshop_sections_ids(const cJSON *request);

#endif
