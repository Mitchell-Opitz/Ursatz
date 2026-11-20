#ifndef URSATZ_WORKSHOP_COORDINATOR_COMMAND_SCHEMAS_H
#define URSATZ_WORKSHOP_COORDINATOR_COMMAND_SCHEMAS_H

#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The commands as schemas (R-664): workshop/commands/<family>.json, validated by workshop/schema/ursatz-commands.v1.schema.json and embedded in
 * the build. The files are parsed once, at the first workshop_open or workshop_tools_open of the process, into a set every workshop shares; the
 * set is read only. A file that does not parse, one that is not a command file or a name defined twice fails the open with URSATZ_ERR_INTERNAL, and
 * so does a command with no row in the dispatch table or a row with no schema (dispatch.c's workshop_command_names). The command entry of a
 * schema is {name, summary, target, base, journaled, bytes, params, one_of, reply, job, example}.
 */

/* Parses the files and checks them against the dispatch table (idempotent: the first call does the work, a later one answers its result). */
UrsatzStatus workshop_command_schemas_open(UrsatzError *error);

/* The schema of a command by name, or NULL for an unknown name (or before the open). */
const cJSON *workshop_command_schema(const char *name);

/* The families in file order, each {schema, schema_version, family, summary, commands}; NULL out of range. */
size_t workshop_command_family_count(void);
const cJSON *workshop_command_family_at(size_t index);

/* The names of the dispatch table's commands, for the parity check and the tests; *count is the number of names. */
const char *const *workshop_command_names(size_t *count);

#endif
