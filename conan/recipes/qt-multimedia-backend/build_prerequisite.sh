#!/usr/bin/env bash

set -o pipefail

readonly source_dir="/home/log/Workspace/GoldenDict-tiger-qt6/.worktrees/audio-multimedia-backend-prereq"
readonly output_dir="${source_dir}/build/audio-multimedia-backend"
readonly log_dir="${output_dir}/logs"
readonly harfbuzz_package_ref="harfbuzz/12.3.0:3d771bc348e5e43ad7250605604a18caa44ce972"

mkdir -p "${log_dir}"
cd "${source_dir}"
rm -f "${log_dir}/conan-install.exit"

conan export conan/recipes/python-html5lib
conan export conan/recipes/qt-libalsa-buildenv
python3 conan/recipes/qt-multimedia-backend/export_recipe.py

# Qt WebEngine runs freshly linked host tools while its Conan dependencies are
# still isolated in the cache. Make HarfBuzz's subset library discoverable to
# those tools; otherwise the build reaches the V8 snapshot step and exits 127.
harfbuzz_package_dir="$(conan cache path "${harfbuzz_package_ref}")"
export LD_LIBRARY_PATH="${harfbuzz_package_dir}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

conan install . \
  -pr:h profiles/qt-webengine \
  -pr:b default \
  -s:h build_type=Release \
  --build=missing \
  -c tools.build:jobs=1 \
  -of "${output_dir}" \
  2>&1 | tee "${log_dir}/conan-install.log"
status=${PIPESTATUS[0]}
printf '%s\n' "${status}" > "${log_dir}/conan-install.exit"
exit "${status}"
