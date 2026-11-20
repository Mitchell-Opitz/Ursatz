#include "coordinator/command_sheet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/strings.h>

#include "coordinator/command_schemas.h"

/* The rules of the text form, one line each (the sheet and the markdown page print the same lines). */
static const char *const kRules[] = {
    "One command a line: <command> [<kind>:<id>[@v<n>]] {<param> <value>}.",
    "The target is what the command works on, written <kind>:<id>: item, piece, collection, style, links, project, subject or job.",
    "A command that changes an item that has versions names the version you read: item:<id>@v<n>.",
    "A value is one token: a word, a number, true, false, null, n/d, a \"quoted string\" or a [list of words].",
    "An object param is written below the command, two spaces in, one member a line: key value (nested members go in two more).",
    "A command that takes a file ends with: file <path>.",
    "A line starting with # is a comment; blank lines are ignored; the header ursatz-text 1 commands is allowed first.",
    "Params in [brackets] are optional; one|two lists the words an enum takes.",
};

static const char *type_text(const cJSON *param, CommonText *scratch) {
    const char *type = cJSON_GetObjectItemCaseSensitive(param, "type")->valuestring;
    if (strcmp(type, "string") == 0) {
        return "string";
    }
    if (strcmp(type, "integer") == 0) {
        return "int";
    }
    if (strcmp(type, "boolean") == 0) {
        return "bool";
    }
    if (strcmp(type, "rational") == 0) {
        return "n/d";
    }
    if (strcmp(type, "object") == 0) {
        return "object";
    }
    if (strcmp(type, "array") == 0) {
        return "list";
    }
    common_text_free(scratch);
    common_text_init(scratch);
    const cJSON *word;
    cJSON_ArrayForEach(word, cJSON_GetObjectItemCaseSensitive(param, "values")) {
        common_text_appendf(scratch, "%s%s", word->prev ? "|" : "", word->valuestring);
    }
    return scratch->data ? scratch->data : "word";
}

static bool flag(const cJSON *object, const char *name) {
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object, name));
}

/* "item:<id>@v<n>" for a command with a target (the version when it changes an item that has versions), "" for none. */
static void target_text(const cJSON *command, CommonText *out) {
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(command, "target");
    if (!cJSON_IsString(target)) {
        return;
    }
    common_text_appendf(out, " %s:<id>%s", target->valuestring, flag(command, "base") ? "@v<n>" : "");
}

/* "<param> <type>" for each required param and "[<param> <type>]" for each optional one, and the file of a command that takes one. */
static void params_text(const cJSON *command, CommonText *out) {
    const cJSON *param;
    CommonText scratch;
    common_text_init(&scratch);
    for (int pass = 0; pass < 2; pass++) {
        cJSON_ArrayForEach(param, cJSON_GetObjectItemCaseSensitive(command, "params")) {
            if (flag(param, "required") != (pass == 0)) {
                continue;
            }
            const char *name = cJSON_GetObjectItemCaseSensitive(param, "name")->valuestring;
            common_text_appendf(out, pass == 0 ? " %s %s" : " [%s %s]", name, type_text(param, &scratch));
        }
    }
    common_text_free(&scratch);
    if (flag(command, "bytes")) {
        common_text_append(out, " file <path>");
    }
}

static const char *command_name(const cJSON *command) {
    return cJSON_GetObjectItemCaseSensitive(command, "name")->valuestring;
}

static const char *command_summary(const cJSON *command) {
    return cJSON_GetObjectItemCaseSensitive(command, "summary")->valuestring;
}

static void capitalize_into(CommonText *out, const char *family) {
    char first[2] = {family[0] >= 'a' && family[0] <= 'z' ? (char)(family[0] - 32) : family[0], '\0'};
    common_text_appendf(out, "%s%s", first, family + 1);
}

