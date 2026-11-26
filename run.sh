#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
make -s -C "$project_dir" >&2
exec "$project_dir/build/config15" "$@"
