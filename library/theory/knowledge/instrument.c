#include "theory/knowledge/instrument.h"

bool instrument_in_range(const Instrument *instrument, int midi) {
    return instrument != NULL && midi >= instrument->lowest && midi <= instrument->highest;
}
