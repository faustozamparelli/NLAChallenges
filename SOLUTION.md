# Challenge 1 — Image filtering and denoising

All 13 tasks were computed in Docker `amsc`, using Eigen 3.4.0 and LIS 2.0.30.
The results below use the supplied `deer.jpg`, not a resized image.

## Answers for Microsoft Forms

| Task | Answer / file to upload |
|---|---|
| 1 | Matrix size **656 × 656** (height × width). |
| 2 | [Noisy image](results/images/task02_noisy.png). |
| 3 | Both vectors have **430336** components; **‖v‖₂ = 41415.341203471929**. |
| 4 | **3865156** nonzeros in A1. |
| 5 | [Smoothed noisy image](results/images/task05_smoothed.png). |
| 6 | **2149056** nonzeros in A2; **not symmetric**. |
| 7 | [Sharpened original image](results/images/task07_sharpened.png). |
| 8 | LIS BiCGSTAB + ILU(0): **17 iterations**, reported final relative residual **4.259715e-13**. |
| 9 | [LIS solution image](results/images/task09_lis_solution.png). |
| 10 | A3 is **not symmetric**; it is **skew-symmetric**. |
| 11 | [Edge-filtered original image](results/images/task11_edges.png). |
| 12 | Eigen BiCGSTAB + diagonal: **52 iterations**, reported final relative residual **2.331624911239061e-11**. |
| 13 | [Eigen solution image](results/images/task13_eigen_solution.png). |

For tasks 8 and 12, “residual” above means the relative Euclidean residual.
Absolute residuals and independent checks are given below.

## Requirements versus implementation choices

The challenge fixes the supplied image, the 0–255 intensity scale, the three
assigned kernels, the products **A1 w**, **A2 v**, **A3 v**, the two inverse
systems, the solver libraries, the tolerances and the requested file types.
Its sliding-weight formula fixes **unflipped kernels and zero padding**; these
are not optional conventions. Border renormalization would change the operator.
The choices below are implementation decisions, not additional course requirements.

### Numerical and display choices

| Choice | Reason and consequence |
|---|---|
| Load one grayscale channel with the Lab1 stb headers; do not resize | Matches the supplied grayscale image and avoids adding an image-processing dependency. Index the returned buffer with one byte per pixel, independently of the original channel count. |
| Use double-precision Eigen matrices/vectors | Retains fractional noise and filter values and supports the tight solver tolerances; bytes are used only for PNG output. |
| Flatten row-wise with zero-based `p = i*n+j` | Makes neighbor offsets easy to audit. Column-wise flattening would also work if used consistently for every operator and reshape. Explicit loops avoid accidentally using Eigen's default column-major memory layout. |
| Uniform pseudorandom fluctuations, `std::mt19937`, seed **2026** | The challenge gives bounds, not a distribution or seed. Map each 32-bit generator output into [−50,50] explicitly to avoid library-dependent distribution algorithms. Visit pixels row-wise and reuse the same `w` throughout, making runs reproducible. |
| Keep `w = v + noise` unclipped and unrounded | Preserves additive noise in the numerical problem. Its range is [-34.528846163192959, 304.95325254275303]. Clipping `w` would change both linear systems and their answers. |
| Clip PNG copies to [0,255], then round to the nearest integer | PNG pixels must be representable bytes. Saturation avoids invalid out-of-range conversions; rounding reduces quantization error compared with truncation. `std::lround` rounds half-values upward here because values are nonnegative after clipping. |
| Do not normalize brightness or take absolute values in required PNGs | Preserves the numerical intensity scale. Signed edge responses below zero display as black, and the task-13 image is naturally dark. An extra absolute-edge PNG shows both polarities but is not a replacement answer or a solver input. |

### Sparse algebra and solver choices

