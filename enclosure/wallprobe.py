#!/usr/bin/env python3
"""Measure local wall thickness of an STL by ray casting.

From sample points on every triangle, a ray goes inward (against the face normal) and the distance to the first
surface it meets is the local material thickness there. Prints the minimum and every sample under the threshold,
grouped into 1 mm cells, so thin spots can be located and judged (a snap finger is meant to be thin; a wall is not).

Usage: wallprobe.py part.stl [--min 1.2] [--box x0 x1 y0 y1 z0 z1 ...]
  --box limits the report to samples inside the given box (repeatable).
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stlinfo import read_stl  # noqa: E402


def probe(tris, sub=3):
    T = np.array(tris, dtype=float)                      # (n, 3, 3)
    a, b, c = T[:, 0], T[:, 1], T[:, 2]
    n = np.cross(b - a, c - a)
    area2 = np.linalg.norm(n, axis=1)
    ok = area2 > 1e-9
    T, a, b, c, n, area2 = T[ok], a[ok], b[ok], c[ok], n[ok], area2[ok]
    n = n / area2[:, None]
    # barycentric sample grid inside each triangle (interior points only)
    w = [(i / sub, j / sub, 1 - (i + j) / sub) for i in range(1, sub) for j in range(1, sub) if i + j < sub]
    w = w or [(1 / 3, 1 / 3, 1 / 3)]
    pts, nrm = [], []
    for (u, v, s) in w:
        pts.append(a * s + b * u + c * v)
        nrm.append(n)
    P = np.concatenate(pts)
    D = -np.concatenate(nrm)
    P = P + D * 1e-4
    e1, e2 = b - a, c - a
    best = np.full(len(P), np.inf)
    for i0 in range(0, len(P), 2000):
        p, d = P[i0:i0 + 2000, None, :], D[i0:i0 + 2000, None, :]
        h = np.cross(d, e2[None])
        det = np.einsum('ijk,jk->ij', h, e1)
        good = np.abs(det) > 1e-12
        inv = np.where(good, 1 / np.where(good, det, 1), 0)
        s = p - a[None]
        u = np.einsum('ijk,ijk->ij', s, h) * inv
        q = np.cross(s, e1[None])
        v = np.einsum('ijk,ijk->ij', d, q) * inv
        t = np.einsum('ijk,jk->ij', q, e2) * inv
        hit = good & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-3)
        t = np.where(hit, t, np.inf)
        best[i0:i0 + 2000] = t.min(axis=1)
    return P, best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('stl')
    ap.add_argument('--min', type=float, default=1.2)
    ap.add_argument('--box', type=float, nargs=6, action='append')
    ap.add_argument('--sub', type=int, default=4)
    ap.add_argument('--summary', action='store_true', help='one line per thickness value with its extent')
    args = ap.parse_args()
    P, t = probe(read_stl(args.stl), args.sub)
    finite = np.isfinite(t)
    sel = finite
    if args.box:
        inbox = np.zeros(len(P), bool)
        for x0, x1, y0, y1, z0, z1 in args.box:
            inbox |= ((P[:, 0] >= x0) & (P[:, 0] <= x1) & (P[:, 1] >= y0) & (P[:, 1] <= y1)
                      & (P[:, 2] >= z0) & (P[:, 2] <= z1))
        sel &= inbox
    print(f'{args.stl}: {sel.sum()} samples, min thickness {t[sel].min():.2f} mm')
    thin = sel & (t < args.min - 1e-3)
    cells = {}
    for p, tt in zip(P[thin], t[thin]):
        k = tuple(np.floor(p).astype(int))
        cells[k] = min(cells.get(k, 9), tt)
    if args.summary:
        groups = {}
        for k, v in cells.items():
            groups.setdefault(round(v, 2), []).append(k)
        for v in sorted(groups):
            K = np.array(groups[v])
            lo, hi = K.min(axis=0), K.max(axis=0) + 1
            print(f'   {v:.2f} mm in {len(K)} cells, x {lo[0]}..{hi[0]} y {lo[1]}..{hi[1]} z {lo[2]}..{hi[2]}')
        return
    for k in sorted(cells, key=lambda k: (cells[k], k)):
        print(f'   {cells[k]:.2f} mm at x {k[0]}..{k[0] + 1} y {k[1]}..{k[1] + 1} z {k[2]}..{k[2] + 1}')


if __name__ == '__main__':
    main()
