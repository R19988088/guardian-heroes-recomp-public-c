#!/usr/bin/env python3
"""Guardian Heroes .mrg archive reader (format taken from the game's own decoder at 0x8242A588).

Archive: BE u32 count at 0, then 0x20-byte records (BE offset, BE size, 24 zero bytes) at 0x20...
Entry:   BE u32 unpacked size, BE u32 packed size, then the LZ stream. If the first u32 looks like
         a texture header (small format id) the entry is stored raw (e.g. sales_promotion.mrg).
LZ tokens (t = next byte; chunk window is the output since the last flush):
  1xxxxxxx          copy ((t&3)+1) bytes from distance ((t>>2)&0x1f)+1
  01xxxxxx b        v=t<<8|b: copy ((v&15)+2) bytes from distance ((v>>4)&0x3ff)+1
  001xxxxx b        v=t<<8|b: copy ((v&0x1ff)+2) bytes from distance ((v>>9)&15)+1
  0001xxxx..0000001 literal run of (t&0x1f) bytes (t in 1..0x1f)
  00000000          end of chunk: output is padded to a multiple of 4 and flushed
"""
import struct, sys


def unpack(src):
    """Decode one LZ stream (without the 8-byte entry header)."""
    out = bytearray()
    buf = bytearray()
    p, n = 0, len(src)
    while p < n:
        t = src[p]; p += 1
        if t & 0x80:
            cnt = (t & 3) + 1; dist = ((t >> 2) & 0x1f) + 1
        elif t & 0x40:
            v = (t << 8) | src[p]; p += 1
            cnt = (v & 15) + 2; dist = ((v >> 4) & 0x3ff) + 1
        elif t & 0x20:
            v = (t << 8) | src[p]; p += 1
            cnt = (v & 0x1ff) + 2; dist = ((v >> 9) & 15) + 1
        elif t:
            k = t & 0x1f
            buf += src[p:p + k]; p += k
            continue
        else:
            pad = (-len(buf)) & 3
            out += buf; out += b'\0' * pad
            buf = bytearray()
            continue
        s = len(buf) - dist
        if s < 0:
            raise ValueError('back reference before chunk start')
        if dist >= cnt:
            buf += buf[s:s + cnt]
        else:
            for i in range(cnt):
                buf.append(buf[s + i])
    if buf:
        out += buf
    return bytes(out)


def pack(data, chunk=2048):
    """Greedy encoder for the stream above. The game flushes every 2048 bytes, so do we."""
    if len(data) % 4:
        raise ValueError('data length must be a multiple of 4')
    out = bytearray()
    for base in range(0, len(data), chunk):
        blk = data[base:base + chunk]
        n = len(blk)
        heads = {}
        lit = bytearray()

        def flush_lit():
            for k in range(0, len(lit), 31):
                part = lit[k:k + 31]
                out.append(len(part)); out.extend(part)
            lit.clear()

        i = 0
        while i < n:
            best = (0, 0, 0)  # score, length, dist
            if i + 2 < n:
                key = blk[i:i + 3]
                cands = heads.get(key)
                if cands:
                    for j in reversed(cands[-12:]):
                        dist = i - j
                        if dist > 1024:
                            break
                        mx = 513 if dist <= 16 else 17
                        mx = min(mx, n - i)
                        ln = 3
                        while ln < mx and blk[j + ln] == blk[i + ln]:
                            ln += 1
                        if dist <= 32 and ln <= 4:
                            sc = ln - 1
                        elif dist <= 16:
                            sc = ln - 2
                        else:
                            sc = ln - 2
                        if sc > best[0]:
                            best = (sc, ln, dist)
            if best[0] >= 1:
                _, ln, dist = best
                flush_lit()
                if dist <= 32 and ln <= 4:
                    out.append(0x80 | ((dist - 1) << 2) | (ln - 1))
                elif dist <= 16:
                    v = 0x2000 | ((dist - 1) << 9) | (ln - 2)
                    out += struct.pack('>H', v)
                else:
                    v = 0x4000 | ((dist - 1) << 4) | (ln - 2)
                    out += struct.pack('>H', v)
                step = ln
            else:
                lit.append(blk[i]); step = 1
            for k in range(i, min(i + step, n - 2)):
                heads.setdefault(blk[k:k + 3], []).append(k)
            i += step
        flush_lit()
        out.append(0)
    return bytes(out)


def pack_entry(data):
    s = pack(data)
    return struct.pack('>II', len(data), len(s)) + s


def build(entries, count=None):
    """Write an archive from raw entry blobs (already with their 8-byte headers where packed)."""
    n = len(entries)
    head = bytearray(0x1000)
    struct.pack_into('>I', head, 0, n)
    body = bytearray()
    pos = 0x1000
    for i, e in enumerate(entries):
        struct.pack_into('>II', head, 0x20 + i * 0x20, pos, len(e))
        pad = (-len(e)) & 0xFFF
        body += e + b'\0' * pad
        pos += len(e) + pad
    return bytes(head) + bytes(body)


class Mrg:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        self.n = struct.unpack('>I', self.d[:4])[0]
        self.ents = [struct.unpack('>II', self.d[0x20 + i * 0x20:0x28 + i * 0x20]) for i in range(self.n)]

    def raw(self, i):
        o, s = self.ents[i]
        return self.d[o:o + s]

    def get(self, i):
        b = self.raw(i)
        us, cs = struct.unpack('>II', b[:8])
        if cs + 8 == len(b) and us >= cs // 2 and us < (1 << 28):
            data = unpack(b[8:])
            if len(data) != us:
                raise ValueError('entry %d: got %#x bytes, header says %#x' % (i, len(data), us))
            return data
        return b


if __name__ == '__main__':
    m = Mrg(sys.argv[1])
    for i in range(m.n):
        o, s = m.ents[i]
        try:
            x = m.get(i)
            print(i, hex(o), hex(s), '->', hex(len(x)), x[:16].hex(' '))
        except Exception as e:
            print(i, hex(o), hex(s), 'ERR', e)