| Choice | Reason and consequence |
|---|---|
| Compressed row-major sparse matrices assembled from triplets | Keeps storage proportional to the nonzero count, not N², and groups output-pixel coefficients by row. Triplets avoid costly repeated insertion. Reserve up to nine coefficients per pixel and omit exact zero weights. |
| Form `4I + A3` using bulk sparse addition | Avoids costly movement/reallocation from inserting a missing diagonal one entry at a time into compressed storage. |
| LIS BiCGSTAB with ILU(0) | Suitable for nonsymmetric A2. ILU(0) approximates neighbor coupling without adding fill. This tested configuration met tolerance and was faster than GMRES(40)/ILU(0), with short recurrences instead of a stored Krylov basis. This is a supported submission choice, not a claim of global optimality. |
| Eigen BiCGSTAB with diagonal preconditioning | Suitable for nonsymmetric `4I+A3`. The tested configuration met tolerance and was faster than GMRES(60)/identity, using O(N) working-vector storage. Here the diagonal is `4I`, so preconditioning is only scalar scaling, not a condition-number improvement. No Eigen preconditioner is prescribed. |
| Zero initial guesses, at most 2000 iterations, no LIS scaling | Makes initial-residual normalization equivalent to RHS normalization and provides a finite failure limit. These settings are chosen, not assigned. |
| LIS `-conv_cond 0` and independently checked relative residuals | With zero initial guess and no scaling, this condition uses `‖w-Ax‖₂/‖w‖₂`. Both absolute and relative residuals are reported to remove ambiguity; the imported solution is checked independently. |
| One MPI process and `OMP_NUM_THREADS=1` | Runs the MPI-enabled driver without `mpirun` and avoids parallel overhead for this workflow. This is not a parallel-performance study; thread/process choices can affect timings and floating-point results. |

### File formats, code style and workflow choices

- Use Eigen's general-coordinate Matrix Market writer for A2, the LIS indexed
  vector-coordinate extension for `v`, `w` and `x`, and Eigen's array-format
  vector writer/reader for `y`. This follows the actual reader interfaces;
  a `.mtx` suffix alone does not guarantee interchangeable vector formats.
  Custom LIS-vector output uses 17-digit precision, and solver residuals are
  checked again after importing the saved solutions.
- Export A2 for LIS as required; rebuild A1/A3 when needed instead of writing
  large unrequested matrix files. Persist intermediate vectors so numbered
  tasks can be rerun separately. Files are uncompressed for direct reader use.
- Keep reusable functions in `image_filters.hpp`, the numbered task dispatcher
  in `challenge1.cpp`, tests in a separate executable, and shared types in a
  `challenge` namespace. `inline` header functions and one translation unit per
  executable keep this small project simple. The stb implementation macros
  must not be included in multiple translation units of the same executable.
- Use C++17 for filesystem paths and `std::clamp`, RAII to free image memory,
  and exceptions caught at the executable boundary to report errors with a
  nonzero exit status. Reject malformed vector files rather than silently
  accepting incomplete, duplicate or nonfinite data.
- Build with a Makefile, `-O2` and warning flags; treat unmodified third-party
  headers as system includes. MPI/OpenMP flags match the installed LIS library,
  and an rpath locates that library at runtime. These are build choices, not
  mathematical assumptions.
- Keep images, numerical data, metrics and logs in separate `results/`
  subfolders with task-numbered names. Save the original PNG and absolute-edge
  PNG as extras. Record environment versions and source hashes for provenance.
- Use Bash runners with fail-fast/pipe-failure handling and a Python-standard-
  library report generator. The host runner uses a temporary shared-folder
  build, removes it on exit, leaves `amsc` running and copies successful outputs
  back. Reruns overwrite same-named outputs; they do not submit anything.
- Validate with an independent stencil, small-shape and manufactured-solution
  tests, and recomputed original-system residuals. The stencil comparison allows
  `1e-11 * max(1, max|reference|)` for roundoff (observed difference: zero).
  Tests and residual checks supplement rather than replace the assigned sparse
  products. A zero RHS returns the exact zero Eigen solution without dividing
  by zero. Small residuals alone do not measure visual denoising quality.

### Uncertainties and limits

1. **Noise and display conventions:** the challenge does not specify the noise
   distribution/seed, whether numerical noise should be clipped, or how signed
   or out-of-range filter outputs should be mapped to PNG bytes. Lab1 provides
   byte conversion and clipping examples, but does not settle all these choices
   for this challenge. The conventions above are explicit; they are not claimed
   to be uniquely intended by the lecturer. Figure 1 explicitly is not a set of
   task solutions, so it cannot resolve these ambiguities.
2. **“Final residual”:** the task text does not state absolute versus relative
   normalization. The answer table labels relative Euclidean residuals; the
   task details give absolute values as well. Solver counts depend on the
   chosen noisy RHS, solver, preconditioner, initial guess and library version;
   they are not universal answers independent of those settings.
3. **Driver provenance versus installed library:** `third_party/lis_test1.c`
   is a byte-identical copy of the pre-existing `~/shared-folder/test/test1.c`,
   not newly written code. Lab2a instructs students to use LIS's distributed
   `test/test1.c`. The local copy's exact downloaded release is not established
   by its name or copyright; LIS **2.0.30** is the linked library version,
   not a proven source-release identifier for that driver.
