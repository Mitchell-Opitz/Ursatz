#ifndef WORKSHOP_SETTINGS_RESOLVE_H
#define WORKSHOP_SETTINGS_RESOLVE_H

/*
 * The settings resolver (R-671, R-673): the value of every setting for a request, from the scopes in this order, each over the one
 * before it: the defaults, the style, the taste profile "all", the named taste profile, the project, the item and the request. A scope
 * contributes its values and then its overrides (a rule waived, reversed, reweighted or switched off). A scope an off list names is
 * skipped. Every value is answered with the scope it came from.
 */

#include <stdbool.h>

#include <cJSON.h>

#include <common/status.h>

typedef struct {
    const char *project_id;
    const char *item_id;
    const char *style_id;
    const char *taste;      /* the request's taste profile, NULL to read the item's, the project's, the style's, else "none" */
    const cJSON *values;    /* the request scope: a nested preferences document, which may hold the explore group; NULL for none */
    const cJSON *overrides; /* the request scope's overrides: [{rule, action, weight}]; NULL for none */
    const cJSON *off;       /* the request's switches: ["style", "taste", "project"]; NULL for none */
    const char *kind;       /* "theme" (default) or "motif": which groups an exploration may move */
} WorkshopSettingsRequest;

/*
 * Resolves request and answers {taste, off, values: [{key, value, from}], overrides: [{rule, action, weight, from}], document} in *out
 * (the caller's). values lists the settings whose source is not "default", or every setting with `all`; document holds them nested by
 * their dotted keys (the explore group left out), or null when there are none. The project of an item is the item's own when
 * project_id is NULL. NOT_FOUND for a style, project or item that is not there; REQUEST for a request member that is not valid.
 */
UrsatzStatus workshop_settings_resolve_request(const WorkshopSettingsRequest *request, bool all, cJSON **out, UrsatzError *error);

/*
 * Checks one override of a project, item or request (R-676): the rule is one of the rule table's, the action off, reverse, reweight or waive,
 * a reweight has a weight from 0 to 20 and a reversal names a soft group. REQUEST at where (the path of the override), naming the rules for
 * an unknown one.
 */
UrsatzStatus workshop_settings_override_check(const char *rule, const char *action, const cJSON *weight, const char *where, UrsatzError *error);

#endif
