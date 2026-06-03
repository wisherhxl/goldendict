#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

case "${1:-}" in
    "")
        exec "$script_dir/scripts/linux/vscode-debug.sh"
        ;;
    -rel|--release)
        exec "$script_dir/scripts/linux/vscode-release.sh"
        ;;
    *)
        echo "Usage: ./vscode.sh [-rel]" >&2
        exit 2
        ;;
esac
