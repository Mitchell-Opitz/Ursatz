#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "flags.h"

/*
 * The flag table and its parser (R-657): the table is sorted and unique, every flag a command row names is in it and every flag in it is named by a
 * row; the parser reads switches, values, repeated values with their positions, the alias and the inputs, and fails with the four messages.
 */

static const CliCommand *row(const char *name) {
    const CliCommand *command = cli_command_find(name);
    assert(command);
    return command;
}

static bool parses(const char *command_name, char **words, int count, CliArgs *out, char *message, size_t size) {
    char *argv[32] = {(char *)"ursatz", (char *)command_name};
    assert(count + 2 < 32);
    for (int i = 0; i < count; i++) {
        argv[2 + i] = words[i];
    }
    return cli_parse(row(command_name), count + 2, argv, out, message, size);
}

static void test_the_table(void) {
    for (size_t i = 1; i < CLI_FLAG_COUNT; i++) {
        assert(strcmp(CLI_FLAGS[i - 1].name, CLI_FLAGS[i].name) < 0); /* sorted and unique */
    }
    for (size_t i = 0; i < CLI_FLAG_COUNT; i++) {
        assert(strncmp(CLI_FLAGS[i].name, "--", 2) == 0 && CLI_FLAGS[i].help && CLI_FLAGS[i].help[0]);
        assert((CLI_FLAGS[i].kind == CLI_FLAG_SWITCH) == (CLI_FLAGS[i].value == NULL));
    }
    bool named[CLI_FLAG_CAPACITY] = {false};
    for (size_t c = 0; c < CLI_COMMAND_COUNT; c++) {
        const CliCommand *command = &CLI_COMMANDS[c];
        assert(command->run && command->summary && command->usage && command->description);
        assert(command->inputs_min >= 0 && command->inputs_min <= command->inputs_max);
        for (size_t other = 0; other < c; other++) {
            assert(strcmp(CLI_COMMANDS[other].name, command->name) != 0);
        }
        for (const char *const *name = command->flags; name && *name; name++) {
            const CliFlag *flag = cli_flag_find(*name);
            assert(flag && strcmp(flag->name, *name) == 0); /* a row names the flag by its name, never by an alias */
            named[flag - CLI_FLAGS] = true;
        }
    }
    for (size_t i = 0; i < CLI_FLAG_COUNT; i++) {
        if (!named[i]) {
            fprintf(stderr, "the flag %s is named by no command\n", CLI_FLAGS[i].name);
        }
        assert(named[i]);
    }
}

static void test_switch_value_and_inputs(void) {
    CliArgs args;
    char message[256];
    char *words[] = {(char *)"--key", (char *)"D minor", (char *)"--no-bass", (char *)"--seed", (char *)"7"};
    assert(parses("progression", words, 5, &args, message, sizeof message));
    assert(strcmp(cli_value(&args, "--key"), "D minor") == 0 && strcmp(cli_value(&args, "--seed"), "7") == 0);
    assert(cli_switch(&args, "--no-bass") && !cli_switch(&args, "--bass") && cli_value(&args, "--meter") == NULL);
    assert(!args.help && args.input_count == 0);

    /* a value may start with "-": it is the next argument as it is */
    char *dash[] = {(char *)"--key", (char *)"-x"};
    assert(parses("progression", dash, 2, &args, message, sizeof message) && strcmp(cli_value(&args, "--key"), "-x") == 0);

    /* an input, and a lone "-" is an input */
    char *input[] = {(char *)"theme.txt", (char *)"--meter", (char *)"4/4"};
    assert(parses("motive", input, 3, &args, message, sizeof message));
    assert(args.input_count == 1 && strcmp(args.inputs[0], "theme.txt") == 0 && strcmp(cli_value(&args, "--meter"), "4/4") == 0);
    char *lone[] = {(char *)"-"};
    assert(parses("motive", lone, 1, &args, message, sizeof message) && args.input_count == 1 && strcmp(args.inputs[0], "-") == 0);
}

