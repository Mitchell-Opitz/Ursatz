#include "server.h"

#include <civetweb.h>

#include "routes/analyzer.h"
#include "routes/composer.h"
#include "routes/corpus.h"
#include "routes/health.h"
#include "routes/styles.h"
#include "service/collection_service.h"

static const char *kCollectionsDir = "collections";
static const char *kLegacyCorpusDbPath = "data/corpus.db";

struct mg_context *ursatz_gui_server_start(const char *listen_port,
                                            const char *frontend_dir) {
  const char *options[] = {
      "listening_ports", listen_port,
      "document_root", frontend_dir,
      "num_threads", "4",
      NULL,
  };

  /* Collections are files under collections/ (see
   * service/collection_service.h); the store is prepared once here. */
  if (!ursatz_gui_collection_service_init(kCollectionsDir, kLegacyCorpusDbPath)) {
    return NULL;
  }

  struct mg_context *ctx = mg_start(NULL, NULL, options);
  if (!ctx) {
    ursatz_gui_collection_service_shutdown();
    return NULL;
  }

  /* Route registration: each tool module registers itself here. This is
   * the only place that wires tool routes into the running server. */
  ursatz_gui_health_register(ctx);
  ursatz_gui_analyzer_register(ctx);
  ursatz_gui_corpus_register(ctx);
  ursatz_gui_composer_register(ctx);
  ursatz_gui_styles_register(ctx);

  return ctx;
}

void ursatz_gui_server_stop(struct mg_context *ctx) {
  if (ctx) {
    mg_stop(ctx);
  }
  ursatz_gui_collection_service_shutdown();
}
