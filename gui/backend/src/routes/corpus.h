#ifndef URSATZ_GUI_ROUTE_CORPUS_H
#define URSATZ_GUI_ROUTE_CORPUS_H

struct mg_context;

/* Registers the Collections tool's routes ("/api/corpus", "/api/corpus/<id>",
 * "/api/corpus/<id>/pieces", "/api/corpus/<id>/pieces/<piece_id>") against
 * ctx, following the tool-registration convention described in
 * ursatz_gui/tool.h. The paths keep the name "corpus" (R-098).
 *
 * The collection store must already be initialized (see
 * ursatz_gui_collection_service_init in service/collection_service.h)
 * before this is called -- these routes only dispatch into that service,
 * they do not own the store's lifecycle. */
void ursatz_gui_corpus_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_CORPUS_H */
