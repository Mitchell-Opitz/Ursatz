#ifndef URSATZ_GUI_ROUTE_MOTIFS_H
#define URSATZ_GUI_ROUTE_MOTIFS_H

#include <civetweb.h>

#include <common/status.h>
#include <cJSON.h>

/*
 * The motif routes under "/api/materials" that call the workshop's motif, variant and render services; materials.c dispatches to them
 * (R-179, R-275, R-276; A20261109-76). Each reads its request, calls one workshop function and answers.
 */

int ursatz_gui_motifs_import_motif(struct mg_connection *conn);
int ursatz_gui_motifs_render(struct mg_connection *conn);
int ursatz_gui_motifs_variant_operations(struct mg_connection *conn);
int ursatz_gui_motifs_variants(struct mg_connection *conn, const char *id);
int ursatz_gui_motifs_each_op(struct mg_connection *conn, const char *id);
int ursatz_gui_motifs_reroll(struct mg_connection *conn, const char *id);
int ursatz_gui_motifs_recognize(struct mg_connection *conn);
int ursatz_gui_motifs_variant_tree(struct mg_connection *conn, const char *id);
int ursatz_gui_motifs_op_checks_get(struct mg_connection *conn, const char *id);
int ursatz_gui_motifs_op_checks_put(struct mg_connection *conn, const char *id);
int ursatz_gui_motifs_random_motifs(struct mg_connection *conn);
int ursatz_gui_motifs_validate(struct mg_connection *conn);

/* A request body in, an item out: the workshop_variants_save, _flag, _promote and _link_form calls. */
typedef UrsatzStatus (*ItemCommand)(const char *id, const cJSON *body, cJSON **item, UrsatzError *error);
int ursatz_gui_motifs_item_command(struct mg_connection *conn, const char *id, ItemCommand command);

#endif /* URSATZ_GUI_ROUTE_MOTIFS_H */
