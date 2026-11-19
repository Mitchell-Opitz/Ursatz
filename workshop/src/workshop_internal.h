#ifndef WORKSHOP_WORKSHOP_INTERNAL_H
#define WORKSHOP_WORKSHOP_INTERNAL_H

/*
 * The services the GUI routes call, in one header (P63 B5). The routes include this and their own headers and nothing else of the
 * workshop; B11 gives the GUI the query and command API and deletes this file.
 */

#include <ursatz_workshop/workshop_api.h>

#include "base/http_status.h"
#include "base/knowledge.h"
#include "base/last_error.h"

#include "items/collections.h"
#include "items/data_check.h"
#include "items/ids.h"
#include "items/library_metadata.h"
#include "items/lock.h"
#include "items/store.h"
#include "items/piece_lock.h"
#include "items/subjects.h"

#include "analysis/analysis_service.h"
#include "analysis/import_cache.h"
#include "analysis/links.h"
#include "analysis/styles.h"

#include "composition/compose_common.h"
#include "composition/compose_jobs.h"
#include "composition/composer_client.h"
#include "composition/motifs.h"
#include "composition/pieces.h"
#include "composition/themes.h"

#include "rendering/export.h"
#include "rendering/playback.h"

#include "jobs/job_queue.h"

#endif
