#!/usr/bin/env bash
# Run inside amsc after: module load gcc-glibc/11.2.0 lis
set -euo pipefail
cd "$(dirname "$0")"
: "${mkEigenInc:?Load gcc-glibc/11.2.0 first}"
: "${mkLisInc:?Load lis first}"
: "${mkLisLib:?Load lis first}"
export OMP_NUM_THREADS=1
mkdir -p results/logs
make -j2 2>&1 | tee results/logs/build.log
make test 2>&1 | tee results/logs/tests.log
{
    printf 'Container: amsc\n'
    g++ --version
    printf 'Eigen: '
    grep -E '^#define EIGEN_(WORLD|MAJOR|MINOR)_VERSION' "$mkEigenInc/Eigen/src/Core/util/Macros.h"
    printf 'LIS module: %s\n' "$mkLisLib"
    printf 'OMP_NUM_THREADS=%s\n' "$OMP_NUM_THREADS"
    sha256sum deer.jpg challenge1.cpp image_filters.hpp test_challenge1.cpp \
        Makefile run_container.sh make_report.py comparison_report.py test_reports.py third_party/*
} > results/logs/environment.txt

for task in 1 2 3 4 5 6 7; do
    ./build/challenge1 "$task" deer.jpg results | tee "results/logs/task${task}.log"
done

# Task 8: the actual image right-hand side, not the special RHS settings 1/2.
# Use the tested BiCGSTAB/ILU(0) configuration. With x0=0 and scale=none,
# conv_cond=0 uses ||r0||2 = ||w||2 for relative-residual normalization.
./build/lis_test1 results/data/A2.mtx results/data/w.mtx \
    results/data/x_lis.mtx results/logs/lis_history.txt \
    -i bicgstab -p ilu -ilu_fill 0 -maxiter 2000 \
    -tol 1e-12 -conv_cond 0 -initx_zeros 1 -scale none -print mem \
    2>&1 | tee results/logs/task8_lis.log

for task in 9 10 11 12 13; do
    ./build/challenge1 "$task" deer.jpg results | tee "results/logs/task${task}.log"
done
bash results/solver_comparison/run_comparison.sh
python3 make_report.py results SOLUTION.md
