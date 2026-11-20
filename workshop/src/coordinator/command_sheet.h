#ifndef URSATZ_WORKSHOP_COORDINATOR_COMMAND_SHEET_H
#define URSATZ_WORKSHOP_COORDINATOR_COMMAND_SHEET_H

#include <cJSON.h>

/*
 * The text generated from the command schemas (R-664): the reference sheet, its markdown page, the grammar and the help of one command. The tool
 * "commands" (params format: sheet, markdown, grammar, json or help; name for help) answers {"format", "text"} with these. Every function answers a
 * string the caller frees, NULL when out of memory. Nothing here is written by hand: a command's line is made from its schema.
 */

/* The rules of the text form (at most 15 lines), then a heading and a line a command for each family: at most 120 lines in all. */
char *workshop_command_sheet_text(void);

/* The sheet as workshop/docs/command-reference.md: a title, the rules and a table a family. */
char *workshop_command_sheet_markdown(void);

/* An EBNF of the text form: the generic productions and one a command. */
char *workshop_command_sheet_grammar(void);

/* The schemas of every command as one JSON array (the frontend and the tests read it). */
char *workshop_command_sheet_json(void);

/* The usage line, the summary, each param, the reply, the job, the base and the example of one command (a schema of command_schemas.h). */
char *workshop_command_sheet_help(const cJSON *schema);

#endif
