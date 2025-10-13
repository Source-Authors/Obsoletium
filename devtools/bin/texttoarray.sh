#!/usr/bin/env bash

set -eu
set -o pipefail

# Usage: texttoarray.sh <filename> <name>
if [ -z "$2" ]; then
    echo "Usage: texttoarray.sh <filename> <name>"
    exit 1
fi

filename="$1"
objname="$2"

# Read the file and convert to an array format.
data=()
while IFS= read -r -n1 byte; do
    data+=("$(printf '%02x' "'$byte")")
done < "$filename"

# Output the C++ array.
echo "static constexpr unsigned char ${objname}[] = {"
for ((i=0; i<${#data[@]}; i++)); do
    printf "0x${data[i]},"
    if (( (i + 1) % 20 == 0 )); then
        echo ""
    fi
done
echo "0x00};"