char *workshop_command_sheet_text(void) {
    CommonText out;
    common_text_init(&out);
    common_text_append(&out, "Commands in Ursatz Text\n");
    for (size_t i = 0; i < sizeof kRules / sizeof kRules[0]; i++) {
        common_text_appendf(&out, "%s\n", kRules[i]);
    }
    for (size_t f = 0; f < workshop_command_family_count(); f++) {
        const cJSON *family = workshop_command_family_at(f);
        common_text_append(&out, "\n");
        capitalize_into(&out, cJSON_GetObjectItemCaseSensitive(family, "family")->valuestring);
        common_text_appendf(&out, ": %s\n", cJSON_GetObjectItemCaseSensitive(family, "summary")->valuestring);
        const cJSON *command;
        cJSON_ArrayForEach(command, cJSON_GetObjectItemCaseSensitive(family, "commands")) {
            common_text_appendf(&out, "%s", command_name(command));
            target_text(command, &out);
            params_text(command, &out);
            common_text_appendf(&out, "  %s\n", command_summary(command));
        }
    }
    if (out.failed) {
        common_text_free(&out);
        return NULL;
    }
    return common_text_take(&out);
}

char *workshop_command_sheet_markdown(void) {
    CommonText out;
    common_text_init(&out);
    common_text_append(&out, "# Command reference\n\n");
    common_text_append(&out, "Every command of the workshop, as Ursatz Text and as the call `ursatz apply` and the GUI send. Generated from `workshop/commands/*.json` "
                             "(tool `commands`, `ursatz help commands --format markdown`): do not edit. [Docs index](README.md).\n\n");
    common_text_append(&out, "## The text form\n\n");
    for (size_t i = 0; i < sizeof kRules / sizeof kRules[0]; i++) {
        common_text_appendf(&out, "- %s\n", kRules[i]);
    }
    for (size_t f = 0; f < workshop_command_family_count(); f++) {
        const cJSON *family = workshop_command_family_at(f);
        common_text_append(&out, "\n## ");
        capitalize_into(&out, cJSON_GetObjectItemCaseSensitive(family, "family")->valuestring);
        common_text_appendf(&out, "\n\n%s\n\n| Command | Target | Params | Does |\n|---|---|---|---|\n", cJSON_GetObjectItemCaseSensitive(family, "summary")->valuestring);
        const cJSON *command;
        cJSON_ArrayForEach(command, cJSON_GetObjectItemCaseSensitive(family, "commands")) {
            CommonText target, params;
            common_text_init(&target);
            common_text_init(&params);
            target_text(command, &target);
            params_text(command, &params);
            common_text_appendf(&out, "| `%s` | %s | %s | %s |\n", command_name(command), target.data && target.data[0] ? target.data + 1 : "none",
                                params.data && params.data[0] ? params.data + 1 : "none", command_summary(command));
            common_text_free(&target);
            common_text_free(&params);
        }
    }
    if (out.failed) {
        common_text_free(&out);
        return NULL;
    }
    return common_text_take(&out);
}

/* A production name: the command name with its dot as a hyphen. */
static void production_name(const char *name, CommonText *out) {
    for (const char *p = name; *p; p++) {
        char c[2] = {*p == '.' ? '-' : *p, '\0'};
        common_text_append(out, c);
    }
}

static const char *value_production(const cJSON *param) {
    const char *type = cJSON_GetObjectItemCaseSensitive(param, "type")->valuestring;
    if (strcmp(type, "string") == 0) {
        return "string";
    }
    if (strcmp(type, "integer") == 0) {
        return "integer";
    }
    if (strcmp(type, "boolean") == 0) {
        return "boolean";
    }
    if (strcmp(type, "rational") == 0) {
        return "rational";
    }
    if (strcmp(type, "object") == 0) {
        return "object";
    }
    if (strcmp(type, "array") == 0) {
        return "list";
    }
    return "word";
}

