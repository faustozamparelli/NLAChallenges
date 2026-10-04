#!/usr/bin/env python3
"""Generate the final answer sheet from saved task measurements."""
import math
from pathlib import Path
import re
import struct
import sys


def one_match(pattern, text):
    matches = re.findall(pattern, text, re.MULTILINE)
    if len(matches) != 1:
        raise ValueError(f"Missing or duplicate LIS diagnostic: {pattern}")
    return matches[0]


def parse_lis_diagnostics(log):
    if one_match(r"^linear solver status\s*:\s*(.+)$", log) != "normal end":
        raise ValueError("LIS did not converge")
    if (one_match(r"^linear solver\s*:\s*(\S+)", log) != "BiCGSTAB"
            or one_match(r"^preconditioner\s*:\s*(\S+)", log) != "ILU(0)"):
        raise ValueError("Expected LIS BiCGSTAB with ILU(0)")
    iterations = int(one_match(r"BiCGSTAB: number of iterations\s*=\s*(\d+)", log))
    residual = float(one_match(r"BiCGSTAB: relative residual\s*=\s*(\S+)", log))
    return {"solver": "BiCGSTAB", "preconditioner": "ILU(0)", "tolerance": "1e-12",
            "iterations": str(iterations), "solver_relative_residual": f"{residual:.6e}"}


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit("Usage: make_report.py RESULTS_DIRECTORY REPORT_PATH [LIS_LOG_PATH]")
    root, report = Path(sys.argv[1]), Path(sys.argv[2])
    tasks = {}
    for number in range(1, 14):
        if number == 8 and len(sys.argv) == 4:
            continue
        lines = (root / "metrics" / f"task{number}.txt").read_text().splitlines()
        tasks[number] = dict(line.split("=", 1) for line in lines)
    if len(sys.argv) == 4:
        tasks[8] = parse_lis_diagnostics(Path(sys.argv[3]).read_text())
        for key in ("relative_residual", "absolute_residual"):
            tasks[8][key] = tasks[9][key]

    for number, preconditioner, tolerance in ((8, "ILU(0)", 1e-12),
                                             (12, "DiagonalPreconditioner", 1e-10)):
        task = tasks[number]
        if (task["solver"] != "BiCGSTAB" or task["preconditioner"] != preconditioner
                or not 1 <= int(task["iterations"]) <= 2000):
            raise ValueError(f"Unexpected solver configuration for task {number}")
        for key in ("solver_relative_residual", "relative_residual"):
            value = float(task[key])
            if not math.isfinite(value) or not 0 <= value <= tolerance:
                raise ValueError(f"Task {number} residual exceeds tolerance")
    for number, key, tolerance in ((9, "relative_residual", 1e-12),
                                   (13, "imported_relative_residual", 1e-10)):
        value = float(tasks[number][key])
        if not math.isfinite(value) or not 0 <= value <= tolerance:
            raise ValueError(f"Task {number} saved solution exceeds tolerance")

    m, n, N = (int(tasks[1][key]) for key in ("height_m", "width_n", "pixels_N"))
    if (m <= 0 or n <= 0 or N != m * n or int(tasks[3]["v_components"]) != N
            or int(tasks[3]["w_components"]) != N or int(tasks[13]["imported_components"]) != N):
        raise ValueError("Incorrect image/vector dimensions")
    counts = ((4, "A1_nonzeros", (3*m-2)*(3*n-2)),
              (6, "A2_nonzeros", 5*m*n-2*m-2*n),
              (10, "A3_nonzeros", 2*(n-1)*(3*m-2)))
    for number, key, expected in counts:
        if int(tasks[number][key]) != expected:
            raise ValueError(f"Incorrect nonzero count for task {number}")
    if (tasks[6]["A2_symmetric"] != "false" or tasks[10]["A3_symmetric"] != "false"
            or float(tasks[10]["A3_transpose_sum_frobenius"]) != 0):
        raise ValueError("Unexpected operator symmetry")
    rhs_norm = float(tasks[3]["w_euclidean_norm"])
    if not math.isfinite(rhs_norm) or rhs_norm <= 0:
        raise ValueError("Invalid right-hand-side norm")
    for number in (8, 9, 12):
        absolute, relative = (float(tasks[number][key]) for key in
                              ("absolute_residual", "relative_residual"))
        if (not math.isfinite(absolute) or absolute < 0
                or not math.isclose(absolute, relative * rhs_norm, rel_tol=1e-12)):
            raise ValueError("Inconsistent absolute and relative residuals")

    image_names = ("task02_noisy.png", "task05_smoothed.png", "task07_sharpened.png",
                   "task09_lis_solution.png", "task11_edges.png", "task13_eigen_solution.png")
    for name in image_names:
        with (root / "images" / name).open("rb") as stream:
            header = stream.read(33)
        if (len(header) != 33 or header[:8] != b"\x89PNG\r\n\x1a\n"
                or header[12:16] != b"IHDR" or struct.unpack(">II", header[16:24]) != (n, m)
                or header[24:26] != b"\x08\x00"):
            raise ValueError(f"Invalid PNG dimensions or grayscale format: {name}")

    if len(sys.argv) == 4:
        (root / "metrics" / "task8.txt").write_text(
            "".join(f"{key}={value}\n" for key, value in tasks[8].items()))
    report.write_text(f"""# Challenge 1 — Final results

The supplied image has {m} × {n} pixels. Sparse operators use row-wise
indexing and zero padding, with the assigned kernels applied without flipping.

| Task | Answer / output |
|---|---|
| 1 | Matrix size: **{m} × {n}** (height × width). |
| 2 | [Noisy image](results/images/task02_noisy.png). |
| 3 | Both vectors have **{N}** components; **‖v‖₂ = {tasks[3]['v_euclidean_norm']}**. |
| 4 | **{tasks[4]['A1_nonzeros']}** nonzeros in A1. |
| 5 | [Smoothed noisy image](results/images/task05_smoothed.png). |
| 6 | **{tasks[6]['A2_nonzeros']}** nonzeros in A2; **not symmetric**. |
| 7 | [Sharpened original image](results/images/task07_sharpened.png). |
| 8 | LIS BiCGSTAB + ILU(0): **{tasks[8]['iterations']} iterations**, final relative residual **{tasks[8]['solver_relative_residual']}**. |
| 9 | [LIS solution image](results/images/task09_lis_solution.png). |
| 10 | A3 is **not symmetric**; it is **skew-symmetric**. |
| 11 | [Edge-filtered original image](results/images/task11_edges.png). |
| 12 | Eigen BiCGSTAB + diagonal preconditioning: **{tasks[12]['iterations']} iterations**, final relative residual **{float(tasks[12]['solver_relative_residual']):.15e}**. |
| 13 | [Eigen solution image](results/images/task13_eigen_solution.png). |

The forward filters compute **A1 w**, **A2 v** and **A3 v**. The inverse
systems are **A2 x = w** (LIS, tolerance 1e−12) and **(4I + A3)y = w**
(Eigen, tolerance 1e−10). Both solvers start from zero and allow 2000 iterations.

Residuals are relative Euclidean norms, ‖b − Ax‖₂ / ‖b‖₂. The saved
solutions satisfy the tolerances when checked against the original systems:

| System | Checked relative residual | Absolute residual |
|---|---:|---:|
| A2 x = w | {tasks[9]['relative_residual']} | {tasks[9]['absolute_residual']} |
| (4I + A3)y = w | {tasks[12]['relative_residual']} | {tasks[12]['absolute_residual']} |

Noise is uniform in [−50, 50], with seed **{tasks[2]['seed']}**. Numerical
vectors retain unclipped double-precision values; exported PNGs clip to [0, 255]
and round to the nearest integer. Negative edge responses therefore appear black.
The task-13 image retains its computed intensity scale.

Matrix Market files are in `results/data/`, task measurements in
`results/metrics/`, and the six requested images in `results/images/`.
Run `bash run_docker.sh` to regenerate the results and this answer sheet.
""")


if __name__ == "__main__":
    main()