static void test_repeated_flags_keep_their_positions(void) {
    CliArgs args;
    char message[256];
    char *words[] = {(char *)"--form", (char *)"binary", (char *)"--given-melody", (char *)"a.mid", (char *)"--at", (char *)"B:2",
                     (char *)"--given-bass", (char *)"b.mid", (char *)"--cells", (char *)"2", (char *)"--style-without", (char *)"x",
                     (char *)"--style-without", (char *)"y"};
    assert(parses("plan", words, 14, &args, message, sizeof message));
    assert(cli_repeat_count(&args, "--given-melody") == 1 && cli_repeat_count(&args, "--given-bass") == 1);
    int position = 0;
    assert(strcmp(cli_repeat_value(&args, "--given-melody", 0, &position), "a.mid") == 0 && position == 4);
    assert(strcmp(cli_repeat_value(&args, "--at", 0, &position), "B:2") == 0 && position == 6);
    assert(strcmp(cli_repeat_value(&args, "--given-bass", 0, &position), "b.mid") == 0 && position == 8);
    assert(strcmp(cli_repeat_value(&args, "--cells", 0, &position), "2") == 0 && position == 10);
    assert(cli_repeat_count(&args, "--style-without") == 2);
    assert(strcmp(cli_repeat_value(&args, "--style-without", 1, &position), "y") == 0 && position == 14);
    assert(cli_repeat_value(&args, "--style-without", 2, NULL) == NULL && cli_repeat_count(&args, "--tone") == 0);
}

static void test_the_alias_and_help(void) {
    CliArgs args;
    char message[256];
    char *alias[] = {(char *)"--mode", (char *)"minor", (char *)"--meter", (char *)"3/4"};
    assert(parses("random-motive", alias, 4, &args, message, sizeof message));
    assert(strcmp(cli_value(&args, "--material"), "minor") == 0);
    char *twice[] = {(char *)"--mode", (char *)"minor", (char *)"--material", (char *)"major"};
    assert(!parses("random-motive", twice, 4, &args, message, sizeof message));
    assert(strcmp(message, "--material given twice") == 0);
    char *help[] = {(char *)"--meter", (char *)"4/4", (char *)"--help"};
    assert(parses("progression", help, 3, &args, message, sizeof message) && args.help);
}

static void test_the_messages(void) {
    CliArgs args;
    char message[256];
    char *unknown[] = {(char *)"--bogus"};
    assert(!parses("progression", unknown, 1, &args, message, sizeof message));
    assert(strcmp(message, "unknown option '--bogus' (see ursatz progression --help)") == 0);
    char *other[] = {(char *)"--tone", (char *)"3=chord"}; /* a flag of the table that this command does not take */
    assert(!parses("progression", other, 2, &args, message, sizeof message));
    assert(strcmp(message, "unknown option '--tone' (see ursatz progression --help)") == 0);
    char *missing[] = {(char *)"--seed"};
    assert(!parses("progression", missing, 1, &args, message, sizeof message));
    assert(strcmp(message, "--seed needs a value") == 0);
    char *given_twice[] = {(char *)"--seed", (char *)"1", (char *)"--seed", (char *)"2"};
    assert(!parses("progression", given_twice, 4, &args, message, sizeof message));
    assert(strcmp(message, "--seed given twice") == 0);
    char *too_many[] = {(char *)"a.txt", (char *)"b.txt"};
    assert(!parses("motive", too_many, 2, &args, message, sizeof message));
    assert(strcmp(message, "motive takes 1 input") == 0);
    assert(!parses("progression", too_many, 1, &args, message, sizeof message));
    assert(strcmp(message, "progression takes no input") == 0);
    assert(!parses("grow", (char *[]){(char *)"a", (char *)"b"}, 2, &args, message, sizeof message));
    assert(strcmp(message, "grow takes at most 1 input") == 0);
}

int main(void) {
    assert(CLI_COMMAND_COUNT == 21);
    test_the_table();
    test_switch_value_and_inputs();
    test_repeated_flags_keep_their_positions();
    test_the_alias_and_help();
    test_the_messages();
    puts("ok");
    return 0;
}
