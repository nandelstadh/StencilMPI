#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_rows(csv_path: Path) -> list[dict[str, float]]:
    with csv_path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        expected = {"n", "p", "time"}
        if reader.fieldnames is None or set(reader.fieldnames) != expected:
            raise ValueError(
                f"{csv_path} must contain exactly these columns: n,p,time"
            )

        rows: list[dict[str, float]] = []
        for row in reader:
            rows.append(
                {
                    "n": float(row["n"]),
                    "p": float(row["p"]),
                    "time": float(row["time"]),
                }
            )

    if not rows:
        raise ValueError(f"{csv_path} is empty")

    rows.sort(key=lambda r: r["p"])
    return rows


def infer_scaling_type(rows: list[dict[str, float]]) -> str:
    n_values = {row["n"] for row in rows}
    return "strong" if len(n_values) == 1 else "weak"


def pick_datasets(input_dir: Path) -> tuple[Path, Path]:
    csv_files = sorted(input_dir.glob("*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {input_dir}")

    by_name: dict[str, Path] = {}
    by_inference: dict[str, Path] = {}

    for csv_file in csv_files:
        stem = csv_file.stem.lower()
        if "strong" in stem and "strong" not in by_name:
            by_name["strong"] = csv_file
        if "weak" in stem and "weak" not in by_name:
            by_name["weak"] = csv_file

        inferred = infer_scaling_type(load_rows(csv_file))
        if inferred not in by_inference:
            by_inference[inferred] = csv_file

    strong_csv = by_inference.get("strong") or by_name.get("strong")
    weak_csv = by_inference.get("weak") or by_name.get("weak")

    if strong_csv is None or weak_csv is None:
        raise ValueError(
            "Could not identify both strong and weak scaling datasets from CSV files."
        )

    return strong_csv, weak_csv


def plot_strong_scaling(rows: list[dict[str, float]], output_path: Path) -> None:
    processes = [row["p"] for row in rows]
    runtimes = [row["time"] for row in rows]
    p0 = processes[0]
    t0 = runtimes[0]

    measured_speedup = [t0 / t for t in runtimes]
    ideal_speedup = [p / p0 for p in processes]

    plt.figure(figsize=(8, 5))
    plt.plot(processes, measured_speedup, "o-", linewidth=2, label="Measured speedup")
    plt.plot(processes, ideal_speedup, "--", linewidth=2, label="Ideal scaling")
    plt.xlabel("Processes (p)")
    plt.ylabel("Speedup")
    plt.title("Strong scaling")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def plot_weak_scaling(rows: list[dict[str, float]], output_path: Path) -> None:
    processes = [row["p"] for row in rows]
    runtimes = [row["time"] for row in rows]
    t0 = runtimes[0]
    ideal_runtime = [t0 for _ in processes]

    plt.figure(figsize=(8, 5))
    plt.plot(processes, runtimes, "o-", linewidth=2, label="Measured runtime")
    plt.plot(processes, ideal_runtime, "--", linewidth=2, label="Ideal weak scaling")
    plt.xlabel("Processes (p)")
    plt.ylabel("Runtime (s)")
    plt.title("Weak scaling")
    if t0 > 0:
        plt.ylim(0.8 * t0, 1.2 * t0)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot strong and weak scaling results from CSV files."
    )
    parser.add_argument(
        "--input-dir",
        default="PelleResults",
        type=Path,
        help="Directory containing CSV files (default: PelleResults)",
    )
    parser.add_argument(
        "--output-dir",
        default="Plots",
        type=Path,
        help="Directory where plots are written (default: Plots)",
    )
    args = parser.parse_args()

    strong_csv, weak_csv = pick_datasets(args.input_dir)
    strong_rows = load_rows(strong_csv)
    weak_rows = load_rows(weak_csv)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    plot_strong_scaling(strong_rows, args.output_dir / "strong_scaling.png")
    plot_weak_scaling(weak_rows, args.output_dir / "weak_scaling.png")

    print(f"Strong scaling data: {strong_csv}")
    print(f"Weak scaling data:   {weak_csv}")
    print(f"Saved plots in:      {args.output_dir}")


if __name__ == "__main__":
    main()
