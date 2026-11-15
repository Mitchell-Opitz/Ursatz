#include "motive/motive_segments.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    size_t first;
    size_t last;
} Group;

static bool group_note_alone_in_bar(const MotiveAnalysis *analysis, size_t note) {
    const MotiveRhythmNote *rhythm = analysis->rhythm;
    return rhythm[note].beat.num == 0 && (note == 0 || rhythm[note - 1].bar != rhythm[note].bar) &&
           (note + 1 >= analysis->note_count || rhythm[note + 1].bar != rhythm[note].bar);
}

static void remove_group(Group *groups, size_t *count, size_t at) {
    memmove(&groups[at], &groups[at + 1], (*count - at - 1) * sizeof(groups[0]));
    (*count)--;
}

static bool boundary_before(const MotiveAnalysis *analysis, size_t i) {
    const MotiveRhythmNote *rhythm = analysis->rhythm;
    if (rhythm[i].rest_before.num > 0) {
        return true;
    }
    /* After a note at least a beat long and at least twice the duration of the note before it. */
    if (i >= 2 && rational_cmp(rhythm[i - 1].duration, analysis->beat_length) >= 0) {
        Rational twice = {rhythm[i - 2].duration.num * 2, rhythm[i - 2].duration.den};
        if (rational_cmp(rhythm[i - 1].duration, twice) >= 0) {
            return true;
        }
    }
    /* The first note of each full bar, when it is on beat 1; the pickup stays with the first group. */
    return rhythm[i].bar >= 1 && rhythm[i].bar != rhythm[i - 1].bar && rhythm[i].beat.num == 0;
}

static bool same_rhythm(const MotiveAnalysis *analysis, Group a, Group b) {
    if (a.last - a.first != b.last - b.first) {
        return false;
    }
    for (size_t k = 0; k <= a.last - a.first; k++) {
        if (rational_cmp(analysis->rhythm[a.first + k].duration, analysis->rhythm[b.first + k].duration) != 0) {
            return false;
        }
    }
    return true;
}

size_t motive_segments_propose(const MotiveAnalysis *analysis, MotiveSketchFeature *out, size_t max) {
    if (!analysis || !out || max == 0 || analysis->note_count == 0) {
        return 0;
    }
    Group groups[MOTIVE_MAX_NOTES];
    size_t count = 1;
    groups[0].first = 0;
    groups[0].last = 0;
    for (size_t i = 1; i < analysis->note_count; i++) {
        if (analysis->rhythm_known && boundary_before(analysis, i)) {
            groups[count].first = i;
            groups[count].last = i;
            count++;
        } else {
            groups[count - 1].last = i;
        }
    }

    /* A group of one note joins the group before it (the first group joins the next), unless that note is alone in its
       bar: a whole-bar note is a segment of its own. */
    for (size_t g = 1; g < count;) {
        bool single = groups[g].first == groups[g].last;
        if (single && !group_note_alone_in_bar(analysis, groups[g].first)) {
            groups[g - 1].last = groups[g].last;
            remove_group(groups, &count, g);
        } else {
            g++;
        }
    }
    if (count > 1 && groups[0].first == groups[0].last && !group_note_alone_in_bar(analysis, 0)) {
        groups[1].first = 0;
        remove_group(groups, &count, 0);
    }

    /* Over max: join the adjacent pair with the fewest notes, earliest first. */
    while (count > max) {
        size_t best = 0;
        size_t best_size = (size_t)-1;
        for (size_t g = 0; g + 1 < count; g++) {
            size_t size = groups[g + 1].last - groups[g].first + 1;
            if (size < best_size) {
                best_size = size;
                best = g;
            }
        }
        groups[best].last = groups[best + 1].last;
        remove_group(groups, &count, best + 1);
    }

    /* Letters by first appearance of a rhythm; a repeat takes the letter with one ' per earlier occurrence. */
    for (size_t g = 0; g < count; g++) {
        size_t earlier = (size_t)-1;
        size_t repeats = 0;
        for (size_t h = 0; h < g; h++) {
            if (same_rhythm(analysis, groups[h], groups[g])) {
                if (earlier == (size_t)-1) {
                    earlier = h;
                }
                repeats++;
            }
        }
        char letter = 'a';
        if (earlier == (size_t)-1) {
            /* The next letter not yet used. */
            size_t distinct = 0;
            for (size_t h = 0; h < g; h++) {
                bool first_of_kind = true;
                for (size_t k = 0; k < h; k++) {
                    if (same_rhythm(analysis, groups[k], groups[h])) {
                        first_of_kind = false;
                        break;
                    }
                }
                distinct += first_of_kind ? 1 : 0;
            }
            letter = (char)('a' + distinct);
        } else {
            letter = out[earlier].label[0];
        }
        size_t length = 0;
        out[g].label[length++] = letter;
        for (size_t k = 0; k < repeats && length + 1 < sizeof(out[g].label); k++) {
            out[g].label[length++] = '\'';
        }
        out[g].label[length] = '\0';
        out[g].first = groups[g].first;
        out[g].last = groups[g].last;
    }
    return count;
}
