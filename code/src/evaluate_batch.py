#!/usr/bin/env python3
import argparse
import csv
import io
import itertools
import os
import re
import statistics
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Tuple

from PIL import Image


@dataclass
class EvalRow:
    image: str
    mode: str
    k: float
    m: float
    g: float
    width: int
    height: int
    original_size_bytes: int
    compressed_size_bytes: int
    compression_rate: float
    bpp: float
    psnr_db: float
    psnr_ok_30db: bool
    output_image: str


def parse_list_of_floats(raw: str) -> List[float]:
    values: List[float] = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        values.append(float(token))
    if not values:
        raise ValueError("Empty float list")
    return values


def find_project_root() -> Path:
    return Path(__file__).resolve().parents[2]


def parse_psnr_output(psnr_stdout: str) -> float:
    match = re.search(r"([+-]?\d+(?:\.\d+)?)", psnr_stdout)
    if not match:
        raise ValueError(f"Impossible de lire le PSNR: {psnr_stdout!r}")
    return float(match.group(1))


def read_ppm_dimensions(path: Path) -> Tuple[int, int]:
    with path.open("rb") as f:
        header_tokens: List[bytes] = []
        while len(header_tokens) < 4:
            line = f.readline()
            if not line:
                break
            line = line.strip()
            if not line or line.startswith(b"#"):
                continue
            header_tokens.extend(line.split())

    if len(header_tokens) < 4:
        raise ValueError(f"PPM header invalide: {path}")
    magic = header_tokens[0]
    if magic not in (b"P3", b"P6"):
        raise ValueError(f"Format PPM non supporté {path}: {magic!r}")

    width = int(header_tokens[1])
    height = int(header_tokens[2])
    _maxval = int(header_tokens[3])
    return width, height


def png_encoded_size_bytes(path: Path) -> int:
    """Return in-memory PNG size for a fair, content-dependent size metric."""
    with Image.open(path) as img:
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        return buf.tell()


def run_command(cmd: List[str], cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True, check=True)


def parse_autoslic_parameters(stdout_text: str) -> Tuple[float, float, float]:
    num = r"([+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)"
    mk = re.search(r"-\s*K\s*:\s*" + num, stdout_text)
    mg = re.search(r"-\s*g\s*:\s*" + num, stdout_text)
    mm = re.search(r"-\s*m\s*:\s*" + num, stdout_text)
    if not (mk and mg and mm):
        raise ValueError(f"Impossible de parser les paramètres autoSLIC:\n{stdout_text}")
    k = float(mk.group(1))
    g = float(mg.group(1))
    m = float(mm.group(1))
    return k, m, g


def evaluate_single(
    root: Path,
    image_path: Path,
    mode: str,
    k: float,
    m: float,
    g: float,
    tmp_dir: Path,
) -> EvalRow:
    bins = root / "code" / "bin"
    slic_bin = bins / "SLIC"
    sliccc_bin = bins / "slicCC"
    psnr_bin = bins / "PSNR"

    stem = image_path.stem
    k_str = f"{k:g}"
    m_str = f"{m:g}"
    g_str = f"{g:g}"

    out_clean = tmp_dir / f"{stem}_{mode}_k{k_str}_m{m_str}_g{g_str}_clean.ppm"
    out_cc = tmp_dir / f"{stem}_{mode}_k{k_str}_m{m_str}_g{g_str}_cc.ppm"

    if mode == "SLIC":
        cmd = [str(slic_bin), str(k), str(m), str(g), str(image_path), str(out_clean)]
    elif mode == "slicCC":
        cmd = [str(sliccc_bin), str(k), str(m), str(g), str(image_path), str(out_cc), str(out_clean)]
    else:
        raise ValueError(f"Mode non supporté: {mode}")

    run_command(cmd, cwd=root)

    psnr_proc = run_command([str(psnr_bin), str(image_path), str(out_clean)], cwd=root)
    psnr_db = parse_psnr_output(psnr_proc.stdout.strip())

    # PPM is raw (nearly fixed 24 bpp), so file size is not usable for RD curves.
    # Use PNG-encoded sizes as a content-dependent proxy for compression rate.
    orig_size = png_encoded_size_bytes(image_path)
    comp_size = png_encoded_size_bytes(out_clean)
    compression_rate = (orig_size / comp_size) if comp_size > 0 else 0.0

    width, height = read_ppm_dimensions(image_path)
    pixels = max(1, width * height)
    bpp = (8.0 * comp_size) / pixels

    return EvalRow(
        image=str(image_path.relative_to(root)),
        mode=mode,
        k=k,
        m=m,
        g=g,
        width=width,
        height=height,
        original_size_bytes=orig_size,
        compressed_size_bytes=comp_size,
        compression_rate=compression_rate,
        bpp=bpp,
        psnr_db=psnr_db,
        psnr_ok_30db=(psnr_db >= 30.0),
        output_image=str(out_clean.relative_to(root)),
    )


