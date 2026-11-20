#ifndef URSATZ_CLI_EXPORT_H
#define URSATZ_CLI_EXPORT_H

#define EXPORT_USAGE_LINE \
    "Usage: ursatz export <plan.json> [--midi <out.mid>] [--musicxml <out.xml>] [--tempo <bpm>] [--plain-midi]"

/* "ursatz export ..."; argv[1] is "export". Returns the exit status. */
int run_export(int argc, char **argv);

#endif
