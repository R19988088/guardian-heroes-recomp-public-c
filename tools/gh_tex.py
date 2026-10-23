#!/usr/bin/env python3
"""Guardian Heroes texture helpers: Xenos 2D untiling, DXT1/3/5 and RGBA8 decode, PNG dump.

Texture entry in an .mrg: BE u32 format, width, height, data size, ... data at +0x1000 (0x1c header words
are zero after the first six). Format ids seen: 2 = k_8_8_8_8, 3 = DXT3/5-like 8 bit, 4 = DXT1, 5 = DXT5.
    python3 -I tools/gh_tex.py <file.mrg|file.mrgc> <out_dir>     dump every texture entry as PNG
"""
import os, struct, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mrg import Mrg


def tiled_offsets(pw, ph, lb):
    """Byte offsets of every texel (block) of a Xenos 2D tiled texture; pw/ph are aligned to 32."""
    y, x = np.mgrid[0:ph, 0:pw].astype(np.int64)
    macro = ((x >> 5) + (y >> 5) * (pw >> 5)) << (lb + 7)
    micro = ((x & 7) + ((y & 6) << 2)) << lb
    off = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 8) << (3 + lb)) + ((y & 1) << 4)
    return (((off & ~0x1FF) << 3) + ((off & 0x1C0) << 2) + (off & 0x3F) + ((y & 16) << 7)
            + ((((y & 8) >> 2) + (x >> 3)) & 3) * 64)


def untile(data, wt, ht, lb):
    """wt x ht texels (or blocks) of 2**lb bytes -> array [ht, wt, 2**lb] of bytes."""
    pw, ph = (wt + 31) & ~31, (ht + 31) & ~31
    off = tiled_offsets(pw, ph, lb)[:ht, :wt]
    need = int(off.max()) + (1 << lb)
    a = np.frombuffer(bytes(data[:need]).ljust(need, b'\0'), np.uint8)
    bs = 1 << lb
    return a[off[..., None] + np.arange(bs)]


def _c565(c):
    r = ((c >> 11) & 31) * 255 // 31
    g = ((c >> 5) & 63) * 255 // 63
    b = (c & 31) * 255 // 31
    return np.stack([r, g, b], -1)


def _color_block(b8):
    """b8: [..., 8] bytes (already 16-bit swapped) -> [..., 4, 4, 4] RGBA (DXT1 style, 4 colour mode unless c0<=c1)."""
    c0 = b8[..., 0].astype(np.int32) | (b8[..., 1].astype(np.int32) << 8)
    c1 = b8[..., 2].astype(np.int32) | (b8[..., 3].astype(np.int32) << 8)
    bits = (b8[..., 4].astype(np.uint32) | (b8[..., 5].astype(np.uint32) << 8)
            | (b8[..., 6].astype(np.uint32) << 16) | (b8[..., 7].astype(np.uint32) << 24))
    p0, p1 = _c565(c0), _c565(c1)
    four = (c0 > c1)[..., None]
    p2 = np.where(four, (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four, (p0 + 2 * p1) // 3, 0)
    pal = np.stack([p0, p1, p2, p3], -2)  # [..., 4, 3]
    alpha3 = np.where(four[..., 0], 255, 0)  # only index 3 is transparent in 3-colour mode (DXT1)
    shifts = (np.arange(16) * 2).astype(np.uint32)
    idx = ((bits[..., None] >> shifts) & 3).astype(np.int64)  # [..., 16]
    rgb = np.take_along_axis(pal, idx[..., None].repeat(3, -1), -2)
    a = np.where(idx == 3, alpha3[..., None], 255)
    out = np.concatenate([rgb, a[..., None]], -1).reshape(*b8.shape[:-1], 4, 4, 4)
    return out


def decode_dxt(raw, w, h, kind):
    """raw: tiled+swapped DXT data. kind 1, 3 or 5."""
    bw, bh = (w + 3) // 4, (h + 3) // 4
    lb = 3 if kind == 1 else 4
    blk = untile(raw, bw, bh, lb)  # [bh, bw, bytes]
    blk = blk.reshape(bh, bw, -1, 2)[..., ::-1].reshape(bh, bw, -1)  # swap bytes in every 16-bit word
    if kind == 1:
        px = _color_block(blk)
    else:
        px = _color_block(blk[..., 8:])
        if kind == 3:
            nib = np.stack([blk[..., :8] & 15, blk[..., :8] >> 4], -1).reshape(bh, bw, 16) * 17
            px[..., 3] = nib.reshape(bh, bw, 4, 4)
        else:
            a0, a1 = blk[..., 0].astype(np.int32), blk[..., 1].astype(np.int32)
            bits = np.zeros(blk.shape[:2], np.uint64)
            for k in range(6):
                bits |= blk[..., 2 + k].astype(np.uint64) << np.uint64(8 * k)
            tab = np.zeros(blk.shape[:2] + (8,), np.int32)
            tab[..., 0], tab[..., 1] = a0, a1
            for k in range(1, 7):
                tab[..., 1 + k] = np.where((a0 > a1), ((7 - k) * a0 + k * a1) // 7, 0)
            m6 = a0 <= a1
            for k in range(1, 5):
                tab[..., 1 + k] = np.where(m6, ((5 - k) * a0 + k * a1) // 5, tab[..., 1 + k])
            tab[..., 6] = np.where(m6, 0, tab[..., 6]); tab[..., 7] = np.where(m6, 255, tab[..., 7])
            idx = ((bits[..., None] >> (np.arange(16, dtype=np.uint64) * np.uint64(3))) & np.uint64(7)).astype(np.int64)
            px[..., 3] = np.take_along_axis(tab, idx, -1).reshape(bh, bw, 4, 4)
    img = px.transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return img[:h, :w].astype(np.uint8)


def decode_rgba8(raw, w, h):
    px = untile(raw, w, h, 2)  # bytes [h, w, 4]; stored big endian ARGB
    return np.stack([px[..., 1], px[..., 2], px[..., 3], px[..., 0]], -1)


def decode_entry(x):
    """x: unpacked entry. Returns RGBA array or None when it is not a texture."""
    fmt, w, h, ds = struct.unpack('>4I', x[:16])
    if fmt not in (2, 3, 4, 5) or not (0 < w <= 8192 and 0 < h <= 8192) or ds + 0x1000 > len(x):
        return None
    raw = x[0x1000:0x1000 + ds]
    if fmt == 2:
        return decode_rgba8(raw, w, h)
    return decode_dxt(raw, w, h, {3: 3, 4: 1, 5: 5}[fmt])


def mrg_of(path):
    """Open .mrg, or an .mrgc (one LZ blob that holds a whole .mrg)."""
    from mrg import unpack
    d = open(path, 'rb').read()
    if path.endswith('c'):
        us, cs = struct.unpack('>II', d[:8])
        inner = unpack(d[8:8 + cs])
        assert len(inner) == us, (len(inner), us)
        m = Mrg.__new__(Mrg)
        m.d = inner
        m.n = struct.unpack('>I', inner[:4])[0]
        m.ents = [struct.unpack('>II', inner[0x20 + i * 0x20:0x28 + i * 0x20]) for i in range(m.n)]
        return m
    return Mrg(path)


if __name__ == '__main__':
    m = mrg_of(sys.argv[1])
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    base = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    for i in range(m.n):
        x = m.get(i)
        img = decode_entry(x)
        if img is None:
            print(i, 'not a texture', x[:16].hex(' ', 4))
            continue
        Image.fromarray(img, 'RGBA').save(os.path.join(out, '%s_%02d.png' % (base, i)))
        print(i, img.shape[1], 'x', img.shape[0])