def evaluate_single_auto_optimal(
    root: Path,
    image_path: Path,
    mode: str,
    tmp_dir: Path,
) -> EvalRow:
    bins = root / "code" / "bin"
    auto_bin = bins / "autoSLIC"

    stem = image_path.stem
    auto_out = tmp_dir / f"{stem}_{mode}_auto.slic"

    auto_proc = run_command([str(auto_bin), mode, str(image_path), str(auto_out)], cwd=root)
    k, m, g = parse_autoslic_parameters(auto_proc.stdout)

    return evaluate_single(root, image_path, mode, k, m, g, tmp_dir)


def ensure_binaries_exist(root: Path, need_auto: bool = False) -> None:
    required = [
        root / "code" / "bin" / "SLIC",
        root / "code" / "bin" / "slicCC",
        root / "code" / "bin" / "PSNR",
    ]
    if need_auto:
        required.append(root / "code" / "bin" / "autoSLIC")
    missing = [p for p in required if not p.exists()]
    if missing:
        joined = "\n".join(str(p) for p in missing)
        raise FileNotFoundError(
            "Binaires requis manquants. Compilez-les d'abord (e.g., make code/bin/SLIC code/bin/slicCC code/bin/PSNR).\n"
            f"Manquants:\n{joined}"
        )


def write_full_csv(rows: Iterable[EvalRow], out_path: Path) -> None:
    fieldnames = [
        "image",
        "mode",
        "k",
        "m",
        "g",
        "width",
        "height",
        "original_size_bytes",
        "compressed_size_bytes",
        "compression_rate",
        "bpp",
        "psnr_db",
        "psnr_ok_30db",
        "output_image",
    ]
    with out_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row.__dict__)


def write_curve_csv(rows: Iterable[EvalRow], out_path: Path) -> None:
    sorted_rows = sorted(rows, key=lambda r: (r.image, r.mode, r.bpp, r.psnr_db))
    fieldnames = [
        "image",
        "mode",
        "bpp",
        "psnr_db",
        "compression_rate",
        "k",
        "m",
        "g",
        "psnr_ok_30db",
    ]
    with out_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in sorted_rows:
            writer.writerow(
                {
                    "image": row.image,
                    "mode": row.mode,
                    "bpp": row.bpp,
                    "psnr_db": row.psnr_db,
                    "compression_rate": row.compression_rate,
                    "k": row.k,
                    "m": row.m,
                    "g": row.g,
                    "psnr_ok_30db": row.psnr_ok_30db,
                }
            )