char *workshop_command_sheet_grammar(void) {
    CommonText out;
    common_text_init(&out);
    common_text_append(&out, "(* Commands in Ursatz Text, grammar 1. Generated from workshop/commands/*.json: do not edit. *)\n");
    common_text_append(&out,
                       "document   = [ header ] , { line } ;\n"
                       "header     = \"ursatz-text 1 commands\" , newline ;\n"
                       "line       = comment | blank | command ;\n"
                       "comment    = \"#\" , { any character } , newline ;\n"
                       "command    = command-name , [ target ] , { param } , [ file ] , newline , { object-member } ;\n"
                       "target     = kind , \":\" , id , [ \"@v\" , integer ] ;\n"
                       "kind       = \"item\" | \"piece\" | \"collection\" | \"style\" | \"links\" | \"project\" | \"subject\" | \"job\" ;\n"
                       "param      = name , value ;\n"
                       "file       = \"file\" , path ;\n"
                       "object-member = indent , name , value , newline , { indent , indent , object-member } ;  (* an object param: its members below the command *)\n"
                       "value      = word | integer | rational | boolean | \"null\" | string | list ;\n"
                       "word       = letter , { letter | digit | \"_\" | \".\" | \":\" | \"#\" | \"@\" | \"'\" | \"+\" | \"/\" | \"-\" } ;\n"
                       "rational   = [ \"-\" ] , digits , \"/\" , digits ;\n"
                       "list       = \"[\" , { value } , \"]\" ;\n"
                       "indent     = \"  \" ;\n\n"
                       "(* One production a command: the params it takes, in any order; (* required *) names the ones it must have. *)\n");
    for (size_t f = 0; f < workshop_command_family_count(); f++) {
        const cJSON *command;
        cJSON_ArrayForEach(command, cJSON_GetObjectItemCaseSensitive(workshop_command_family_at(f), "commands")) {
            CommonText line;
            common_text_init(&line);
            production_name(command_name(command), &line);
            common_text_appendf(&out, "%s = \"%s\"", line.data ? line.data : "", command_name(command));
            common_text_free(&line);
            const cJSON *target = cJSON_GetObjectItemCaseSensitive(command, "target");
            if (cJSON_IsString(target)) {
                common_text_appendf(&out, " , \"%s:\" , id%s", target->valuestring, flag(command, "base") ? " , \"@v\" , integer" : "");
            }
            const cJSON *params = cJSON_GetObjectItemCaseSensitive(command, "params");
            if (cJSON_GetArraySize(params) > 0) {
                common_text_append(&out, " , { ");
                const cJSON *param;
                cJSON_ArrayForEach(param, params) {
                    common_text_appendf(&out, "%s\"%s\" , %s", param->prev ? " | " : "", cJSON_GetObjectItemCaseSensitive(param, "name")->valuestring,
                                        value_production(param));
                }
                common_text_append(&out, " }");
            }
            if (flag(command, "bytes")) {
                common_text_append(&out, " , \"file\" , path");
            }
            common_text_append(&out, " ;");
            CommonText required;
            common_text_init(&required);
            const cJSON *param;
            cJSON_ArrayForEach(param, params) {
                if (flag(param, "required")) {
                    common_text_appendf(&required, "%s%s", required.length ? ", " : "", cJSON_GetObjectItemCaseSensitive(param, "name")->valuestring);
                }
            }
            if (required.length) {
                common_text_appendf(&out, "  (* required: %s *)", required.data);
            }
            common_text_free(&required);
            common_text_append(&out, "\n");
        }
    }
    if (out.failed) {
        common_text_free(&out);
        return NULL;
    }
    return common_text_take(&out);
}

char *workshop_command_sheet_json(void) {
    cJSON *all = cJSON_CreateArray();
    for (size_t f = 0; all && f < workshop_command_family_count(); f++) {
        const cJSON *command;
        cJSON_ArrayForEach(command, cJSON_GetObjectItemCaseSensitive(workshop_command_family_at(f), "commands")) {
            cJSON *copy = cJSON_Duplicate(command, true);
            if (!copy) {
                cJSON_Delete(all);
                return NULL;
            }
            cJSON_AddItemToArray(all, copy);
        }
    }
    char *text = all ? cJSON_Print(all) : NULL;
    cJSON_Delete(all);
    return text;
}

