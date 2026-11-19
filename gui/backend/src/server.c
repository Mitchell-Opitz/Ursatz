#include "server.h"

#include <civetweb.h>
#include <stdio.h>
#include <unistd.h>
#include <unistd.h>

#include "routes/analyzer.h"
#include "routes/composer.h"
#include "routes/corpus.h"
#include "routes/jobs.h"
#include "routes/links.h"
#include "routes/materials.h"
#include "routes/styles.h"
#include "workshop_internal.h"

/* Every store path below is relative to the data folder, which is the backend's working directory once the server starts. */
static Workshop *g_workshop = NULL;
static const char *kCollectionsDir = "collections";
static const char *kMaterialsDir = "materials";
static const char *kSubjectsDir = "subjects";
static const char *kLinksDir = "links";

Workshop *ursatz_gui_workshop(void) {
  return g_workshop;
}

struct mg_context *ursatz_gui_server_start(Workshop *workshop, const char *listen_port, const char *frontend_dir) {
  char threads[16];
  snprintf(threads, sizeof(threads), "%d", workshop_http_threads(workshop));
  const char *options[] = {
      "listening_ports", listen_port,
      "document_root", frontend_dir,
      "num_threads", threads,
      NULL,
  };

  /* A data folder the one-time migrations of the GUI before the overhaul never ran on is refused (service/data_check.h),
   * before any store is opened. */
  char data_message[2048];
  const char *data_dir = workshop_data_dir(workshop);
  if (!workshop_data_check(data_dir, data_message, sizeof(data_message))) {
    fprintf(stderr, "ursatz-gui-backend: %s\n", data_message);
    return NULL;
  }
  if (chdir(data_dir) != 0) {
    fprintf(stderr, "ursatz-gui-backend: cannot enter the data folder %s\n", data_dir);
    return NULL;
  }

  /* Collections are files under collections/ (see
   * service/collection_service.h); the store is prepared once here. */
  if (!ursatz_gui_collection_service_init(kCollectionsDir)) {
    return NULL;
  }

  /* Materials and subjects are files too (service/material_service.h,
   * service/subject_service.h), prepared beside collections. */
  if (!workshop_material_service_init(kMaterialsDir)) {
    workshop_collection_service_shutdown();
    return NULL;
  }
  if (!workshop_subject_service_init(kSubjectsDir)) {
    workshop_material_service_shutdown();
    workshop_collection_service_shutdown();
    return NULL;
  }

  /* Motive link runs and their cache are files too (service/link_service.h). */
  if (!workshop_link_service_init(kLinksDir)) {
    workshop_subject_service_shutdown();
    workshop_material_service_shutdown();
    workshop_collection_service_shutdown();
    return NULL;
  }

  /* Long Composer runs go to worker threads (service/job_queue.h); the workers are up before any route. */
  if (!workshop_jobs_start((size_t)workshop_job_workers(workshop))) {
    workshop_link_service_shutdown();
    workshop_subject_service_shutdown();
    workshop_material_service_shutdown();
    workshop_collection_service_shutdown();
    return NULL;
  }

  struct mg_context *ctx = mg_start(NULL, NULL, options);
  if (!ctx) {
    workshop_jobs_stop();
    workshop_link_service_shutdown();
    workshop_subject_service_shutdown();
    workshop_material_service_shutdown();
    workshop_collection_service_shutdown();
    return NULL;
  }

  /* Route registration: each tool module registers itself here. This is
   * the only place that wires tool routes into the running server. */
  ursatz_gui_analyzer_register(ctx);
  ursatz_gui_corpus_register(ctx);
  ursatz_gui_composer_register(ctx);
  ursatz_gui_styles_register(ctx);
  ursatz_gui_materials_register(ctx);
  ursatz_gui_links_register(ctx);
  ursatz_gui_jobs_register(ctx);

  g_workshop = workshop;
  return ctx;
}

void ursatz_gui_server_stop(struct mg_context *ctx) {
  if (ctx) {
    mg_stop(ctx);
  }
  workshop_jobs_stop(); /* cancels what runs and joins the workers, after the last request is answered */
  workshop_link_service_shutdown();
  workshop_subject_service_shutdown();
  workshop_material_service_shutdown();
  workshop_collection_service_shutdown();
  g_workshop = NULL;
}
