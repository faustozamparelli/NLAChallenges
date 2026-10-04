# NLA Challenge 1

This challenge uses sparse linear algebra to filter and denoise the supplied
grayscale image. The 13 tasks add noise, build smoothing, sharpening and edge
operators, apply them to image vectors, and solve two systems with LIS and Eigen.

[Challenge1.pdf](Challenge1.pdf) contains the assignment.
[SOLUTION.md](SOLUTION.md) lists the final answers and links to the six output images.

The implementation uses C++17, Eigen, LIS and the bundled stb image headers.
Noise uses seed 2026; numerical values retain full precision, while PNG pixels
are clipped to 0–255 and rounded.

To reproduce the results with the configured `amsc` Docker container:

```bash
bash run_docker.sh
```

Inside the container, with this folder available:

```bash
module load gcc-glibc/11.2.0 lis
bash run_container.sh
```

The runners build the executables, run all tasks and regenerate `SOLUTION.md`.
Final images, Matrix Market data and task measurements are saved in `results/`.
