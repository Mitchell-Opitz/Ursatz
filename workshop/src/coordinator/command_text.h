#ifndef URSATZ_WORKSHOP_COORDINATOR_COMMAND_TEXT_H
#define URSATZ_WORKSHOP_COORDINATOR_COMMAND_TEXT_H

#include <cJSON.h>

#include <common/status.h>

/*
 * Commands written in Ursatz Text (R-665): the tool "commands-parse" reads them into the command objects `ursatz apply` sends. One command a line,
 *   <name> [<kind>:<id>[@v<n>]] {<param> <value>}
 * the target present when the command's schema has one (its kind the schema's), `@v<n>` (the version read) required when the schema's `base` is
 * true, a value one unit read by the param's type (a number for a string is its text, n/d for a rational is the string "n/d", [a b] an array of
 * scalars). Params of any type may also be written below the command, at two spaces, as member lines of the tree form (an object param is
 * `  region relation omt.relation.dominant anchor home`, read with text_block_parse); `file <path>` ends a command whose schema takes a file.
 * Comment lines and blank lines are ignored and the header `ursatz-text 1 commands` is allowed first.
 *
 * *out is {"commands": [{command, target?, base_version?, params, file?}]} (no actor); a failure is URSATZ_ERR_REQUEST at "/params/text" with
 * "line <n>: <message>": an unknown command, a target of another kind than the command works on, a missing version, an unknown param, a param
 * given twice, a param with no value or a value of another type than its param's.
 */
UrsatzStatus workshop_command_text_parse(const char *text, cJSON **out, UrsatzError *error);

#endif
