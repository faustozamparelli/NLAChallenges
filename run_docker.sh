#!/usr/bin/env bash
# Build and run the challenge in the amsc container.
set -euo pipefail
root=$(cd "$(dirname "$0")" && pwd)
shared="$HOME/shared-folder"
mkdir -p "$shared"
staging=$(mktemp -d "$shared/challenge1.XXXXXX")
trap 'rm -rf "$staging"' EXIT
for file in Makefile challenge1.cpp image_filters.hpp \
            run_container.sh make_report.py deer.jpg; do
    cp "$root/$file" "$staging/"
done
cp -R "$root/third_party" "$staging/"
container_dir="/shared-folder/$(basename "$staging")"
docker start amsc >/dev/null
docker exec -w "$container_dir" amsc /bin/bash \
    --rcfile /u/sw/etc/bash.bashrc -ic \
    'module load gcc-glibc/11.2.0 lis && bash run_container.sh'
cp -R "$staging/results" "$root/"
cp "$staging/SOLUTION.md" "$root/SOLUTION.md"
printf '\nSaved results to %s/results and %s/SOLUTION.md\n' "$root" "$root"
