"""Validate saved solver comparisons and render their measured Markdown summary."""
from dataclasses import dataclass
import hashlib
import math
from pathlib import Path
import re
import statistics


@dataclass(frozen=True)
class Measurement:
    iterations: int
    seconds: float
    reported: float
    relative: float


def finite_number(value, name, *, positive=False):
    number = float(value)
    if not math.isfinite(number) or number < 0 or (positive and number == 0):
        raise ValueError(f"Invalid {name}: {value}")
    return number


def checked_measurement(iterations, seconds, reported, relative, tolerance):
    measurement = Measurement(int(iterations), finite_number(seconds, "solver time", positive=True),
                              finite_number(reported, "reported residual"),
                              finite_number(relative, "true residual"))
    if not 1 <= measurement.iterations <= 2000 or max(measurement.reported, measurement.relative) > tolerance:
        raise ValueError("Comparison did not meet its iteration/residual requirements")
    return measurement


def validate_hashes(root):
    comparison = root / "solver_comparison"
    expected = {"deer.jpg", "results/data/A2.mtx", "results/data/w.mtx", "image_filters.hpp",
                "results/solver_comparison/compare.cpp", "results/solver_comparison/run_comparison.sh",
                "third_party/lis_test1.c",
                "third_party/stb_image.h", "third_party/stb_image_write.h"}
    seen = set()
    for line in (comparison / "input_hashes.txt").read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        if name not in expected or name in seen:
            raise ValueError(f"Unexpected/duplicate comparison fingerprint: {name}")
        seen.add(name)
        # Benchmark paths are recorded relative to the project, not the results folder.
        path = root.parent / name
        hasher = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                hasher.update(chunk)
        actual = hasher.hexdigest()
        if actual != digest:
            raise ValueError(f"Stale comparison input/source: {name}; rerun the comparison")
    if seen != expected:
        raise ValueError("Incomplete comparison input fingerprints")


def one_match(pattern, text):
    matches = re.findall(pattern, text, flags=re.MULTILINE)
    if len(matches) != 1:
        raise ValueError(f"Missing/duplicate comparison diagnostic: {pattern}")
    return matches[0]


def load_measurements(root):
    validate_hashes(root)
    comparison = root / "solver_comparison"
    measurements = {}
    for method, label in (("gmres", "GMRES"), ("bicgstab", "BiCGSTAB")):
        runs = []
        for trial in range(1, 4):
            text = (comparison / f"lis_{method}_{trial}.log").read_text()
            if "linear solver status  : normal end" not in text:
                raise ValueError("LIS comparison did not report successful convergence")
            if (one_match(r"linear solver\s*:\s*(\S+)", text) != label
                    or one_match(r"preconditioner\s*:\s*(\S+)", text) != "ILU(0)"
                    or one_match(r"^number of threads\s*=\s*(\d+)", text) != "1"):
                raise ValueError("Unexpected LIS method, preconditioner or thread count")
            runs.append(checked_measurement(
                one_match(rf"{label}: number of iterations\s*=\s*(\d+)", text),
                one_match(rf"{label}: elapsed time\s*=\s*(\S+)", text),
                one_match(rf"{label}: relative residual\s*=\s*(\S+)", text),
                one_match(r"true_relative=(\S+)", text), 1e-12))
        measurements[method] = runs

    eigen = {"GMRES60_identity": {}, "BiCGSTAB_diagonal": {}}
    for line in (comparison / "eigen_comparison.log").read_text().splitlines():
        label, *tokens = line.split()
        if label not in eigen:
            raise ValueError(f"Unexpected Eigen comparison method: {label}")
        values = dict(token.split("=", 1) for token in tokens)
        if len(values) != len(tokens) or set(values) != {
                "trial", "iterations", "status", "seconds", "reported", "true_relative"}:
            raise ValueError("Invalid Eigen comparison diagnostics")
        trial = int(values["trial"])
        if values["status"] != "0" or trial not in range(1, 4) or trial in eigen[label]:
            raise ValueError("Failed/duplicate Eigen comparison trial")
        eigen[label][trial] = checked_measurement(values["iterations"], values["seconds"],
                                                  values["reported"], values["true_relative"], 1e-10)
    for label, runs in eigen.items():
        if set(runs) != {1, 2, 3}:
            raise ValueError("Incomplete Eigen comparison trials")
        measurements[label] = [runs[trial] for trial in range(1, 4)]
    return measurements


