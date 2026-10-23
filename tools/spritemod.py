#!/usr/bin/env python3
"""Sprite mods for Guardian Heroes: export frames as PNG, edit them, apply them back into the game files.

Workflow (run from the repo root; the game files are game/pkg/media/chara_ss.mrg and chara.mrg):

    python3 -I tools/spritemod.py export chara_ss 0         # -> mods/chara_ss/00/f000.png ... + info.txt
    (repaint any of the PNGs, keep the file names and the image size)
    python3 -I tools/spritemod.py apply  chara_ss 0         # builds the new .mrg, keeps chara_ss.mrg.orig
    python3 -I tools/spritemod.py restore chara_ss          # puts the original back

The game then loads the patched file; nothing in the program changes. Copy the .mrg to the console the same way as
the original. Game data and exported images never go into git (mods/ is ignored).

PNG rules
- Same width and height as the exported file. Only the files you changed (or all of them) are read.
- Indexed PNG (mode P): exported that way, with the character's real palette. If you keep the palette, indices are
  copied as they are and the costume palettes keep working. Index 0 is transparent.
- Any other PNG (RGBA): every pixel snaps to the nearest colour of palette 0, alpha < 128 is transparent. The game
  draws with 256 palette colours per character, so a true-colour repaint is reduced to them. Costume recolours only
  touch a few palette entries, so they work where your colour is exactly one of those entries and look off elsewhere.
"""
import os, shutil, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mrg
from sprites import Chara

MEDIA = os.environ.get('GH_MEDIA', 'game/pkg/media')
MODS = os.environ.get('GH_MODS', 'mods')


def paths(name):
    return os.path.join(MEDIA, name + '.mrg'), os.path.join(MODS, name)


def frame_png(c, f, pal=0):
    w, h = c.recs[f][:2]
    im = Image.fromarray(c.idx(f), 'P')
    p = c.pals[pal]
    im.putpalette(p[:, :3].astype(np.uint8).reshape(-1).tolist())
    im.info['transparency'] = 0
    return im


def cmd_export(name, entry):
    src, d = paths(name)
    d = os.path.join(d, '%02d' % entry)
    os.makedirs(d, exist_ok=True)
    c = Chara(mrg.Mrg(src).get(entry))
    with open(os.path.join(d, 'info.txt'), 'w') as fh:
        fh.write('frame width height pivot_x pivot_y\n')
        for f, (w, h, x, y, _) in enumerate(c.recs):
            fh.write('f%03d %d %d %d %d\n' % (f, w, h, x, y))
    n = 0
    for f, (w, h, *_r) in enumerate(c.recs):
        if not w or not h:
            continue
        frame_png(c, f).save(os.path.join(d, 'f%03d.png' % f), transparency=0)
        n += 1
    print('exported %d frames to %s' % (n, d))


def quantise(rgba, pal):
    """RGBA array -> palette indices (nearest colour of the opaque palette entries, alpha < 128 -> 0)."""
    cols = pal[:, :3].astype(np.int32)
    ok = (pal[:, 3] > 0)
    ok[0] = False
    ids = np.nonzero(ok)[0]
    flat = rgba[..., :3].reshape(-1, 3).astype(np.int32)
    out = np.zeros(len(flat), np.uint8)
    for s in range(0, len(flat), 65536):  # keep the distance matrix small
        d = ((flat[s:s + 65536, None, :] - cols[ids][None, :, :]) ** 2).sum(-1)
        out[s:s + 65536] = ids[d.argmin(1)]
    out = out.reshape(rgba.shape[:2])
    out[rgba[..., 3] < 128] = 0
    return out


def load_frame(path, c, f):
    w, h = c.recs[f][:2]
    im = Image.open(path)
    if im.size != (w, h):
        raise SystemExit('%s: size %dx%d, the game frame is %dx%d' % (path, im.width, im.height, w, h))
    pal0 = c.pals[0]
    if im.mode == 'P':
        pp = np.array(im.getpalette() or [], np.uint8)
        pp = np.pad(pp, (0, max(0, 768 - len(pp)))).reshape(-1, 3)[:256]
        used = np.unique(np.asarray(im))
        if all((pp[i] == pal0[i][:3]).all() for i in used if i < 256 and i != 0):
            a = np.asarray(im).copy()
            return a, 'indices'
    rgba = np.asarray(im.convert('RGBA'))
    return quantise(rgba, pal0), 'snapped'


def cmd_apply(name, entry):
    src, d = paths(name)
    d = os.path.join(d, '%02d' % entry)
    if not os.path.isdir(d):
        raise SystemExit('no folder %s (run export first)' % d)
    orig = src + '.orig'
    if not os.path.exists(orig):
        shutil.copy2(src, orig)
    M = mrg.Mrg(orig)  # always build from the untouched original
    c = Chara(M.get(entry))
    n = {'indices': 0, 'snapped': 0, 'same': 0}
    for f in range(len(c.recs)):
        p = os.path.join(d, 'f%03d.png' % f)
        if not os.path.exists(p):
            continue
        a, how = load_frame(p, c, f)
        if (a == c.idx(f)).all():
            n['same'] += 1
            continue
        c.set_idx(f, a)
        n[how] += 1
    ents = [M.raw(i) for i in range(M.n)]
    if n['indices'] + n['snapped']:
        ents[entry] = mrg.pack_entry(bytes(c.x))
    open(src, 'wb').write(mrg.build(ents))
    print('%s entry %d: %d frames from indexed PNG, %d snapped to the palette, %d unchanged'
          % (name, entry, n['indices'], n['snapped'], n['same']))


def cmd_restore(name):
    src, _ = paths(name)
    if os.path.exists(src + '.orig'):
        shutil.copy2(src + '.orig', src)
        print('restored', src)
    else:
        print('no backup, nothing to do')


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] in ('export', 'apply'):
        {'export': cmd_export, 'apply': cmd_apply}[a[0]](a[1], int(a[2]))
    elif len(a) == 2 and a[0] == 'restore':
        cmd_restore(a[1])
    else:
        sys.exit(__doc__)
