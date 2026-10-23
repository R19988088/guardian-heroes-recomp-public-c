#!/usr/bin/env python3
"""Decrypt (retail key) and LZX-unpack the PE image of a normal-compressed XEX2.

    python3 -I tools/xex_unpack.py default.xex out.bin

Research helper for finding the game's own file decompressors. The output is the raw image
(base 0x82000000 when no section headers are applied); it is game code and must not be committed.
"""
import struct, sys
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

RETAIL = bytes.fromhex('20B185A59D28FDC340583FBB0896BF91')


class Lzx:
    def __init__(self, window_bits):
        self.wb = window_bits
        self.win = bytearray(1 << window_bits)
        self.wpos = 0
        self.slots = {15: 30, 16: 32, 17: 34, 18: 36, 19: 38, 20: 42, 21: 50}[window_bits]
        eb, base = [], []
        b = 0
        for i in range(self.slots):
            e = 0 if i < 4 else min((i - 2) // 2, 17)
            eb.append(e)
            base.append(b)
            b += 1 << e
        self.extra, self.base = eb, base
        self.R = [1, 1, 1]
        self.main = [0] * (256 + 8 * self.slots)
        self.length = [0] * 249
        self.btype = 0
        self.brem = 0
        self.first = True
        self.filesize = 0
        self.frames = 0
        self.aligned = None

    @staticmethod
    def table(lens):
        mx = max(lens) if lens else 0
        if mx == 0:
            return (0, [])
        t = [-1] * (1 << mx)
        code = 0
        for L in range(1, mx + 1):
            for s, l in enumerate(lens):
                if l == L:
                    n = 1 << (mx - L)
                    st = code << (mx - L)
                    for k in range(st, st + n):
                        t[k] = (s << 5) | L
                    code += 1
            code <<= 1
        return (mx, t)

    def frame(self, data, outlen):
        """Decode one input chunk into outlen bytes (appended to window); return them."""
        words = struct.unpack('<%dH' % (len(data) // 2), data[:len(data) // 2 * 2])
        st = {'i': 0, 'buf': 0, 'n': 0}

        def need(k):
            while st['n'] < k:
                w = words[st['i']] if st['i'] < len(words) else 0
                st['i'] += 1
                st['buf'] = (st['buf'] << 16) | w
                st['n'] += 16

        def bits(k):
            if k == 0:
                return 0
            need(k)
            st['n'] -= k
            v = (st['buf'] >> st['n']) & ((1 << k) - 1)
            st['buf'] &= (1 << st['n']) - 1
            return v

        def dec(tb):
            mx, t = tb
            need(mx)
            v = (st['buf'] >> (st['n'] - mx)) & ((1 << mx) - 1)
            e = t[v]
            if e < 0:
                raise ValueError('bad huffman code')
            L = e & 31
            st['n'] -= L
            st['buf'] &= (1 << st['n']) - 1
            return e >> 5

        def read_lens(lens, a, z):
            pre = self.table([bits(4) for _ in range(20)])
            x = a
            while x < z:
                s = dec(pre)
                if s == 17:
                    y = bits(4) + 4
                    for _ in range(y):
                        lens[x] = 0; x += 1
                elif s == 18:
                    y = bits(5) + 20
                    for _ in range(y):
                        lens[x] = 0; x += 1
                elif s == 19:
                    y = bits(1) + 4
                    s2 = dec(pre)
                    v = (lens[x] - s2) % 17
                    for _ in range(y):
                        lens[x] = v; x += 1
                else:
                    lens[x] = (lens[x] - s) % 17
                    x += 1

        win, wm = self.win, (1 << self.wb) - 1
        wp = self.wpos
        start = wp
        target = wp + outlen
        if self.first:
            self.first = False
            if bits(1):
                self.filesize = (bits(16) << 16) | bits(16)
        R0, R1, R2 = self.R
        while wp < target:
            if self.brem == 0:
                if self.btype == 3 and st['n'] >= 0 and False:
                    pass
                self.btype = bits(3)
                hi = bits(16)
                self.brem = (hi << 8) | bits(8)
                if self.btype == 2:
                    al = [bits(3) for _ in range(8)]
                    self.atab = self.table(al)
                if self.btype in (1, 2):
                    read_lens(self.main, 0, 256)
                    read_lens(self.main, 256, 256 + 8 * self.slots)
                    self.mtab = self.table(self.main)
                    read_lens(self.length, 0, 249)
                    self.ltab = self.table(self.length)
                elif self.btype == 3:
                    # align to 16 bits, then three 32-bit LE registers
                    if st['n'] % 16 == 0 and st['n'] == 0:
                        pass
                    st['n'] -= st['n'] % 16
                    st['buf'] &= (1 << st['n']) - 1
                    # unread whole words stay in the buffer: rewind index
                    st['i'] -= st['n'] // 16
                    st['n'] = 0; st['buf'] = 0
                    raw = b''.join(struct.pack('<H', w) for w in words[st['i']:st['i'] + 6])
                    R0, R1, R2 = struct.unpack('<III', raw)
                    st['i'] += 6
                else:
                    raise ValueError('bad block type %d' % self.btype)
            run = min(self.brem, target - wp) if self.btype != 3 else min(self.brem, target - wp)
            if self.btype == 3:
                raw = b''.join(struct.pack('<H', w) for w in words[st['i']:st['i'] + (run + 1) // 2])
                for k in range(run):
                    win[(wp + k) & wm] = raw[k]
                st['i'] += (run + 1) // 2
                wp += run
                self.brem -= run
                continue
            mtab = self.mtab; ltab = self.ltab
            done = 0
            while done < run:
                s = dec(mtab)
                if s < 256:
                    win[wp & wm] = s
                    wp += 1; done += 1
                    continue
                m = s - 256
                ml = m & 7
                slot = m >> 3
                if ml == 7:
                    ml += dec(ltab)
                ml += 2
                if slot == 0:
                    off = R0
                elif slot == 1:
                    off = R1; R1 = R0; R0 = off
                elif slot == 2:
                    off = R2; R2 = R0; R0 = off
                else:
                    ex = self.extra[slot]
                    off = self.base[slot] - 2
                    if self.btype == 2 and ex >= 3:
                        if ex > 3:
                            off += bits(ex - 3) << 3
                        off += dec(self.atab)
                    elif ex > 0:
                        off += bits(ex)
                    R2 = R1; R1 = R0; R0 = off
                for k in range(ml):
                    win[wp & wm] = win[(wp - off) & wm]
                    wp += 1
                done += ml
            self.brem -= done
            if self.brem < 0:
                raise ValueError('block overrun')
        self.R = [R0, R1, R2]
        self.wpos = wp
        out = bytearray((win[(start + k) & wm]) for k in range(outlen))
        if self.filesize and self.frames < 32768 and outlen > 10:
            cur = start
            i = 0
            end = outlen - 10
            while i < end:
                if out[i] != 0xE8:
                    i += 1; cur += 1
                    continue
                ab = struct.unpack_from('<i', out, i + 1)[0]
                if -cur <= ab < self.filesize:
                    rel = ab - cur if ab >= 0 else ab + self.filesize
                    struct.pack_into('<i', out, i + 1, rel)
                i += 5; cur += 5
        self.frames += 1
        return bytes(out)


def main():
    d = open(sys.argv[1], 'rb').read()
    pe = struct.unpack('>I', d[8:12])[0]
    sec = struct.unpack('>I', d[16:20])[0]
    nopt = struct.unpack('>I', d[20:24])[0]
    opts = {struct.unpack('>I', d[24 + i * 8:28 + i * 8])[0]: struct.unpack('>I', d[28 + i * 8:32 + i * 8])[0] for i in range(nopt)}
    ff = opts[0x3FF]
    sz, enc, comp = struct.unpack('>IHH', d[ff:ff + 8])
    assert comp == 2
    window, bsize = struct.unpack('>II', d[ff + 8:ff + 16])
    wb = window.bit_length() - 1
    imgsize = struct.unpack('>I', d[sec + 4:sec + 8])[0]
    enc_key = d[sec + 0x150:sec + 0x160]
    sk = Cipher(algorithms.AES(RETAIL), modes.ECB()).decryptor().update(enc_key)
    body = Cipher(algorithms.AES(sk), modes.CBC(b'\0' * 16)).decryptor().update(d[pe:pe + (len(d) - pe) // 16 * 16])
    lz = Lzx(wb)
    out = bytearray()
    p = 0
    bs = bsize
    while bs:
        nxt = struct.unpack('>I', body[p:p + 4])[0]
        p += 24
        q = p
        while True:
            cs = struct.unpack('>H', body[q:q + 2])[0]
            q += 2
            if not cs:
                break
            out += lz.frame(body[q:q + cs], min(0x8000, 1 << 30))
            q += cs
        p = p + bs - 24 if False else p
        # blocks are contiguous: next block starts after this block's bytes
        p = (p - 24) + bs
        bs = nxt
    open(sys.argv[2], 'wb').write(out)
    print('image', hex(len(out)), 'expected', hex(imgsize))


main()