char *workshop_command_sheet_help(const cJSON *command) {
    CommonText out;
    common_text_init(&out);
    common_text_appendf(&out, "%s", command_name(command));
    target_text(command, &out);
    params_text(command, &out);
    common_text_appendf(&out, "\n\n%s.\n", command_summary(command));
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(command, "params");
    common_text_append(&out, "\nParams:\n");
    if (cJSON_GetArraySize(params) == 0) {
        common_text_append(&out, "  none\n");
    }
    const cJSON *param;
    CommonText scratch;
    common_text_init(&scratch);
    cJSON_ArrayForEach(param, params) {
        const cJSON *minimum = cJSON_GetObjectItemCaseSensitive(param, "minimum");
        const cJSON *maximum = cJSON_GetObjectItemCaseSensitive(param, "maximum");
        const cJSON *max_length = cJSON_GetObjectItemCaseSensitive(param, "max_length");
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(param, "items");
        common_text_appendf(&out, "  %s  %s%s", cJSON_GetObjectItemCaseSensitive(param, "name")->valuestring, type_text(param, &scratch),
                            flag(param, "required") ? ", required" : "");
        if (cJSON_IsString(items)) {
            common_text_appendf(&out, ", of %s", items->valuestring);
        }
        if (cJSON_IsNumber(minimum) && cJSON_IsNumber(maximum)) {
            common_text_appendf(&out, ", from %.0f to %.0f", minimum->valuedouble, maximum->valuedouble);
        } else if (cJSON_IsNumber(minimum)) {
            common_text_appendf(&out, ", %.0f or more", minimum->valuedouble);
        } else if (cJSON_IsNumber(maximum)) {
            common_text_appendf(&out, ", %.0f or less", maximum->valuedouble);
        }
        if (cJSON_IsNumber(max_length)) {
            common_text_appendf(&out, ", at most %.0f characters", max_length->valuedouble);
        }
        common_text_appendf(&out, "\n      %s\n", cJSON_GetObjectItemCaseSensitive(param, "help")->valuestring);
    }
    common_text_free(&scratch);
    const cJSON *group;
    cJSON_ArrayForEach(group, cJSON_GetObjectItemCaseSensitive(command, "one_of")) {
        common_text_append(&out, "  give exactly one of:");
        const cJSON *member;
        cJSON_ArrayForEach(member, group) {
            common_text_appendf(&out, "%s %s", member->prev ? "," : "", member->valuestring);
        }
        common_text_append(&out, "\n");
    }
    common_text_appendf(&out, "\nReply: %s\n", cJSON_GetObjectItemCaseSensitive(command, "reply")->valuestring);
    const cJSON *job = cJSON_GetObjectItemCaseSensitive(command, "job");
    if (cJSON_IsString(job)) {
        common_text_appendf(&out, "Job: the command answers with a %s job; wait for it with --wait.\n", job->valuestring);
    }
    if (flag(command, "base")) {
        common_text_append(&out, "Base: name the version you read (item:<id>@v<n>); the command is refused when the item has a newer one.\n");
    }
    if (!flag(command, "journaled")) {
        common_text_append(&out, "Journal: none (a runtime control).\n");
    }
    common_text_appendf(&out, "\nExample:\n");
    const char *example = cJSON_GetObjectItemCaseSensitive(command, "example")->valuestring;
    for (const char *line = example; *line;) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        common_text_appendf(&out, "  %.*s\n", (int)length, line);
        line += length + (end ? 1 : 0);
    }
    if (out.failed) {
        common_text_free(&out);
        return NULL;
    }
    return common_text_take(&out);
}
