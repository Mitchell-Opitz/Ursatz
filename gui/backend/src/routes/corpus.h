#ifndef URSATZ_GUI_ROUTE_CORPUS_H
#define URSATZ_GUI_ROUTE_CORPUS_H

struct mg_context;

/* Registers the Corpus tool's routes ("/api/corpus", "/api/corpus/<id>/pieces",
 * "/api/corpus/<id>/pieces/<piece_id>") against ctx, following the
 * tool-registration convention described in ursatz_gui/tool.h.
 *
 * The corpus store itself must already be open (see
 * ursatz_gui_corpus_service_init in service/corpus_service.h) before this
 * is called -- these routes only dispatch into that service, they do not
 * own the store's lifecycle. */
void ursatz_gui_corpus_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_CORPUS_H */
