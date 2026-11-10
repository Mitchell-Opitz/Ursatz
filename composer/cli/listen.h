#ifndef URSATZ_COMPOSER_CLI_LISTEN_H
#define URSATZ_COMPOSER_CLI_LISTEN_H

#define LISTEN_USAGE_LINE                                                                                              \
    "Usage: ursatz-composer listen --what motifs|themes --out-dir <dir> [--count N] [--seed N] [--settings <file>] "   \
    "[--mode major|minor] [--meter n/m] [--style <file>] [--blend <file>] [--blend-weight <n/d>] "                     \
    "[--blend-weights <list>] [--tempo <bpm>] [--strategy segments|drawn] [--into sentence|period] "                    \
    "[--development melody|balanced|theme] [--bars 4|8] [--check] [--motif <file> | --theme-request <file>] "           \
    "| --print-settings"

/*
 * The `listen` subcommand: argv[1] is "listen". A batch of motifs or themes from fixed seeds, written as files a listening
 * session plays and reads (docs/cli-listen.md). Returns the process exit status:
 *   0  at least one sample was written
 *   1  usage, an invalid option or an unreadable file
 *   2  no sample could be composed
 *   3  an output file could not be written
 */
int run_listen(int argc, char **argv);

#endif
