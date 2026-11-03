#ifndef URSATZ_COMPOSER_CLI_PLAN_H
#define URSATZ_COMPOSER_CLI_PLAN_H

#define PLAN_USAGE_LINE                                                                                                \
    "Usage: ursatz-composer plan --form <name> --key <K> [--mode major|minor] --meter <m> [--bars N] [--count N] "     \
    "[--modulate] [--motive <file> | --from-corpus <dir> --like <file>] [--accompaniment <p>] [--texture <t>] [--given-melody <f> | --given-progression "   \
    "\"<I IV V I>\" | --like-progression \"<I IV V I>\" (with --from-corpus) | --given-bass <f> [--at <L>[:<bar>]] [--cells N]]... [--seed N] [--style <file>] [--style-weight N/D] [--style-without <category>]... [--blend <file>] [--blend-weight N/D | --blend-weights D=N/D,...] --out <plan.json> "           \
    "[--format text|json]"

/* "ursatz-composer plan ..."; argv[1] is "plan". Returns the exit status. */
int run_plan(int argc, char **argv);

#endif
