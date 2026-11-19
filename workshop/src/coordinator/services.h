#ifndef URSATZ_WORKSHOP_COORDINATOR_SERVICES_H
#define URSATZ_WORKSHOP_COORDINATOR_SERVICES_H

/*
 * The services the workshop's query and command handlers call, in one header (P63 B5, moved into the coordinator by B11). Nothing outside the
 * workshop includes it: the GUI reaches the workshop through workshop_api.h.
 */

#include <ursatz_workshop/workshop_api.h>

#include "base/http_status.h"
#include "base/knowledge.h"
#include "base/last_error.h"

#include "items/collections.h"
#include "items/data_check.h"
#include "items/ids.h"
#include "items/layers.h"
#include "items/layout.h"
#include "items/library_metadata.h"
#include "items/lock.h"
#include "items/store.h"
#include "items/piece_lock.h"
#include "items/subjects.h"

#include "analysis/analysis.h"
#include "analysis/analysis_jobs.h"
#include "analysis/import_cache.h"
#include "analysis/links.h"
#include "analysis/styles.h"

#include "composition/compose_common.h"
#include "composition/compose_jobs.h"
#include "composition/composer_client.h"
#include "composition/motif_import.h"
#include "composition/motifs.h"
#include "composition/pieces.h"
#include "composition/themes.h"
#include "composition/variants.h"

#include "rendering/export.h"
#include "rendering/playback.h"
#include "rendering/render_cache.h"

#include "jobs/job_queue.h"

#endif
