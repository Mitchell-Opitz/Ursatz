#include "hemiola_rules.h"

#include "meter_class_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_HEMIOLA_RULE_COUNT] = {"hemiola-meter", "hemiola-duration", "hemiola-chord",
                                                                      "hemiola-accent", "hemiola-evidence"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_HEMIOLA_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_meter_hemiola, StandardMeterHemiolaRule, STANDARD_METER_HEMIOLA_RULE_COUNT, g_table)

bool standard_meter_hemiola_window(const TimeSignature *time_signature, Duration **out_pulse, unsigned *out_measures) {
    if (time_signature == NULL || out_pulse == NULL || out_measures == NULL) {
        return false;
    }
    unsigned top = time_signature_top_number(time_signature);
    unsigned measures = 0;
    if (top == 3 && standard_meter_type(time_signature) == METER_TYPE_SIMPLE) {
        measures = 2; /* simple triple: the beat is the pulse */
    } else if (top == 6 && standard_meter_type(time_signature) == METER_TYPE_COMPOUND) {
        measures = 1; /* compound duple: the eighth is the pulse, two groups of three */
    } else {
        return false;
    }
    Duration *pulse = NULL;
    if (time_signature_unit_duration(time_signature, &pulse) != DURATION_OK) {
        return false;
    }
    *out_pulse = pulse;
    *out_measures = measures;
    return true;
}

bool standard_meter_hemiola_duration_evidence(const bool attack[STANDARD_METER_HEMIOLA_PULSES]) {
    return attack != NULL && attack[2] && attack[4] && !attack[1] && !attack[3] && !attack[5];
}

bool standard_meter_hemiola_chord_evidence(const bool change[STANDARD_METER_HEMIOLA_PULSES]) {
    return change != NULL && change[2] && change[4] && !change[1] && !change[3] && !change[5];
}

bool standard_meter_hemiola_accent_evidence(const bool long_onset[STANDARD_METER_HEMIOLA_PULSES],
                                            const bool attack[STANDARD_METER_HEMIOLA_PULSES]) {
    return long_onset != NULL && attack != NULL && long_onset[2] && long_onset[4] && !attack[3];
}

bool standard_meter_hemiola_evidence_met(unsigned agreeing, unsigned available, unsigned minimum) {
    if (minimum == 0) {
        minimum = 1;
    }
    return available >= minimum && agreeing >= minimum && agreeing <= available;
}
