#!/usr/bin/env sh
set -eu

build_type=Debug
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

if ! command -v conan >/dev/null 2>&1; then
    echo "Conan was not found in PATH." >&2
    exit 1
fi

if ! command -v code >/dev/null 2>&1; then
    echo "VS Code command 'code' was not found in PATH." >&2
    exit 1
fi

cd "$repo_root"

conan install . --build=missing -s build_type="$build_type"

generators_dir="$repo_root/build/$build_type/generators"
if [ ! -d "$generators_dir" ] && [ -d "$repo_root/build/generators" ]; then
    generators_dir="$repo_root/build/generators"
fi

if [ -f "$generators_dir/deactivate_conanrun.sh" ]; then
    . "$generators_dir/deactivate_conanrun.sh"
fi
if [ -f "$generators_dir/deactivate_conanbuild.sh" ]; then
    . "$generators_dir/deactivate_conanbuild.sh"
fi

if [ -f "$generators_dir/conanbuild.sh" ]; then
    . "$generators_dir/conanbuild.sh"
fi
if [ -f "$generators_dir/conanrun.sh" ]; then
    . "$generators_dir/conanrun.sh"
fi

code "$repo_root"
