#ifndef URSATZ_GUI_ROUTE_COMPOSER_H
#define URSATZ_GUI_ROUTE_COMPOSER_H

struct mg_context;

/* Registers the Composer tool's routes (everything under "/api/composer")
 * against ctx, following the tool-registration convention described in
 * ursatz_gui/tool.h. */
void ursatz_gui_composer_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_COMPOSER_H */
