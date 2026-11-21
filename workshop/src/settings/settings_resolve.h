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
    bool explore;           /* move settings away from the resolved ones (R-673): explore_seed and explore_distance say how */
    unsigned long long explore_seed;
    int explore_distance;   /* 1 to 8, 0 for the resolved explore.distance */
} WorkshopSettingsRequest;

/*
 * Resolves request and answers {taste, off, values: [{key, value, from}], overrides: [{rule, action, weight, from}], document} in *out
 * (the caller's). values lists the settings whose source is not "default", or every setting with `all`; document holds them nested by
 * their dotted keys (the explore group left out), or null when there are none. The project of an item is the item's own when
 * project_id is NULL. With request->explore the answer also holds explore: {seed, distance, moves: [{key, from, to}]}: the soft settings of the
 * groups the kind reads (theme: melody, motif and development; motif: motif) that are booleans or integers from 0 to 20, sorted by key; a xorshift64
 * generator seeded with the seed (0 is 1) picks `distance` distinct ones, and each moves from its resolved value (a boolean flips, an integer of 3 or
 * more goes to 0, a smaller one to the lesser of its value plus 3 and 20). A moved setting comes from "explore". NOT_FOUND for a style, project or item that is not there; REQUEST for a request member that is not valid.
 */
UrsatzStatus workshop_settings_resolve_request(const WorkshopSettingsRequest *request, bool all, cJSON **out, UrsatzError *error);

/*
 * Checks one override of a project, item or request (R-676): the rule is one of the rule table's, the action off, reverse, reweight or waive,
 * a reweight has a weight from 0 to 20 and a reversal names a soft group. REQUEST at where (the path of the override), naming the rules for
 * an unknown one.
 */
UrsatzStatus workshop_settings_override_check(const char *rule, const char *action, const cJSON *weight, const char *where, UrsatzError *error);

#endif
