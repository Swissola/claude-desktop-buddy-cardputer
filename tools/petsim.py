#!/usr/bin/env python3
"""Host-side simulator for the M5StickC buddy ASCII-pet renderer.

Reproduces buddyPrintSprite()'s block-centring so pet body alignment can be
checked across all species / frames / scales WITHOUT flashing hardware — the
fix for the "middle section misaligned" bug is a pure-arithmetic change, so we
can validate it here first.

It renders each 5-row frame as a CHARACTER-COLUMN grid: each row is placed at the
x-column the firmware would compute (pixel x / BUDDY_CHAR_W), so misalignment
shows up as rows literally not lining up. Two modes:
  OLD = the original per-row trim+recentre (only trims at scale 2) — the BUG
  NEW = block centred on widest row, padding preserved — the FIX

Usage:
  python petsim.py <species.cpp> [--scale 1|2] [--mode old|new|both] [--state sleep|idle|...]
  python petsim.py --all            # scan every species, flag frames that differ old-vs-new
"""
import re, sys, argparse, glob, os

# Renderer constants (from buddy.cpp, confirmed by survey)
BUDDY_X_CENTER = 67
BUDDY_CHAR_W   = 6
BUDDY_CHAR_H   = 8
BUDDY_Y_BASE   = 30

def col_old(line, scale, x_off=0):
    """OLD buddyPrintLine x, in CHARACTER columns. Per-row: at scale>1 it strips
    leading+trailing spaces and re-centres by trimmed width; at scale 1 keeps
    padding. Returns (left_col, text_to_draw)."""
    s = line
    if scale > 1:
        s = s.strip()
    w_px = len(s) * BUDDY_CHAR_W * scale
    x_px = BUDDY_X_CENTER - w_px // 2 + x_off * scale
    return (x_px / (BUDDY_CHAR_W * scale), s)  # left col in char-units of this scale

def col_new(lines, scale, x_off=0):
    """NEW buddyPrintSprite: one left edge for the whole block, centred on the
    widest row, padding preserved. Returns left_col (char-units) shared by all rows."""
    max_len = max((len(l) for l in lines), default=0)
    block_w = max_len * BUDDY_CHAR_W * scale
    x_px = BUDDY_X_CENTER - block_w // 2 + x_off * scale
    return x_px / (BUDDY_CHAR_W * scale)

def render(lines, scale, mode):
    """Return list of (col_float, text) per row for the given mode."""
    out = []
    if mode == 'new':
        left = col_new(lines, scale)
        for l in lines:
            out.append((left, l))   # padding preserved
    else:  # old
        for l in lines:
            out.append(col_old(l, scale))
    return out

def show(title, rows):
    print(f"  {title}")
    # find min col to left-justify the display without losing relative offset
    base = min((c for c, _ in rows), default=0)
    for c, text in rows:
        pad = int(round(c - base))
        print("    |" + " " * max(0, pad) + text)
    print()

# --- parse a species .cpp: pull out 5-row const char* arrays -----------------
ARR_RE = re.compile(
    r'static\s+const\s+char\s*\*\s*const\s+(\w+)\s*\[\s*\d*\s*\]\s*=\s*\{(.*?)\}\s*;',
    re.DOTALL)
STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')

def parse_frames(path):
    src = open(path, encoding='utf-8').read()
    frames = {}
    for m in ARR_RE.finditer(src):
        name, body = m.group(1), m.group(2)
        rows = [s.encode().decode('unicode_escape') for s in STR_RE.findall(body)]
        if len(rows) == 5:                # body sprite frames are 5 rows
            frames[name] = rows
    return frames

def frames_differ(rows, scale):
    """True if OLD and NEW place rows differently (i.e. the bug shows here)."""
    old = render(rows, scale, 'old')
    new = render(rows, scale, 'new')
    # compare each row's left col rounded to a char
    base_o = min(c for c, _ in old); base_n = min(c for c, _ in new)
    o = [round(c - base_o) for c, _ in old]
    n = [round(c - base_n) for c, _ in new]
    return o != n

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('species', nargs='?')
    ap.add_argument('--scale', type=int, default=2)
    ap.add_argument('--mode', default='both', choices=['old', 'new', 'both'])
    ap.add_argument('--all', action='store_true')
    a = ap.parse_args()

    if a.all:
        here = os.path.dirname(os.path.abspath(__file__))
        files = sorted(glob.glob(os.path.join(here, '..', 'src', 'buddies', '*.cpp')))
        print(f"Scanning {len(files)} species for frames where OLD != NEW at scale 2...\n")
        for f in files:
            frames = parse_frames(f)
            bad = [name for name, rows in frames.items() if frames_differ(rows, 2)]
            tag = f"  <-- {len(bad)}/{len(frames)} frames misalign (OLD)" if bad else "  ok"
            print(f"{os.path.basename(f):20} {len(frames):2} frames{tag}")
            if bad:
                print(f"      affected: {', '.join(bad)}")
        return

    if not a.species:
        ap.error("give a species .cpp or --all")
    frames = parse_frames(a.species)
    print(f"{a.species}: {len(frames)} frames @ scale {a.scale}\n")
    for name, rows in frames.items():
        print(f"=== {name} ===")
        if a.mode in ('old', 'both'):
            show("OLD (per-row recentre — buggy):", render(rows, a.scale, 'old'))
        if a.mode in ('new', 'both'):
            show("NEW (block centred — fix):", render(rows, a.scale, 'new'))

if __name__ == '__main__':
    main()
