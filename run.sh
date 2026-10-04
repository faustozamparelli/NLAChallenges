#!/bin/bash
set -eo pipefail

export LOGNAME="${LOGNAME:-$(id -un)}"

repo_dir=$(cd "$(dirname "$0")" && pwd)
cd "$repo_dir"

image=${1:-deer.jpg}
output=${2:-results}

source /u/sw/etc/profile
module load gcc-glibc
module load eigen
module load lis
set -u

build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT

g++ -O2 -std=c++17 -I"${mkEigenInc}" -Ithird_party challenge1.cpp -o "$build_dir/challenge1"
mpicc -DUSE_MPI -I"${mkLisInc}" third_party/lis_test1.c -L"${mkLisLib}" -llis -o "$build_dir/lis_solver"

mkdir -p "$output"
for task in 1 2 3 4 5 6 7; do
    "$build_dir/challenge1" "$task" "$image" "$output"
done

"$build_dir/lis_solver" "$output/data/A2.mtx" "$output/data/w.mtx" \
    "$output/data/x_lis.mtx" "$output/hist.txt" -i bicgstab -p ilu -tol 1.0e-12 -maxiter 2000 \
    | tee "$output/lis_output.txt"
iterations=$(awk '/BiCGSTAB: number of iterations =/ { print $NF }' "$output/lis_output.txt")
solver_residual=$(awk '/BiCGSTAB: relative residual/ { print $NF }' "$output/lis_output.txt")

for task in 9 10 11 12 13; do
    "$build_dir/challenge1" "$task" "$image" "$output"
done

true_residual=$(awk -F= '$1 == "relative_residual" { print $2 }' "$output/metrics/task9.txt")
absolute_residual=$(awk -F= '$1 == "absolute_residual" { print $2 }' "$output/metrics/task9.txt")
{
    printf 'solver=BiCGSTAB\n'
    printf 'preconditioner=ILU(0)\n'
    printf 'tolerance=1e-12\n'
    printf 'iterations=%s\n' "$iterations"
    printf 'solver_relative_residual=%s\n' "$solver_residual"
    printf 'relative_residual=%s\n' "$true_residual"
    printf 'absolute_residual=%s\n' "$absolute_residual"
} > "$output/metrics/task8.txt"