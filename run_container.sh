#!/usr/bin/env bash
# Run inside amsc after: module load gcc-glibc/11.2.0 lis
set -euo pipefail
cd "$(dirname "$0")"
: "${mkEigenInc:?Load gcc-glibc/11.2.0 first}"
: "${mkLisInc:?Load lis first}"
: "${mkLisLib:?Load lis first}"
export OMP_NUM_THREADS=1
make -j2

for task in 1 2 3 4 5 6 7; do
    ./build/challenge1 "$task" deer.jpg results
done

# Task 8: solve A2*x=w with a zero initial guess and relative tolerance 1e-12.
./build/lis_test1 results/data/A2.mtx results/data/w.mtx \
    results/data/x_lis.mtx build/lis_history.txt \
    -i bicgstab -p ilu -ilu_fill 0 -maxiter 2000 \
    -tol 1e-12 -conv_cond 0 -initx_zeros 1 -scale none -print mem \
    2>&1 | tee build/lis.log

for task in 9 10 11 12 13; do
    ./build/challenge1 "$task" deer.jpg results
done
python3 make_report.py results SOLUTION.md build/lis.log
