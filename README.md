# NLA Challenge 1

Solutions to the 13 tasks in [Challenge1.pdf](Challenge1.pdf), using Eigen,
LIS and stb for image loading and saving.

## Run

In the course container, run `bash run.sh` from this directory. The script
builds the Eigen and LIS programs, runs all 13 tasks in order, checks both
solution residuals, and writes outputs under `results/`. Optional arguments
are the input image and output directory: `bash run.sh deer.jpg results`.

[submission.txt](submission.txt) contains the final form answers and the PNG
upload paths for all 13 tasks.

## Results

| Task | Answer / output |
|---|---|
| 1 | Matrix size: **656 × 656** (height × width). |
| 2 | [Noisy image](results/images/task02_noisy.png). |
| 3 | Both vectors have **430336** components; **‖v‖₂ = 41415.341203471929**. |
| 4 | **3865156** nonzeros in A1. |
| 5 | [Smoothed noisy image](results/images/task05_smoothed.png). |
| 6 | **2149056** nonzeros in A2; **not symmetric**. |
| 7 | [Sharpened original image](results/images/task07_sharpened.png). |
| 8 | LIS BiCGSTAB + ILU(0): **18 iterations**, final relative residual **7.644217e-13**. |
| 9 | [LIS solution image](results/images/task09_lis_solution.png). |
| 10 | **2575460** nonzeros in A3; **not symmetric**, but **skew-symmetric**. |
| 11 | [Edge-filtered original image](results/images/task11_edges.png). |
| 12 | Eigen BiCGSTAB + diagonal preconditioning, initial guess **w/4**: **46 iterations**, final relative residual **2.7198720144561966e-11**. |
| 13 | [Eigen solution image](results/images/task13_eigen_solution.png). |

The forward filters compute **A1 w**, **A2 v** and **A3 v**. The inverse
systems are **A2 x = w** (LIS, tolerance 1e−12) and **(4I + A3)y = w**
(Eigen, tolerance 1e−10). LIS starts from zero; Eigen starts from **y0 = w/4**.
Both solvers allow up to 2000 iterations.

For the Eigen system, the diagonal is **D = 4I**, so **y0 = D⁻¹w = w/4**
solves the diagonal approximation and equals one Jacobi step from zero.
Its initial residual is **w − (4I + A3)y0 = −A3w/4**. In a controlled comparison
using the same matrix, seed-42 noisy vector, solver and tolerance, this guess
reduced the initial relative residual from **1** to approximately **0.390** and
the iteration count from **48** to **46**. The diagonal preconditioner itself
is uniform scaling by 1/4; it does not change the condition number.

Residuals are relative Euclidean norms, ‖b − Ax‖₂ / ‖b‖₂. The saved
solutions satisfy the tolerances when checked against the original systems:

| System | Checked relative residual | Absolute residual |
|---|---:|---:|
| A2 x = w | 7.644276195653998e-13 | 3.448697983765888e-08 |
| (4I + A3)y = w | 2.7198721953538819e-11 | 1.2270642132934253e-06 |

Sparse operators use row-major storage, row-wise pixel indexing, zero padding
and unflipped kernels. Row-major sparse storage suits the repeated matrix-vector
products in BiCGSTAB and is recommended for this solver in the
[Eigen documentation](https://libeigen.gitlab.io/eigen/docs-nightly/classEigen_1_1BiCGSTAB.html).
Noise is uniform integer in [−50, 50], with seed **42**. Noisy pixel values are
clipped to [0, 255] before vectorization; PNG pixels are rounded to bytes.
Negative edge responses appear black, and the task-13 image retains its computed
intensity scale.

Final images, Matrix Market data and task measurements are saved in
`results/images/`, `results/data/` and `results/metrics/`.

The implementation uses C++17, Eigen, LIS and the bundled stb image headers.
