# Challenge 1 — Final results

The supplied image has 656 × 656 pixels. Sparse operators use row-wise
indexing and zero padding, with the assigned kernels applied without flipping.

| Task | Answer / output |
|---|---|
| 1 | Matrix size: **656 × 656** (height × width). |
| 2 | [Noisy image](results/images/task02_noisy.png). |
| 3 | Both vectors have **430336** components; **‖v‖₂ = 41415.341203471929**. |
| 4 | **3865156** nonzeros in A1. |
| 5 | [Smoothed noisy image](results/images/task05_smoothed.png). |
| 6 | **2149056** nonzeros in A2; **not symmetric**. |
| 7 | [Sharpened original image](results/images/task07_sharpened.png). |
| 8 | LIS BiCGSTAB + ILU(0): **17 iterations**, final relative residual **4.259715e-13**. |
| 9 | [LIS solution image](results/images/task09_lis_solution.png). |
| 10 | A3 is **not symmetric**; it is **skew-symmetric**. |
| 11 | [Edge-filtered original image](results/images/task11_edges.png). |
| 12 | Eigen BiCGSTAB + diagonal preconditioning: **52 iterations**, final relative residual **2.331624911239061e-11**. |
| 13 | [Eigen solution image](results/images/task13_eigen_solution.png). |

The forward filters compute **A1 w**, **A2 v** and **A3 v**. The inverse
systems are **A2 x = w** (LIS, tolerance 1e−12) and **(4I + A3)y = w**
(Eigen, tolerance 1e−10). Both solvers start from zero and allow 2000 iterations.

Residuals are relative Euclidean norms, ‖b − Ax‖₂ / ‖b‖₂. The saved
solutions satisfy the tolerances when checked against the original systems:

| System | Checked relative residual | Absolute residual |
|---|---:|---:|
| A2 x = w | 4.2597938356252448e-13 | 1.9393812205034772e-08 |
| (4I + A3)y = w | 2.3316248560842142e-11 | 1.0615324669779776e-06 |

Noise is uniform in [−50, 50], with seed **2026**. Numerical
vectors retain unclipped double-precision values; exported PNGs clip to [0, 255]
and round to the nearest integer. Negative edge responses therefore appear black.
The task-13 image retains its computed intensity scale.

Matrix Market files are in `results/data/`, task measurements in
`results/metrics/`, and the six requested images in `results/images/`.
Run `bash run_docker.sh` to regenerate the results and this answer sheet.
