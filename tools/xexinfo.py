#!/usr/bin/env python3
"""XEX2 header facts: title id, versions, image base, entry, imports, compression. python -I tools/xexinfo.py default.xex"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
magic, flags, pe_off, _, sec_off, nopt = struct.unpack('>4sIIIII', d[:24])
print('magic', magic, 'flags %08x' % flags, 'pe_off %x' % pe_off, 'sec_off %x' % sec_off, 'opt', nopt)
opts = {}
for i in range(nopt):
    k, v = struct.unpack('>II', d[24 + i * 8:32 + i * 8])
    opts[k] = v
for k, v in sorted(opts.items()):
    print('  opt %08x = %08x' % (k, v))
img_size = struct.unpack('>I', d[sec_off + 4:sec_off + 8])[0]
print('image size %x' % img_size)
if 0x40006 in opts:
    o = opts[0x40006]
    mid, ver, bver, tid, plat, etype, disc, ndisc = struct.unpack('>IIIIBBBB', d[o:o + 20])
    print('exec: media %08X version %d.%d.%d.%d base %d.%d.%d.%d title %08X' % (mid, ver >> 28, (ver >> 24) & 15, (ver >> 8) & 0xFFFF if False else (ver >> 8) & 0xFFFF, ver & 255, bver >> 28, (bver >> 24) & 15, (bver >> 8) & 0xFFFF, bver & 255, tid))
if 0x10201 in opts: print('image base %x' % opts[0x10201])
if 0x10100 in opts: print('entry %x' % opts[0x10100])
if 0x3FF in opts:
    o = opts[0x3FF]
    sz, enc, comp = struct.unpack('>IHH', d[o:o + 8])
    print('file format: size %x encryption %d compression %d (1=basic 2=lzx)' % (sz, enc, comp))
if 0x103FF in opts:
    o = opts[0x103FF]
    tsz, tbytes, tcnt = struct.unpack('>III', d[o:o + 12])
    names = d[o + 12:o + 12 + tbytes].split(bytes([0]))
    names = [n.decode() for n in names if n]
    print('import libs:', names)
    p = o + 12 + tbytes
    for i in range(tcnt):
        bsz, = struct.unpack('>I', d[p:p + 4])
        lid, ver, minver, nameidx, cnt = struct.unpack('>IIIHH', d[p + 24:p + 40])
        print('  lib %s version %d.%d.%d.%d imports %d' % (names[nameidx & 0xFF], ver >> 28, (ver >> 24) & 15, (ver >> 8) & 0xFFFF, ver & 255, cnt))
        p += bsz
