#ifndef URSATZ_GUI_TOOL_H
#define URSATZ_GUI_TOOL_H

/*
 * Tool module convention.
 *
 * A "tool" is an independent GUI module (Analysis, Corpus, Generator, ...).
 * Tools never call each other directly, and never call Ursatz persistence
 * functions directly — they go through the shared service layer in
 * backend/src/service/.
 *
 * To add a new tool:
 *   1. Create backend/src/routes/<tool_name>.c and .h.
 *   2. Expose a single registration function following the naming
 *      convention below, taking the civetweb context to register against.
 *   3. Call that registration function from server.c's route setup.
 *   4. Give the tool's HTTP routes a "/api/<tool_name>/..." prefix.
 *   5. Any data access goes through backend/src/service/, never straight
 *      into Ursatz's library calls from route code.
 *
 * health.c/health.h is the reference implementation of this pattern.
 */

struct mg_context;

typedef void (*ursatz_gui_tool_register_fn)(struct mg_context *ctx);

#endif /* URSATZ_GUI_TOOL_H */
