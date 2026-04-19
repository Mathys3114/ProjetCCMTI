#!/usr/bin/env python3
import csv
from pathlib import Path
import statistics

csv_path = Path("evaluation_results/rd_curve_points.csv")

if not csv_path.exists():
    print(f"CSV not found: {csv_path}")
    exit(1)

bpps = []
psnrs = []
ks = []
with csv_path.open("r") as f:
    reader = csv.DictReader(f)
    for row in reader:
        try:
            bpps.append(float(row.get("bpp", 0)))
            psnrs.append(float(row.get("psnr_db", 0)))
            ks.append(float(row.get("k", 0)))
        except:
            pass

print(f"Total points: {len(bpps)}")
print(f"BPP stats: min={min(bpps):.4f}, max={max(bpps):.4f}, mean={statistics.mean(bpps):.4f}, stdev={statistics.stdev(bpps) if len(bpps) > 1 else 0:.4f}")
print(f"PSNR stats: min={min(psnrs):.2f}, max={max(psnrs):.2f}, mean={statistics.mean(psnrs):.2f}, stdev={statistics.stdev(psnrs) if len(psnrs) > 1 else 0:.2f}")
print(f"K stats: min={min(ks):.0f}, max={max(ks):.0f}")
print(f"\nUnique K values: {sorted(set(ks))}")
print(f"Unique BPP (first 20): {sorted(set(bpps))[:20]}")
