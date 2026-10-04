#!/usr/bin/env bash
# Inside amsc: module load gcc-glibc/11.2.0 lis; bash results/solver_comparison/run_comparison.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${mkEigenInc:?Load gcc-glibc/11.2.0}"
: "${mkLisInc:?Load lis}"
: "${mkLisLib:?Load lis}"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export OMP_NUM_THREADS=1
cd "$work"
cp "$root/results/data/A2.mtx" "$root/results/data/w.mtx" "$root/deer.jpg" .
g++ -O2 -std=c++17 -Wall -Wextra -Wpedantic -I"$root" -I"$mkEigenInc" -isystem "$root/third_party" \
    "$root/results/solver_comparison/compare.cpp" -o compare
mpicc -O2 -DUSE_MPI -fopenmp -I"$mkLisInc" "$root/third_party/lis_test1.c" \
    -L"$mkLisLib" -Wl,-rpath,"$mkLisLib" -llis -lm -o lis_test1
./compare > eigen_comparison.log
for method in gmres bicgstab; do
    for trial in 1 2 3; do
        ./lis_test1 A2.mtx w.mtx "x_${method}_${trial}.mtx" "history_${method}_${trial}.txt" \
            -i "$method" -p ilu -ilu_fill 0 -restart 40 -maxiter 2000 \
            -tol 1e-12 -conv_cond 0 -initx_zeros 1 -scale none -print mem \
            > "lis_${method}_${trial}.log" 2>&1
        ./compare "x_${method}_${trial}.mtx" >> "lis_${method}_${trial}.log"
    done
done
(
    cd "$root"
    sha256sum deer.jpg results/data/A2.mtx results/data/w.mtx image_filters.hpp \
        results/solver_comparison/compare.cpp results/solver_comparison/run_comparison.sh \
        third_party/lis_test1.c \
        third_party/stb_image.h third_party/stb_image_write.h
) > input_hashes.txt
{
    g++ --version
    grep -E '^#define EIGEN_(WORLD|MAJOR|MINOR)_VERSION' "$mkEigenInc/Eigen/src/Core/util/Macros.h"
    printf 'LIS library: %s\nOMP_NUM_THREADS=%s\nTrials=3\n' "$mkLisLib" "$OMP_NUM_THREADS"
} > environment.txt
cp eigen_comparison.log lis_*.log input_hashes.txt environment.txt "$root/results/solver_comparison/"
printf 'Comparison logs saved to %s/results/solver_comparison\n' "$root"
