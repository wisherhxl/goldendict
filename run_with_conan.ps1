# SPDX-License-Identifier: GPL-3.0-or-later

$launcher = Join-Path $PSScriptRoot "scripts\run_with_conan.py"
& python $launcher @args
exit $LASTEXITCODE
