#ifndef URSATZ_GUI_COMPOSER_SERVICE_H
#define URSATZ_GUI_COMPOSER_SERVICE_H

/* The only code that starts the Composer (R-086). It runs the Composer
 * command line with posix_spawn and an argument array, never through a
 * shell, and never changes the working directory: every path in the
 * arguments is absolute. */

typedef enum {
  URSATZ_GUI_COMPOSER_OK = 0,
  URSATZ_GUI_COMPOSER_ERR_NO_BINARY,
  URSATZ_GUI_COMPOSER_ERR_TIMEOUT,
  URSATZ_GUI_COMPOSER_ERR_SPAWN_FAILED,
  URSATZ_GUI_COMPOSER_ERR_ALLOC
} UrsatzGuiComposerResult;

typedef struct {
  int exit_status;
  char *stdout_text;
  char *stderr_text;
} UrsatzGuiComposerOutput;

/* The Composer executable: the environment variable URSATZ_GUI_COMPOSER when
 * it is set and not empty, else the one built with the backend
 * (URSATZ_GUI_COMPOSER_BIN). NULL when that file cannot be executed. */
const char *ursatz_gui_composer_binary(void);

/* Runs the Composer with args (NULL-terminated, without the program name),
 * waits for it and captures stdout and stderr. The wait is limited to 120
 * seconds, or to URSATZ_GUI_COMPOSER_TIMEOUT seconds when that is set; a run
 * that takes longer is killed with SIGKILL and gives ERR_TIMEOUT. On OK, out
 * holds the exit status and both texts (never NULL). On any other result out
 * is left empty. */
UrsatzGuiComposerResult ursatz_gui_composer_run(const char *const *args, UrsatzGuiComposerOutput *out);

void ursatz_gui_composer_output_destroy(UrsatzGuiComposerOutput *out);

#endif /* URSATZ_GUI_COMPOSER_SERVICE_H */
