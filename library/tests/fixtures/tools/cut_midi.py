#!/usr/bin/env python3
"""Cut a Standard MIDI File at a tick.

usage: cut_midi.py <in.mid> <out.mid> <cut-tick>

Events at or after <cut-tick> are dropped (notes starting there are gone).
A note held across the cut gets a note-off at the cut, a held sustain pedal
is released there, and every track ends with an end-of-track at the cut.
The cut must fall on a bar line: every time signature at or before it must be
4/4 (checked), and <cut-tick> must be a multiple of one bar.

Used for tests/fixtures/framework-v1-corpus/Mozart 545.mid:
  python3 tests/fixtures/tools/cut_midi.py "Mozart 545.mid" "Mozart 545.mid" 140160
(140160 = 146 bars * 4 beats * 240 ticks, the start of measure 147.)
Run it on the uncut original (git history of the file).
"""
import struct
import sys


def vlq_read(d, p):
    v = 0
    while True:
        b = d[p]
        p += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, p


def vlq_write(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    return bytes(reversed(out))


def parse_track(d, p, end):
    """Return a list of (tick, bytes) with absolute ticks; bytes is the raw event."""
    events = []
    tick = 0
    running = None
    while p < end:
        dt, p = vlq_read(d, p)
        tick += dt
        b = d[p]
        if b == 0xFF:
            ty = d[p + 1]
            n, q = vlq_read(d, p + 2)
            events.append((tick, d[p:q + n]))
            p = q + n
            if ty == 0x2F:
                break
        elif b in (0xF0, 0xF7):
            n, q = vlq_read(d, p + 1)
            events.append((tick, d[p:q + n]))
            p = q + n
        else:
            if b & 0x80:
                running = b
                p += 1
            size = 1 if (running >> 4) in (0xC, 0xD) else 2
            events.append((tick, bytes([running]) + d[p:p + size]))
            p += size
    return events


def main():
    src, dst, cut = sys.argv[1], sys.argv[2], int(sys.argv[3])
    d = open(src, "rb").read()
    assert d[:4] == b"MThd"
    fmt, ntracks, division = struct.unpack(">HHH", d[8:14])
    assert division & 0x8000 == 0
    bar = division * 4
    assert cut % bar == 0, "cut is not on a 4/4 bar line"
    p = 14
    tracks = []
    for _ in range(ntracks):
        assert d[p:p + 4] == b"MTrk"
        n = struct.unpack(">I", d[p + 4:p + 8])[0]
        tracks.append(parse_track(d, p + 8, p + 8 + n))
        p += 8 + n

    for ev in (e for t in tracks for e in t):
        tick, raw = ev
        if tick <= cut and raw[0] == 0xFF and raw[1] == 0x58:
            assert raw[3:5] == bytes([4, 2]), "time signature before the cut is not 4/4"

    out = bytearray(d[:14])
    for events in tracks:
        kept = [e for e in events if e[0] < cut and not (e[1][0] == 0xFF and e[1][1] == 0x2F)]
        held = {}
        pedal = {}
        for tick, raw in kept:
            s = raw[0]
            if s & 0xF0 == 0x90 and raw[2] > 0:
                held[(s & 0x0F, raw[1])] = True
            elif s & 0xF0 == 0x80 or (s & 0xF0 == 0x90 and raw[2] == 0):
                held.pop((s & 0x0F, raw[1]), None)
            elif s & 0xF0 == 0xB0 and raw[1] == 64:
                pedal[s & 0x0F] = raw[2] >= 64
        for (ch, note) in sorted(held):
            kept.append((cut, bytes([0x80 | ch, note, 0])))
        for ch in sorted(pedal):
            if pedal[ch]:
                kept.append((cut, bytes([0xB0 | ch, 64, 0])))
        kept.append((cut, bytes([0xFF, 0x2F, 0x00])))
        body = bytearray()
        last = 0
        for tick, raw in kept:
            body += vlq_write(tick - last) + raw
            last = tick
        out += b"MTrk" + struct.pack(">I", len(body)) + body
    open(dst, "wb").write(out)


if __name__ == "__main__":
    main()
