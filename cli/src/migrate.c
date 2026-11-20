#include "migrate.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

#include "cli_options.h"
#include "session.h"

int run_migrate(const CliArgs *args) {
    char data[PATH_MAX];
    int status = cli_data_folder(cli_global(), data, sizeof data);
    if (status != 0) {
        return status;
    }
    char *backup = cli_value(args, "--backup") ? cli_path_absolute(cli_value(args, "--backup")) : NULL;
    if (cli_value(args, "--backup") && !backup) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    WorkshopMigrateOptions options;
    memset(&options, 0, sizeof options);
    options.dry_run = cli_switch(args, "--dry-run");
    options.backup_dir = backup;

    long pid = 0;
    int port = 0;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    if (workshop_lock_holder(data, &pid, &port, &error) == URSATZ_OK) {
        free(backup);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "stop the Ursatz process %ld first: the data folder %s is in use", pid, data);
    }
    memset(&error, 0, sizeof error);
    cJSON *report = NULL;
    if (workshop_migrate(data, &options, &report, &error) != URSATZ_OK) {
        free(backup);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", error.message);
    }
    free(backup);
    char *text = report ? cJSON_Print(report) : NULL;
    printf("%s\n", text ? text : "{}");
    free(text);
    cJSON_Delete(report);
    return 0;
}