4. **Scope:** the implemented choices satisfy the measured tolerances. A
   controlled comparison with GMRES supports the BiCGSTAB selections below, but no exhaustive
   solver benchmark or independent confirmation of the lecturer's preferred
   display conventions has been performed. The linked Microsoft
   Forms fields have not been checked for extra wording beyond the local PDF.
   Nothing is submitted automatically.

## Task-by-task solution

### 1. Load the image

`stbi_load(..., 1)` reads grayscale pixels into `Eigen::MatrixXd`.
The result is **m = 656, n = 656**, so N = mn = **430336**.
The original intensity range is [6, 255].

### 2. Add noise

For every pixel, set `W(i,j) = F(i,j) + noise(i,j)`.
The observed fluctuations range from -49.999962840229266 to
49.999909568577976, within the prescribed interval.
The displayed result is `results/images/task02_noisy.png`; the unrounded numerical
right-hand side is preserved in `results/data/w.mtx`.

### 3. Reshape into vectors

Explicit index loops avoid confusing Eigen's default column-major storage with
row-wise mathematical flattening. Both `v` and `w` have **430336** entries.
Eigen's vector `.norm()` gives **‖v‖₂ = 41415.341203471929**.
Reshaping `v` back into an image reproduces the original matrix exactly.

### 4. Construct the smoothing operator A1

Use

```text
Hav1 = (1/12) * [ 1  1  1
                  1  4  1
                  1  1  1 ]
```

For output pixel `(i,j)` and a valid neighbor `(r,s) = (i+a-1,j+b-1)`, insert
`A1(i*n+j, r*n+s) = Hav1(a,b)` using sparse triplets.
The operator has shape **430336 × 430336**, not 3 × 3.

Counting valid offsets in each coordinate gives
`nnz(A1) = (3m−2)(3n−2) = 3865156`.
Equivalently, for this image: 9 entries per interior row, 6 per edge row,
and 4 per corner row. No zero-valued coefficients are stored.

### 5. Smooth the noisy image

Compute **A1 w**, not A1 v, then reshape using the same indexing.
The maximum absolute difference from the independent stencil is
**0**.
Output: `results/images/task05_smoothed.png`.
Zero padding can darken borders; this is part of the assigned operator.

### 6. Construct the sharpening operator A2

Use

```text
Hsh1 = [  0  −3   0
         −1   9  −3
          0  −1   0 ]
```

The same triplet rule produces an N × N sparse matrix. It has its diagonal,
two horizontal offsets and two vertical offsets, hence
`nnz(A2) = mn + 2m(n−1) + 2n(m−1) = 5mn−2m−2n = 2149056`.

**A2 is not symmetric:** a pixel assigns weight −3 to its right neighbor,
while that neighbor assigns weight −1 back to it. Numerically,
`‖A2−A2ᵀ‖F = 2621.9992372233824`.
The diagonal weight 9 exceeds the sum of absolute off-diagonal weights,
which is at most 8. Thus A2 is strictly row diagonally dominant and nonsingular.

### 7. Sharpen the original image

Compute **A2 v**, not A2 w. The sparse product agrees exactly with the
independent stencil. Its raw range is [-917,
1345]; clipping is applied only to the exported PNG.
Output: `results/images/task07_sharpened.png`.

### 8. Solve A2 x = w with LIS

Eigen exports the full general-coordinate matrix to `results/data/A2.mtx`.
The noisy right-hand side is in `results/data/w.mtx`. Use the unmodified
LIS `test1.c` example driver used in the course workflow, copied from
`~/shared-folder/test/test1.c`, with **BiCGSTAB and ILU(0)**:

```bash
./build/lis_test1 results/data/A2.mtx results/data/w.mtx \
  results/data/x_lis.mtx results/logs/lis_history.txt \
  -i bicgstab -p ilu -ilu_fill 0 -maxiter 2000 \
  -tol 1e-12 -conv_cond 0 -initx_zeros 1 -scale none -print mem
```

**Why this method was chosen:** A2 is nonsymmetric, so ordinary CG is not
appropriate. BiCGSTAB handles nonsymmetric systems without requiring A2ᵀ and
uses short recurrences, avoiding GMRES's Krylov-basis storage and Arnoldi
orthogonalization. In the controlled three-run comparison below, BiCGSTAB/ILU(0)
met the same tolerance and was faster than GMRES(40)/ILU(0). We therefore use it
for the submission, rather than selecting a solver by popularity or iteration
count alone. The submitted run reaches tolerance in **17** iterations.

