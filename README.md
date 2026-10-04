# NLA Challenge 1

**[SOLUTION.md](SOLUTION.md)** contains all 13 answers, task-by-task explanations,
solver settings, residual checks, a measured GMRES/BiCGSTAB comparison and links
to the PNGs to upload. Its **Requirements versus implementation choices**
section explains numerical, display, solver, code-style and workflow decisions
and remaining uncertainties. Everything was computed in the Docker container `amsc`.

**Submission solvers:** task 8 uses **LIS BiCGSTAB + ILU(0)**; task 12 uses
**Eigen BiCGSTAB + diagonal preconditioning**. Both met the required true-residual
tolerances and were faster than the tested GMRES alternatives on this image.
These are evidence-based choices, not a claim of optimality among all available solvers.

## Reproduce

From this folder on macOS:

```bash
bash run_docker.sh
```

This uses the same `amsc` container and `/u/sw/etc/bash.bashrc` startup file as
`~/.config/fish/config.fish`. It stages the source in a fresh directory under
`~/shared-folder`, loads `gcc-glibc/11.2.0` and `lis`, builds with compiler warnings,
runs the numerical/report tests, all tasks in order and the solver comparison,
then copies `results/` and the regenerated `SOLUTION.md` here. The temporary
build is removed.
The container is left running. No course notes/PDFs are modified and nothing is
submitted, committed or pushed.

### Run interactively inside the container

From **this Challenge1 folder on the host**, stage only the required files:

```bash
mkdir -p "$HOME/shared-folder/Challenge1"
cp Makefile challenge1.cpp image_filters.hpp test_challenge1.cpp \
   run_container.sh make_report.py comparison_report.py test_reports.py deer.jpg \
   "$HOME/shared-folder/Challenge1/"
cp -R third_party "$HOME/shared-folder/Challenge1/"
mkdir -p "$HOME/shared-folder/Challenge1/results/solver_comparison"
cp results/solver_comparison/compare.cpp results/solver_comparison/run_comparison.sh \
   "$HOME/shared-folder/Challenge1/results/solver_comparison/"
```

These commands overwrite same-named source files in that staging folder. In
fish, enter `amsc` using your alias. From another shell, use the equivalent:

```bash
docker start amsc >/dev/null
docker exec -it -w /shared-folder amsc /bin/bash --rcfile /u/sw/etc/bash.bashrc -i
```

Now **inside the container**:

```bash
cd /shared-folder/Challenge1
module load gcc-glibc/11.2.0 lis
bash run_container.sh
```

This builds the executables, runs tests and all 13 tasks in order, benchmarks
both solver alternatives three times each, then creates `SOLUTION.md`.
Python 3, Eigen and LIS must be available in the container.
Same-named result files are overwritten. The files are already visible on the
host at `~/shared-folder/Challenge1/`; they are not automatically copied to
this original folder by the interactive run. After `exit`, from the original
Challenge1 folder on the host, retrieve them with:

```bash
cp -R "$HOME/shared-folder/Challenge1/results" .
cp "$HOME/shared-folder/Challenge1/SOLUTION.md" .
```

Copying into an existing `results/` merges directories and overwrites matching
files; unrelated old files remain. The automatic `run_docker.sh` route handles
staging and retrieval for you instead.

### Run one task

After loading the modules, `make -j2` builds without running the full workflow;
`make test` runs the numerical and Python report-validation tests. For example,
inside the staged container folder:

```bash
./build/challenge1 5 deer.jpg results
```

Task 5 requires task 2's `results/data/w.mtx`. Dependencies are:

| Task | Prior outputs required |
|---|---|
| 1, 2, 4, 10 | Only `deer.jpg` |
| 3, 5 | `w.mtx` (task 2) |
| 6 | Only `deer.jpg`; writes `A2.mtx` for later tasks |
| 7 | `A2.mtx` (6), `v.mtx` (3) |
| 8 | `A2.mtx` (6), `w.mtx` (2); use LIS command below |
| 9 | `A2.mtx` (6), `w.mtx` (2), `x_lis.mtx` (8) |
| 11 | `v.mtx` (3) |
| 12 | `w.mtx` (2); writes `y_eigen.mtx` |
| 13 | `w.mtx` (2), `y_eigen.mtx` (12) |

Every C++ task loads `deer.jpg` for dimensions. Tasks 4/5 and 10/11/12/13
reconstruct operators when needed instead of saving large A1/A3 files.
Task 8 is not a C++ dispatcher option; run:

```bash
export OMP_NUM_THREADS=1
mkdir -p results/logs
./build/lis_test1 results/data/A2.mtx results/data/w.mtx \
  results/data/x_lis.mtx results/logs/lis_history.txt \
  -i bicgstab -p ilu -ilu_fill 0 -maxiter 2000 \
  -tol 1e-12 -conv_cond 0 -initx_zeros 1 -scale none -print mem \
  2>&1 | tee results/logs/task8_lis.log
```

