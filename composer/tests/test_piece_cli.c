#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

/* The `piece` command, run as a process (docs/cli-piece.md, docs/piece-request.md). */

int main(void) {
    char line[2048];
    snprintf(line, sizeof(line), "\"%s\" piece --help >/dev/null 2>&1", COMPOSER_CLI_PATH);
    int status = system(line);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("test_piece_cli: ok\n");
    return 0;
}