ILU(0) retains the original sparsity pattern in its incomplete factors while
accounting approximately for off-diagonal neighbor coupling. In contrast,
Jacobi would only scale every row by the same factor 1/9 and would not improve
the condition number. Thus the substantive preconditioning choice is ILU(0),
not merely using any preconditioner.

GMRES remains a valid alternative: it minimizes the residual in its current
Krylov space for the applicable preconditioned formulation, but pays for
orthogonalization and basis storage. BiCGSTAB may have irregular residuals or
breakdown, so library success alone is not our acceptance criterion: the saved
solution must also satisfy an independently recomputed original-system residual.
A BiCGSTAB iteration generally involves two matrix-vector products, versus one
for a GMRES Arnoldi step; raw counts are not comparable work units.

LIS `-conv_cond 0` tests `‖w−A2*x‖₂ / ‖w−A2*x0‖₂`; because **x0 = 0** and
scaling is disabled, the denominator is exactly **‖w‖₂**.

- Solver status: **normal end**.
- Iteration count: **17**.
- LIS-reported relative residual: **4.259715e-13** (driver prints seven significant digits).
- Independently recomputed relative residual: **4.2597938356252448e-13**.
- Independently recomputed absolute residual: **1.9393812205034772e-08**.

The original-system check satisfies the prescribed 1e−12 tolerance.

### 9. Import x and export its image

The indexed LIS-vector reader checks the header, dimensions, index ranges,
duplicates, missing records and finite values. It does not assume sorted records.
Reshape the imported solution and export `results/images/task09_lis_solution.png`.
Its raw intensity range is [-1.0634304680638731, 268.63985403864427].
This is an inverse-filter solution, not the forward sharpened image from task 7.

### 10. Construct the edge operator A3

Use

```text
Hed2 = [ −1  0  1
         −2  0  2
         −1  0  1 ]
```

**A3 is not symmetric.** Opposite offsets have opposite weights, including
at boundaries under zero padding, so **A3ᵀ = −A3**.
The computed check `‖A3+A3ᵀ‖F` is **0**.
Its nonzero count is `2(n−1)(3m−2) = 2575460`.

### 11. Detect edges in the original image

Compute **A3 v**, retaining signed numerical values. The raw range is
[-758, 816]; the independent stencil
difference is **0**.
The required output `results/images/task11_edges.png` clips negative responses
to black. The extra `task11_edges_absolute.png` displays both edge polarities
using `clip(abs(A3*v),0,255)`; it is only a visualization, never a solver input.

### 12. Solve (4I + A3)y = w with Eigen

Form **M = A3 + 4I** by bulk sparse addition. Repeated insertion into an
already-compressed matrix would be needlessly expensive at this image size.
M remains nonsymmetric, so ordinary CG is not justified.

For any real z, skew-symmetry gives `zᵀA3z = 0`, hence
`zᵀMz = 4‖z‖₂²`. A nonzero null vector is therefore impossible: the solution
is unique. This does **not** make M SPD, since it is not symmetric.

Use `Eigen::BiCGSTAB<Sparse, Eigen::DiagonalPreconditioner<double>>` from
`<Eigen/IterativeLinearSolvers>`, with tolerance **1e−10**, maximum **2000**
iterations and zero initial guess. BiCGSTAB has no GMRES-style restart parameter.

**Why this method was chosen:** M is nonsymmetric, so BiCGSTAB is suitable
without assuming SPD structure. Its short recurrences use O(N) working-vector
storage instead of GMRES's O(kN) basis storage for restart length k. The
controlled three-run comparison below found this configuration faster than
GMRES(60)/identity while passing the same true-residual tolerance. The submission
uses the tested configuration; this run takes **52** iterations.

The diagonal of A3 is zero, so the diagonal preconditioner for M is **D = 4I**.
Applying its inverse is just multiplication by 1/4: the scaled matrix is
`I + A3/4`. Scalar scaling leaves the 2-norm condition number unchanged and
does not remove the off-diagonal coupling. It can change floating-point
behavior, but it is not substantive conditioning improvement here. We retain
the tested diagonal configuration without crediting it with a conditioning gain.

