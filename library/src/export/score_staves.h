#ifndef MUSIC_EXPORT_SCORE_STAVES_H
#define MUSIC_EXPORT_SCORE_STAVES_H

#include <stdbool.h>

/*
 * ScoreStaves (music-export): decides which instruments are written on a grand staff (treble and bass) and, for
 * those, which staff each note belongs to. Pure: nothing imported or analyzed is changed, the staff numbers are a
 * separate result that the caller passes on as MusicXmlExportScore.staff.
 */

/*
 * True when a General MIDI program (0-based) is written on two staves: the pianos, harpsichord and clavinet (0 to 7),
 * celesta (8), the organs and accordion (16 to 21) and harp (46). False for every other program and for -1 (none).
 */
bool score_program_uses_grand_staff(int program);

#endif
