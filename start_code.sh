#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

case "${1:-}" in
    ""|debug|dbg|d)
        exec "$script_dir/scripts/linux/vscode-debug.sh"
        ;;
    release|rel|r)
        exec "$script_dir/scripts/linux/vscode-release.sh"
        ;;
    *)
        echo "Usage: ./start_code.sh [debug|dbg|d|release|rel|r]" >&2
        exit 2
        ;;
esac