def render_comparison(root):
    comparison = root / "solver_comparison"
    if not (comparison / "eigen_comparison.log").exists():
        return ("## Controlled comparison: BiCGSTAB versus GMRES\n\n"
                "No validated comparison logs are available for this run. Run\n"
                "`bash results/solver_comparison/run_comparison.sh` inside `amsc`,\n"
                "then regenerate the report; no speed ranking is asserted here.\n")
    measurements = load_measurements(root)
    rows = []
    configs = ((8, "gmres", "LIS GMRES(40) + ILU(0), comparison alternative"),
               (8, "bicgstab", "LIS BiCGSTAB + ILU(0), submission choice"),
               (12, "GMRES60_identity", "Eigen GMRES(60) + identity, comparison alternative"),
               (12, "BiCGSTAB_diagonal", "Eigen BiCGSTAB + diagonal, submission choice"))
    medians = {}
    for task, key, label in configs:
        runs = measurements[key]
        counts = sorted({run.iterations for run in runs})
        count_text = str(counts[0]) if len(counts) == 1 else f"{counts[0]}–{counts[-1]}"
        medians[key] = statistics.median(run.seconds for run in runs)
        rows.append(f"| {task} | {label} | {count_text} | "
                    f"{max(run.relative for run in runs):.6e} | {medians[key]:.3f} s |")
    table = "\n".join(rows)
    ratio8 = medians["gmres"] / medians["bicgstab"]
    ratio12 = medians["GMRES60_identity"] / medians["BiCGSTAB_diagonal"]
    if ratio8 > 1 and ratio12 > 1:
        conclusion = ("The selected BiCGSTAB configurations were faster for both tested systems,\n"
                      "while all methods met the prescribed true-residual tolerances.\n")
    else:
        conclusion = ("The timing comparison does not favor BiCGSTAB on both tasks.\n"
                      "Use the measured table rather than assuming a universal speed ranking.\n")
    return f"""## Controlled comparison: BiCGSTAB versus GMRES

Both alternatives were tested in the same `amsc` container using the same saved
A2 and w, the same reconstructed task-12 matrix, double precision, zero initial
guesses, a 2000-iteration limit and one OpenMP thread. LIS uses ILU(0), no
scaling and `-conv_cond 0` for both methods. Eigen compares GMRES(60)/identity
against BiCGSTAB/diagonal. Each configuration was run three times; all runs
reported success and passed independently recomputed original-system residual
checks. This table is generated from validated logs, not fixed example numbers;
input/source hashes prevent results from different data being silently reused.

| Task | Method | Reported iterations | Maximum true relative residual over 3 runs | Median solver time |
|---|---|---:|---:|---:|
{table}

**Measured conclusion:** {conclusion}
The median GMRES/BiCGSTAB time ratios are **{ratio8:.2f}** for task 8 and
**{ratio12:.2f}** for task 12 (a value above one favors BiCGSTAB).
BiCGSTAB uses O(N) working-vector storage rather than GMRES's O(kN) basis
storage for restart length k. Smaller final residuals reflect tolerance
overshoot; they do not demonstrate universally greater accuracy or visually
better denoising. GMRES remains defensible for its residual-minimizing
construction, but that does not imply a speed advantage.

LIS times are the driver's elapsed solver times, including preconditioner
setup; Eigen times include `compute` and `solve`. Neither includes image
loading, matrix assembly, disk I/O or residual verification. Timings are local
observations, not portable guarantees. Configurations run in fixed order rather
than a randomized performance study; compare methods within each library, not
Eigen against LIS. Iteration counts have different per-step costs.
The submission answer table, vectors and PNGs use **BiCGSTAB/ILU(0)** for task 8
and **BiCGSTAB/diagonal** for task 12. They are regenerated by the complete runner,
not copied from comparison trials. Both selections are supported by convergence,
verified original-system residuals, measured runtime and short-recurrence storage;
this comparison does not establish optimality among all available solvers.

Logs, input fingerprints, environment information, comparison source and the
rerun script are in `results/solver_comparison/`. Inside `amsc`, with this whole
project in the shared folder, rerun and refresh the report with:

```bash
module load gcc-glibc/11.2.0 lis
bash results/solver_comparison/run_comparison.sh
python3 make_report.py results SOLUTION.md
```
"""
