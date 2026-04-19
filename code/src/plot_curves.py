#!/usr/bin/env python3
import argparse
import csv
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt
import numpy as np


def read_curve_csv(csv_path: Path) -> List[Dict]:
    """Read rd_curve_points.csv and return list of dictionaries."""
    rows = []
    with csv_path.open("r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def parse_float(val):
    try:
        return float(val)
    except (ValueError, TypeError):
        return None


def group_by_mode_image(rows: List[Dict]) -> Dict[Tuple[str, str], List[Dict]]:
    """Group rows by (mode, image) for per-image curves."""
    grouped = {}
    for row in rows:
        mode = row.get("mode", "").strip()
        image = row.get("image", "").strip()
        key = (mode, image)
        if key not in grouped:
            grouped[key] = []
        grouped[key].append(row)
    return grouped


def group_by_mode(rows: List[Dict]) -> Dict[str, List[Dict]]:
    """Group rows by mode for overall curves."""
    grouped = {}
    for row in rows:
        mode = row.get("mode", "").strip()
        if mode not in grouped:
            grouped[mode] = []
        grouped[mode].append(row)
    return grouped


def plot_global_curves(rows: List[Dict], output_dir: Path, psnr_threshold: float = None):
    """Plot global rate/distortion curves by mode."""
    grouped = group_by_mode(rows)

    fig, ax = plt.subplots(figsize=(10, 6))

    for mode in sorted(grouped.keys()):
        points = grouped[mode]
        # Aggregate by tested parameter tuple to avoid visually misleading
        # "lines" built by connecting tens of thousands of unrelated image points.
        by_params = {}
        for p in points:
            bpp = parse_float(p.get("bpp"))
            psnr = parse_float(p.get("psnr_db"))
            if bpp is None or psnr is None:
                continue
            if psnr_threshold is not None and psnr < psnr_threshold:
                continue

            k = p.get("k", "")
            m = p.get("m", "")
            g = p.get("g", "")
            key = (k, m, g)
            by_params.setdefault(key, {"bpp": [], "psnr": []})
            by_params[key]["bpp"].append(bpp)
            by_params[key]["psnr"].append(psnr)

        curve_points = []
        for _, vals in by_params.items():
            if vals["bpp"] and vals["psnr"]:
                curve_points.append((float(np.mean(vals["bpp"])), float(np.mean(vals["psnr"]))))

        if curve_points:
            curve_points.sort(key=lambda x: x[0])
            bpps_sorted = [b for b, _ in curve_points]
            psnrs_sorted = [p for _, p in curve_points]
            ax.plot(bpps_sorted, psnrs_sorted, marker="o", label=mode, linewidth=2, markersize=6)

    ax.set_xlabel("Débit (bits/pixel)")
    ax.set_ylabel("PSNR (dB)")
    title = "Courbes Débit/Distorsion"
    if psnr_threshold:
        title += f" (PSNR >= {psnr_threshold} dB)"
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    ax.legend()

    suffix = f"_psnr{psnr_threshold}" if psnr_threshold else ""
    output_file = output_dir / f"rd_curves_global{suffix}.png"
    fig.savefig(output_file, dpi=150, bbox_inches="tight")
    print(f"Saved: {output_file}")
    plt.close(fig)


def plot_global_cloud(rows: List[Dict], output_dir: Path, psnr_threshold: float = None, num_bins: int = 20):
    """Plot global cloud (all points) + binned median trend per mode."""
    grouped = group_by_mode(rows)

    fig, ax = plt.subplots(figsize=(11, 7))

    for mode in sorted(grouped.keys()):
        points = grouped[mode]
        bpps = []
        psnrs = []
        for p in points:
            bpp = parse_float(p.get("bpp"))
            psnr = parse_float(p.get("psnr_db"))
            if bpp is None or psnr is None:
                continue
            if psnr_threshold is not None and psnr < psnr_threshold:
                continue
            bpps.append(bpp)
            psnrs.append(psnr)

        if not bpps:
            continue

        # Cloud of all points to show spread.
        ax.scatter(bpps, psnrs, s=8, alpha=0.15, label=f"{mode} (points)")

        # Median trend by bpp bins for robust interpretation.
        bpp_arr = np.array(bpps)
        psnr_arr = np.array(psnrs)
        if bpp_arr.size >= 4:
            bmin = float(np.min(bpp_arr))
            bmax = float(np.max(bpp_arr))
            if bmax > bmin:
                edges = np.linspace(bmin, bmax, num=max(4, num_bins) + 1)
                centers = []
                medians = []
                p25 = []
                p75 = []
                for i in range(len(edges) - 1):
                    left, right = edges[i], edges[i + 1]
                    mask = (bpp_arr >= left) & (bpp_arr < right if i < len(edges) - 2 else bpp_arr <= right)
                    if np.count_nonzero(mask) < 3:
                        continue
                    vals = psnr_arr[mask]
                    centers.append((left + right) / 2.0)
                    medians.append(float(np.median(vals)))
                    p25.append(float(np.percentile(vals, 25)))
                    p75.append(float(np.percentile(vals, 75)))

                if centers:
                    ax.plot(centers, medians, linewidth=2.5, label=f"{mode} (médiane)")
                    ax.fill_between(centers, p25, p75, alpha=0.15)

    ax.set_xlabel("Débit (bits/pixel)")
    ax.set_ylabel("PSNR (dB)")
    title = "Nuage global Débit/Distorsion + tendance médiane"
    if psnr_threshold:
        title += f" (PSNR >= {psnr_threshold} dB)"
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    ax.legend()

    suffix = f"_psnr{psnr_threshold}" if psnr_threshold else ""
    output_file = output_dir / f"rd_curves_cloud{suffix}.png"
    fig.savefig(output_file, dpi=150, bbox_inches="tight")
    print(f"Saved: {output_file}")
    plt.close(fig)


def plot_per_image_curves(rows: List[Dict], output_dir: Path, psnr_threshold: float = None):
    """Plot rate/distortion curves per image and mode."""
    grouped = group_by_mode_image(rows)

    # Limit to first N unique images to avoid too many plots
    unique_images = sorted(set(r.get("image", "").strip() for r in rows))[:5]

    for image in unique_images:
        fig, ax = plt.subplots(figsize=(10, 6))

        for mode in ["SLIC", "slicCC"]:
            key = (mode, image)
            if key not in grouped:
                continue

            points = grouped[key]
            bpps = []
            psnrs = []

            for p in points:
                bpp = parse_float(p.get("bpp"))
                psnr = parse_float(p.get("psnr_db"))
                if bpp is not None and psnr is not None:
                    if psnr_threshold is None or psnr >= psnr_threshold:
                        bpps.append(bpp)
                        psnrs.append(psnr)

            if bpps and psnrs:
                sorted_pairs = sorted(zip(bpps, psnrs))
                bpps_sorted = [b for b, _ in sorted_pairs]
                psnrs_sorted = [p for _, p in sorted_pairs]
                ax.plot(bpps_sorted, psnrs_sorted, marker="o", label=mode, linewidth=2, markersize=4)

        ax.set_xlabel("Débit (bits/pixel)")
        ax.set_ylabel("PSNR (dB)")
        img_name = Path(image).stem
        title = f"Courbes Débit/Distorsion - {img_name}"
        if psnr_threshold:
            title += f" (PSNR >= {psnr_threshold} dB)"
        ax.set_title(title)
        ax.grid(True, alpha=0.3)
        ax.legend()

        suffix = f"_psnr{psnr_threshold}" if psnr_threshold else ""
        output_file = output_dir / f"rd_curves_{img_name}{suffix}.png"
        fig.savefig(output_file, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_file}")
        plt.close(fig)


def plot_summary_by_params(summary_csv: Path, output_dir: Path):
    """Plot summary statistics: avg compression rate vs avg PSNR by params."""
    rows = []
    with summary_csv.open("r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    fig, ax = plt.subplots(figsize=(10, 6))

    grouped_by_mode = {}
    for row in rows:
        mode = row.get("mode", "").strip()
        if mode not in grouped_by_mode:
            grouped_by_mode[mode] = {"bpp": [], "psnr": [], "k": []}

        try:
            avg_psnr = float(row.get("avg_psnr_db", 0))
            avg_bpp = float(row.get("avg_bpp", 0))
            k = row.get("k", "")
            grouped_by_mode[mode]["psnr"].append(avg_psnr)
            grouped_by_mode[mode]["bpp"].append(avg_bpp)
            grouped_by_mode[mode]["k"].append(k)
        except (ValueError, TypeError):
            pass

    for mode in sorted(grouped_by_mode.keys()):
        data = grouped_by_mode[mode]
        if data["bpp"] and data["psnr"]:
            sorted_pairs = sorted(zip(data["bpp"], data["psnr"]))
            bpps_sorted = [b for b, _ in sorted_pairs]
            psnrs_sorted = [p for _, p in sorted_pairs]
            ax.plot(bpps_sorted, psnrs_sorted, marker="s", label=mode, linewidth=2, markersize=6)

    ax.set_xlabel("Débit moyen (bits/pixel)")
    ax.set_ylabel("PSNR moyen (dB)")
    ax.set_title("Résumé Débit/Distorsion (moyennes par paramètres)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.axhline(y=30, color="red", linestyle="--", alpha=0.5, label="PSNR 30 dB")

    output_file = output_dir / "rd_summary_params.png"
    fig.savefig(output_file, dpi=150, bbox_inches="tight")
    print(f"Saved: {output_file}")
    plt.close(fig)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot rate/distortion curves from batch evaluation CSV.")
    parser.add_argument(
        "--results-dir",
        default="evaluation_results",
        help="Directory containing CSV files (default: evaluation_results).",
    )
    parser.add_argument(
        "--output-dir",
        default="evaluation_results",
        help="Directory where plots will be saved (default: same as results-dir).",
    )
    parser.add_argument(
        "--psnr-threshold",
        type=float,
        default=None,
        help="Filter points to PSNR >= threshold (e.g., 30.0 for PSNR >= 30 dB).",
    )
    parser.add_argument(
        "--per-image",
        action="store_true",
        help="Also generate per-image plots (limited to first 5 images).",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        help="Generate summary plot from rd_summary_by_params.csv.",
    )
    parser.add_argument(
        "--cloud",
        action="store_true",
        help="Generate cloud plot (all points) with median trend bins.",
    )
    parser.add_argument(
        "--bins",
        type=int,
        default=20,
        help="Number of bins for cloud median trend (default: 20).",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    results_dir = Path(args.results_dir).resolve()
    output_dir = Path(args.output_dir).resolve()

    if not results_dir.exists():
        print(f"Results directory not found: {results_dir}", file=sys.stderr)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    curve_csv = results_dir / "rd_curve_points.csv"
    if not curve_csv.exists():
        print(f"Curve CSV not found: {curve_csv}", file=sys.stderr)
        return 1

    print(f"Reading curves from: {curve_csv}")
    rows = read_curve_csv(curve_csv)

    if not rows:
        print("No data found in CSV.", file=sys.stderr)
        return 1

    print(f"Generating global curves...")
    plot_global_curves(rows, output_dir, psnr_threshold=args.psnr_threshold)

    if args.cloud:
        print("Generating cloud plot with median trend...")
        plot_global_cloud(rows, output_dir, psnr_threshold=args.psnr_threshold, num_bins=args.bins)

    if args.psnr_threshold:
        plot_global_curves(rows, output_dir, psnr_threshold=None)
        if args.cloud:
            plot_global_cloud(rows, output_dir, psnr_threshold=None, num_bins=args.bins)

    if args.per_image:
        print(f"Generating per-image curves (limited to first 5 images)...")
        plot_per_image_curves(rows, output_dir, psnr_threshold=args.psnr_threshold)

    if args.summary:
        summary_csv = results_dir / "rd_summary_by_params.csv"
        if summary_csv.exists():
            print(f"Generating summary plot...")
            plot_summary_by_params(summary_csv, output_dir)
        else:
            print(f"Summary CSV not found: {summary_csv}", file=sys.stderr)

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
