#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <install-directory> <expected-architecture>" >&2
    exit 1
fi

install_directory=$1
expected_architecture=$2

if [[ ! -d "$install_directory" ]]; then
    echo "Install directory does not exist: $install_directory" >&2
    exit 1
fi

# Keep the list in a file so find failures propagate and the count stays in this shell.
binary_list=$(mktemp)
trap 'rm -f "$binary_list"' EXIT
find "$install_directory" -type f \( -name '*.dylib' -o -perm -111 \) -print0 > "$binary_list"

count=0
while IFS= read -r -d '' binary; do
    architectures=$(lipo -archs "$binary")
    if [[ "$architectures" != "$expected_architecture" ]]; then
        echo "Unexpected architecture for $binary: $architectures (expected $expected_architecture)" >&2
        exit 1
    fi
    count=$((count + 1))
done < "$binary_list"

if [[ $count -eq 0 ]]; then
    echo "No binaries found in: $install_directory" >&2
    exit 1
fi

echo "Verified $count binaries: $expected_architecture"