After all task metrics/images exist, refresh the report with
`python3 make_report.py results SOLUTION.md`. After changing task 2's noise,
rerun all tasks depending on `w` rather than mixing old and new outputs.

### Compare GMRES and BiCGSTAB

The complete runner includes this comparison. To repeat it separately inside
`amsc`, after loading the modules and generating `A2.mtx` and `w.mtx`:

```bash
bash results/solver_comparison/run_comparison.sh
python3 make_report.py results SOLUTION.md
```

The script uses a temporary build, one OpenMP thread, the same inputs and three
runs per configuration. It checks solver success and recomputes every original
system residual; timings exclude assembly, I/O and verification. Dimensions come
from `deer.jpg`, not a fixed image size. The main solution vectors/images use
BiCGSTAB; rerunning the comparison alone does not overwrite them. Use the complete
runner to regenerate submission vectors, PNGs, counts and residuals together.

The report derives iteration counts, maximum true residuals and median times
from the logs. Saved input/source SHA-256 fingerprints prevent stale comparisons
being reused after a source, image, matrix or RHS change: rerun the comparison if
report generation rejects a mismatch. If comparison logs are absent, the report
states that no validated timing comparison is available instead of inventing one.
Fixed-order local timings are not universal speed guarantees.

## Files

| Path | Purpose |
|---|---|
| `challenge1.cpp` | Numbered task implementation, Eigen mathematics and diagnostics. |
| `image_filters.hpp` | Image I/O, flattening, noise, sparse assembly, vector I/O and Eigen solve. |
| `test_challenge1.cpp` | Numerical, indexing, format and image-layout tests. |
| `test_reports.py` | Eleven regression tests for comparisons, selected submission solver and rejection of invalid/stale logs. |
| `Makefile` | Builds the task executable, tests and LIS driver. |
| `run_docker.sh` | Host-side container runner and output retrieval. |
| `run_container.sh` | Container-side tests, task sequence and solver comparison. |
| `make_report.py` | Validates task metrics/PNG headers and generates the answer sheet; Python standard library only. |
| `comparison_report.py` | Validates comparison fingerprints/logs and renders measured benchmark results. |
| `third_party/` | Unmodified Lab1 stb headers and pre-existing LIS example `test1.c` driver. |
| `results/images/` | Eight PNGs: original, six requested outputs, and extra absolute-edge visualization. |
| `results/data/` | `A2.mtx`, `v.mtx`, `w.mtx`, `x_lis.mtx`, `y_eigen.mtx`. |
| `results/metrics/` | Machine-readable measurements per task. |
| `results/logs/` | Build/tests, task output, LIS residual history and environment/source hashes. |
| `results/solver_comparison/` | Benchmark source/script, three-run solver logs, environment and input fingerprints. |

The Matrix Market files are intentionally uncompressed for direct use by the
Eigen/LIS readers; compression is not prohibited by the assignment. `A2.mtx` is
about 78 MiB; do not mistake it for a small 3×3 kernel. Dense N×N operators are
never allocated.

Noise is deterministic (seed 2026). Numerical values are retained without
clipping; all required PNGs use clipping to 0–255 and nearest-integer rounding.
Solver counts depend on these conventions and the documented library versions.

## Source provenance

- `third_party/stb_image.h` and `stb_image_write.h`: copied unchanged from
  `../LABS/Lab1/`, with their original license information intact.
- `third_party/lis_test1.c`: copied unchanged from the pre-existing host file
  `~/shared-folder/test/test1.c` (visible as `/shared-folder/test/test1.c` inside
  `amsc`). It is LIS's example driver, not code written for this challenge.
  `../LABS/Lab2/Lab2a_BasicLis.md`, steps 3–5, instructs students to obtain LIS's
  `test/` folder and compile `test1.c`. The exact release originally downloaded
  for the local copy is not verified; the linked library is LIS 2.0.30.
  The original copyright and license are retained. Byte identity was verified
  with `cmp`; both files have SHA-256
  `e5e689f9de4e98d8c84030c3663e05b304b9a389d37b54e970890c2a87178596`.
- Context: the challenge, `../Lectures/`, `../Transcripts/`, `../LABS/`, and the
  combined `../../NA_Notes.tex`, `../../NA_Chapters/` and `../../NA_Notes.pdf`.
  Only relevant image/filter, sparse-format and solver passages are used; unrelated
  lectures are not attributed to this challenge.

For submission, use the answer table in `SOLUTION.md`. The lab transcript asks
for answers/images in Microsoft Forms and implementation files in the separate
course upload folder. This solution does not submit either for you.
