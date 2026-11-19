#ifndef URSATZ_GUI_ROUTE_SUBJECTS_H
#define URSATZ_GUI_ROUTE_SUBJECTS_H

struct mg_context;

/* Registers the subject routes ("/api/subjects/...") against ctx; they dispatch into the workshop's subject service. */
void ursatz_gui_subjects_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_SUBJECTS_H */
