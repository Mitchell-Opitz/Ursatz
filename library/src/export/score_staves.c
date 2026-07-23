#include "export/score_staves.h"

bool score_program_uses_grand_staff(int program) {
    return (program >= 0 && program <= 8) || (program >= 16 && program <= 21) || program == 46;
}
