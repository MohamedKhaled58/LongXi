#!/usr/bin/env python3
"""
WDF Archive Unpacker
"""

import argparse
import os
import struct
from pathlib import Path


class WdfHeader:
    FORMAT = '<III'
    SIZE = 12

    def __init__(self, data: bytes):
        self.id, self.count, self.offset = struct.unpack(self.FORMAT, data)


class WdfIndexEntry:
    FORMAT = '<IIII'
    SIZE = 16

    def __init__(self, data: bytes):
        self.uid, self.offset, self.size, self.space = struct.unpack(self.FORMAT, data)


def normalize_path(path: str) -> str:
    return path.replace('\\', '/').lower()


def stringtoid(path: str) -> int:
    m = [0] * 70
    path_bytes = path.encode('ascii', errors='replace')[:64]
    for i, b in enumerate(path_bytes):
        m[i] = b

    i = 0
    while i < 64 and m[i] != 0:
        i += 1
    m[i] = 0x9BE74448
    m[i + 1] = 0x66F42C48
    i += 2

    v = 0xF4FA8928
    x = 0x37A8470E
    y = 0x7758B42B

    for j in range(i):
        v = ((v << 1) | (v >> 31)) & 0xFFFFFFFF
        w = (0x267B0B11 ^ v) & 0xFFFFFFFF

        x ^= m[j]
        y ^= m[j]

        mult1 = ((w + y) | 0x02040801) & 0xBFEF7FDF
        p1 = (x * mult1) & 0xFFFFFFFFFFFFFFFF
        lo1 = p1 & 0xFFFFFFFF
        hi1 = (p1 >> 32) & 0xFFFFFFFF
        sum1 = lo1 + hi1 + (1 if hi1 != 0 else 0)
        x = sum1 & 0xFFFFFFFF
        if sum1 > 0xFFFFFFFF:
            x = (x + 1) & 0xFFFFFFFF

        mult2 = ((w + x) | 0x00804021) & 0x7DFEFBFF
        p2 = (y * mult2) & 0xFFFFFFFFFFFFFFFF
        lo2 = p2 & 0xFFFFFFFF
        hi2 = (p2 >> 32) & 0xFFFFFFFF
        hi2x2 = hi2 * 2
        sum2 = lo2 + (hi2x2 & 0xFFFFFFFF) + (1 if hi2x2 > 0xFFFFFFFF else 0)
        y = sum2 & 0xFFFFFFFF
        if sum2 > 0xFFFFFFFF:
            y = (y + 2) & 0xFFFFFFFF

    return (x ^ y) & 0xFFFFFFFF


class WdfReader:
    def __init__(self, path: str):
        self.path = Path(path)

    def read_index(self):
        with self.path.open('rb') as f:
            header = WdfHeader(f.read(WdfHeader.SIZE))
            f.seek(header.offset)
            entries = []
            for _ in range(header.count):
                raw = f.read(WdfIndexEntry.SIZE)
                if len(raw) < WdfIndexEntry.SIZE:
                    break
                entries.append(WdfIndexEntry(raw))
            return entries

    def read_entry(self, entry: WdfIndexEntry) -> bytes:
        with self.path.open('rb') as f:
            f.seek(entry.offset)
            return f.read(entry.size)


def load_manifest(path: str):
    if not path:
        return {}
    p = Path(path)
    if not p.exists():
        return {}

    mapping = {}
    for line in p.read_text(encoding='utf-8', errors='ignore').splitlines():
        value = line.strip()
        if not value or value.startswith('#'):
            continue
        normalized = normalize_path(value)
        mapping[stringtoid(normalized)] = normalized
    return mapping


def safe_name(uid: int) -> str:
    return f'UID_{uid:08X}.dat'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive')
    parser.add_argument('manifest', nargs='?')
    args = parser.parse_args()

    reader = WdfReader(args.archive)
    entries = reader.read_index()
    manifest = load_manifest(args.manifest)

    out_dir = Path(args.archive).with_suffix('')
    out_dir = Path(str(out_dir) + '_Unpacked')
    out_dir.mkdir(parents=True, exist_ok=True)

    for entry in entries:
        rel = manifest.get(entry.uid, safe_name(entry.uid))
        target = out_dir / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(reader.read_entry(entry))

    print(f'Extracted {len(entries)} files to {out_dir}')


if __name__ == '__main__':
    main()