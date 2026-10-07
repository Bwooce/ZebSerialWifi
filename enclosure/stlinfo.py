#!/usr/bin/env python3
"""Print bounding box, volume and triangle count of STL files (ASCII or binary).

With --components, also list connected pieces (by shared vertices) with their bbox and volume,
which is how the interference checks locate overlaps.
"""
import struct
import sys


def read_stl(path):
    data = open(path, 'rb').read()
    tris = []
    if data[:5] == b'solid' and b'facet' in data[:400]:
        verts = []
        for line in data.decode(errors='replace').splitlines():
            t = line.split()
            if t and t[0] == 'vertex':
                verts.append(tuple(float(v) for v in t[1:4]))
                if len(verts) == 3:
                    tris.append(verts)
                    verts = []
    else:
        n = struct.unpack('<I', data[80:84])[0]
        for i in range(n):
            off = 84 + i * 50
            v = struct.unpack('<12f', data[off:off + 48])
            tris.append([v[3:6], v[6:9], v[9:12]])
    return tris


def vol(tris):
    s = 0.0
    for a, b, c in tris:
        s += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0])
              + a[2] * (b[0] * c[1] - b[1] * c[0]))
    return s / 6.0


def bbox(tris):
    pts = [p for t in tris for p in t]
    return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]


def components(tris):
    parent = {}

    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    key = lambda p: tuple(round(c, 4) for c in p)
    for t in tris:
        k = [key(p) for p in t]
        for o in k[1:]:
            parent[find(k[0])] = find(o)
    groups = {}
    for t in tris:
        groups.setdefault(find(key(t[0])), []).append(t)
    return list(groups.values())


for path in [a for a in sys.argv[1:] if not a.startswith('--')]:
    tris = read_stl(path)
    if not tris:
        print(f'{path}: EMPTY')
        continue
    lo, hi = bbox(tris)
    print(f'{path}: {len(tris)} tris, volume {vol(tris) / 1000:.3f} cm3, '
          f'bbox {[round(v, 2) for v in lo]} .. {[round(v, 2) for v in hi]}')
    if '--components' in sys.argv:
        for g in sorted(components(tris), key=vol, reverse=True):
            lo, hi = bbox(g)
            print(f'   piece vol {vol(g):8.3f} mm3  {[round(v, 2) for v in lo]} .. {[round(v, 2) for v in hi]}')
