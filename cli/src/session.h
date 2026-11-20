#ifndef URSATZ_CLI_SESSION_H
#define URSATZ_CLI_SESSION_H

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

/*
 * A command's way to the data folder (R-659): the program opens the workshop under the folder's lock, or, when another Ursatz process holds the
 * folder and serves HTTP, connects to it. The command calls queries and commands without knowing which.
 *
 * The global flags --data <dir>, --port <n> and --actor <name> stand before the subcommand (ursatz --data <dir> status); the folder is --data or
 * the environment variable URSATZ_DATA.
 */
typedef struct {
    const char *data; /* --data, NULL when not given */
    int port;         /* --port, 0 when not given */
    const char *actor; /* --actor, "claude" when not given */
} CliGlobal;

/* The global flags of this process (set by main before the command runs). */
const CliGlobal *cli_global(void);
void cli_global_set(const CliGlobal *global);

/* The data folder of the process as an absolute path (--data, else URSATZ_DATA) into out: 0, or the exit status after printing the failure. */
int cli_data_folder(const CliGlobal *global, char *out, size_t size);

typedef struct CliSession CliSession;

/*
 * Opens a session: *out is the session on 0. Returns 0, or the exit status after printing the failure:
 *   1  "Error: name the data folder with --data <dir> or URSATZ_DATA"
 *   2  a folder another process holds that serves no HTTP ("...is in use by another Ursatz process (pid N); it takes no connections, so run again when
 *      it ends"), or a port that serves another folder, or any other failure of the open.
 * Every path argument of a command is made absolute (cli_path_absolute) before this runs: the workshop works in the data folder from its start.
 */
int cli_session_open(const CliGlobal *global, CliSession **out);

/* Whether the session opened the workshop here (local) or connected to the process that holds the folder (remote). */
bool cli_session_is_local(const CliSession *session);
/* The data folder as an absolute path. */
const char *cli_session_data_dir(const CliSession *session);
/* The workshop of a local session, NULL for a remote one. */
Workshop *cli_session_workshop(const CliSession *session);
/* The port of the process a remote session is connected to, 0 for a local one. */
int cli_session_port(const CliSession *session);

UrsatzStatus cli_session_query(CliSession *session, const char *name, const cJSON *params, WorkshopReply *reply, UrsatzError *error);

/* command is the command object without its actor (the session adds it: the --actor flag); it is changed, not consumed. */
UrsatzStatus cli_session_command(CliSession *session, cJSON *command, const WorkshopBytes *bytes, WorkshopReply *reply, UrsatzError *error);

/* Polls job.get every 200 ms until the job is done, failed or cancelled; *job is a new object (the caller deletes it) holding the job's members.
 * Returns 0 or the status of a failed poll. */
UrsatzStatus cli_session_wait_job(CliSession *session, const char *job_id, cJSON **job, UrsatzError *error);

void cli_session_close(CliSession *session);

/* path made absolute against the starting directory of the process ("" stays ""); a malloc'ed string. */
char *cli_path_absolute(const char *path);

#endif