BiCGSTAB can have irregular residuals or breakdown, whereas GMRES pays for
orthogonalization; neither is uniformly best. We require both **Eigen::Success** and an independently
recomputed residual on the original matrix, and check it again after reimporting y.

- Solver status: **Eigen::Success**.
- Iteration count: **52**.
- Solver-reported relative residual: **2.3316249112390609e-11**.
- Independently recomputed relative residual: **2.3316248560842142e-11**.
- Independently recomputed absolute residual: **1.0615324669779776e-06**.

Both residual checks satisfy the prescribed 1e−10 tolerance.
The unrounded solution is saved in `results/data/y_eigen.mtx`.

### 13. Reimport y and export its image

`Eigen::loadMarketVector` reads the array-format solution into an Eigen vector.
It has **430336** components. After the disk round trip,
its relative residual is still **2.3316248560842142e-11**.
Reshape to 656 × 656 and export `results/images/task13_eigen_solution.png`.
The raw intensity range is [-14.053171545773221, 82.060236512572388].
Its darker appearance is expected: on a constant interior region, A3 annihilates
the constant and `4y = w`, so this solve is not brightness-preserving.
Do not normalize y merely to make the displayed result resemble the original.

## Controlled comparison: BiCGSTAB versus GMRES

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
| 8 | LIS GMRES(40) + ILU(0), comparison alternative | 33 | 4.773250e-13 | 0.413 s |
| 8 | LIS BiCGSTAB + ILU(0), submission choice | 17 | 4.259794e-13 | 0.215 s |
| 12 | Eigen GMRES(60) + identity, comparison alternative | 47 | 9.102669e-11 | 0.433 s |
| 12 | Eigen BiCGSTAB + diagonal, submission choice | 52 | 2.331625e-11 | 0.174 s |

**Measured conclusion:** The selected BiCGSTAB configurations were faster for both tested systems,
while all methods met the prescribed true-residual tolerances.

The median GMRES/BiCGSTAB time ratios are **1.92** for task 8 and
**2.48** for task 12 (a value above one favors BiCGSTAB).
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


## Validation and source context

`results/logs/tests.log` records passing tests for:

- Row-wise flatten/reshape and a hand-computed 2 × 2 border example.
- All three filter products and nonzero formulas on 30 small image shapes,
  including nonsquare images, single-row/column images and no row wrapping.
- Kernel orientation, smoothing symmetry and Sobel skew-symmetry.
- Fixed-seed noise, clipping/rounding, and a lossless PNG round trip.
- Precision-preserving Eigen/LIS file round trips; unordered/commented LIS
  vectors and rejection of malformed records.
- A manufactured Eigen solution and a zero right-hand side.
- Eleven report/comparison regression tests: data-driven timings, failed or missing
  trials, duplicate diagnostics, invalid/excessive residuals, stale input hashes,
  and rejection of the wrong submission solver or malformed LIS diagnostics.

The report generator also checks analytic nonzero counts, vector dimensions,
symmetry/skew-symmetry, exact recorded stencil agreement, consistency of absolute
and relative residuals, and each PNG's dimensions and 8-bit grayscale header.

On the full deer image, all three sparse forward products match the independent
stencil exactly. Both inverse solutions pass independently recomputed residual
checks on the **original, unscaled systems**. Small algebraic residuals do not
imply that the noisy inverse images are visually identical to the original.

The implementation follows `Challenge1.pdf` (all three pages), the supplied
Lab1 image headers/tutorial, Lab2 Eigen/LIS interoperability and solver material,
the corresponding lab transcripts, and the relevant combined course-note
passages (`lab:image-layout`, `lab:filter-assembly`, `lab:matrix-market`,
`lab:solver-checks`, `lab:challenge-solves`). Solver reasoning is consistent with
`P2_Antonietti_NLA_26_27.pdf`, particularly the nonsymmetric methods on pages 86–91.
LIS options/vector records were checked against `lis_userguide.pdf` pages 36–38
and 175; Eigen's BiCGSTAB and comparison GMRES APIs were checked against its
installed 3.4.0 headers.
The course TeX and PDF were used as context and were not modified.

## Reproduce and submit

From this folder on the host, run `bash run_docker.sh`. It uses the same Docker
container and startup file as the fish `amsc` alias, builds and executes tasks
in order, then copies `results/` and this report back into this folder.
No files are submitted automatically. Enter the numeric answers in Microsoft
Forms and upload the requested PNGs there; upload the implementation separately
as described in the lab transcript. See `README.md` for the file inventory.
