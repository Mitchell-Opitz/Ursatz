#ifndef URSATZ_GUI_ROUTE_EVENTS_ROUTES_H
#define URSATZ_GUI_ROUTE_EVENTS_ROUTES_H

#include "route_table.h"

/* The rows of the event stream route (route_table.h). */
const UrsatzGuiRoute *ursatz_gui_events_routes(size_t *count);

/* Ends every stream that is open and refuses new ones: called before the server stops, so the request threads that wait for events come back. */
void ursatz_gui_events_stop(void);

/* Lets the streams start again (the server starts). */
void ursatz_gui_events_start(void);

#endif