def write_summary_csv(rows: Iterable[EvalRow], out_path: Path) -> None:
    grouped = {}
    for r in rows:
        key = (r.mode, r.k, r.m, r.g)
        grouped.setdefault(key, []).append(r)

    fieldnames = [
        "mode",
        "k",
        "m",
        "g",
        "num_images",
        "avg_psnr_db",
        "min_psnr_db",
        "avg_compression_rate",
        "avg_bpp",
        "all_psnr_ge_30db",
    ]
    with out_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for (mode, k, m, g), lst in sorted(grouped.items(), key=lambda x: (x[0][0], x[0][1], x[0][2], x[0][3])):
            psnrs = [x.psnr_db for x in lst]
            rates = [x.compression_rate for x in lst]
            bpps = [x.bpp for x in lst]
            writer.writerow(
                {
                    "mode": mode,
                    "k": k,
                    "m": m,
                    "g": g,
                    "num_images": len(lst),
                    "avg_psnr_db": statistics.mean(psnrs),
                    "min_psnr_db": min(psnrs),
                    "avg_compression_rate": statistics.mean(rates),
                    "avg_bpp": statistics.mean(bpps),
                    "all_psnr_ge_30db": all(p >= 30.0 for p in psnrs),
                }
            )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Batch evaluation for SLIC and slicCC on a PPM dataset."
    )
    parser.add_argument(
        "--dataset",
        default="BDD",
        help="Dataset directory containing PPM images (recursive). Default: BDD",
    )
    parser.add_argument(
        "--output",
        default="evaluation_results",
        help="Directory where CSV outputs and generated images are written.",
    )
    parser.add_argument(
        "--modes",
        default="SLIC,slicCC",
        help="Modes to evaluate, comma-separated. Default: SLIC,slicCC",
    )
    parser.add_argument(
        "--k-values",
        default="100,200,400,800",
        help="Comma-separated K values. Default: 100,200,400,800",
    )
    parser.add_argument(
        "--m-values",
        default="10.0",
        help="Comma-separated m values. Default: 10.0",
    )
    parser.add_argument(
        "--g-values",
        default="2.0",
        help="Comma-separated g values. Default: 2.0",
    )
    parser.add_argument(
        "--keep-images",
        action="store_true",
        help="Keep generated reconstructed images in output/generated.",
    )
    parser.add_argument(
        "--auto-optimal",
        action="store_true",
        help="Use autoSLIC to find (K,m,g) per image/mode, then evaluate those optimal params.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = find_project_root()

    dataset_dir = (root / args.dataset).resolve()
    if not dataset_dir.exists():
        print(f"Dataset folder not found: {dataset_dir}", file=sys.stderr)
        print("Create it and add .ppm files, or use --dataset.", file=sys.stderr)
        return 1

    ensure_binaries_exist(root, need_auto=args.auto_optimal)

    modes = [x.strip() for x in args.modes.split(",") if x.strip()]
    valid_modes = {"SLIC", "slicCC"}
    for mode in modes:
        if mode not in valid_modes:
            print(f"Invalid mode: {mode}. Allowed: SLIC, slicCC", file=sys.stderr)
            return 1

    k_values: List[float] = []
    m_values: List[float] = []
    g_values: List[float] = []
    if not args.auto_optimal:
        k_values = parse_list_of_floats(args.k_values)
        m_values = parse_list_of_floats(args.m_values)
        g_values = parse_list_of_floats(args.g_values)

    images = sorted(dataset_dir.rglob("*.ppm"))
    if not images:
        print(f"No .ppm images found in {dataset_dir}", file=sys.stderr)
        return 1

    out_dir = (root / args.output).resolve()
    generated_dir = out_dir / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.auto_optimal:
        total = len(images) * len(modes)
    else:
        combinations = list(itertools.product(modes, k_values, m_values, g_values))
        total = len(images) * len(combinations)

    rows: List[EvalRow] = []
    done = 0

    for img in images:
        print(f"Processing image: {img.relative_to(root)}")
        if args.auto_optimal:
            for mode in modes:
                done += 1
                print(f"[{done}/{total}] mode={mode} auto=ON image={img.name}", flush=True)
                try:
                    row = evaluate_single_auto_optimal(root, img, mode, generated_dir)
                    rows.append(row)
                    print(
                        f"  -> optimal: k={row.k:g} m={row.m:g} g={row.g:g} | "
                        f"psnr={row.psnr_db:.4f} bpp={row.bpp:.4f}",
                        flush=True,
                    )
                except subprocess.CalledProcessError as e:
                    print("Command failed:", file=sys.stderr)
                    print(" ".join(map(str, e.cmd)), file=sys.stderr)
                    print(e.stderr or e.stdout or str(e), file=sys.stderr)
                except Exception as e:
                    print(f"Evaluation error for {img} mode={mode} auto=ON: {e}", file=sys.stderr)
        else:
            for mode, k, m, g in combinations:
                done += 1
                print(
                    f"[{done}/{total}] mode={mode} k={k:g} m={m:g} g={g:g} image={img.name}",
                    flush=True,
                )
                try:
                    row = evaluate_single(root, img, mode, k, m, g, generated_dir)
                    rows.append(row)
                except subprocess.CalledProcessError as e:
                    print("Command failed:", file=sys.stderr)
                    print(" ".join(map(str, e.cmd)), file=sys.stderr)
                    print(e.stderr or e.stdout or str(e), file=sys.stderr)
                except Exception as e:
                    print(f"Evaluation error for {img} mode={mode} k={k} m={m} g={g}: {e}", file=sys.stderr)

    if not rows:
        print("No successful evaluations.", file=sys.stderr)
        return 2

    full_csv = out_dir / "rd_full_results.csv"
    curve_csv = out_dir / "rd_curve_points.csv"
    summary_csv = out_dir / "rd_summary_by_params.csv"

    write_full_csv(rows, full_csv)
    write_curve_csv(rows, curve_csv)
    write_summary_csv(rows, summary_csv)

    if not args.keep_images:
        for ppm in generated_dir.glob("*.ppm"):
            try:
                ppm.unlink()
            except OSError:
                pass
        for slic in generated_dir.glob("*.slic"):
            try:
                slic.unlink()
            except OSError:
                pass

    print("\nDone.")
    print(f"Full results: {full_csv}")
    print(f"Curve points: {curve_csv}")
    print(f"Summary: {summary_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
