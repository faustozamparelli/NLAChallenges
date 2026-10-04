# NLA Challenge 1

This challenge uses sparse linear algebra to filter and denoise the supplied
grayscale image. The 13 tasks add noise, build smoothing, sharpening and edge
operators, apply them to image vectors, and solve two systems with LIS and Eigen.

[Challenge1.pdf](Challenge1.pdf) contains the assignment.

## Run

In the course container, run `bash run.sh` from this directory. The script
builds the Eigen and LIS programs, runs all 13 tasks in order, checks both
solution residuals, and writes outputs under `results/`. Optional arguments
are the input image and output directory: `bash run.sh deer.jpg results`.

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
| 10 | A3 is **not symmetric**; it is **skew-symmetric**. |
| 11 | [Edge-filtered original image](results/images/task11_edges.png). |
| 12 | Eigen BiCGSTAB + diagonal preconditioning: **48 iterations**, final relative residual **9.715645986150306e-11**. |
| 13 | [Eigen solution image](results/images/task13_eigen_solution.png). |

The forward filters compute **A1 w**, **A2 v** and **A3 v**. The inverse
systems are **A2 x = w** (LIS, tolerance 1e−12) and **(4I + A3)y = w**
(Eigen, tolerance 1e−10). Both solvers start from zero and allow up to 2000
iterations.

Residuals are relative Euclidean norms, ‖b − Ax‖₂ / ‖b‖₂. The saved
solutions satisfy the tolerances when checked against the original systems:

| System | Checked relative residual | Absolute residual |
|---|---:|---:|
| A2 x = w | 7.644276195653998e-13 | 3.448697983765888e-08 |
| (4I + A3)y = w | 9.7156459808474435e-11 | 4.3831917957361468e-06 |

Sparse operators use row-wise indexing, zero padding and unflipped kernels.
Noise is uniform integer in [−50, 50], with seed **42**. Noisy pixel values are
clipped to [0, 255] before vectorization; PNG pixels are rounded to bytes.
Negative edge responses appear black, and the task-13 image retains its computed
intensity scale.

Final images, Matrix Market data and task measurements are saved in
`results/images/`, `results/data/` and `results/metrics/`.

The implementation uses C++17, Eigen, LIS and the bundled stb image headers.
