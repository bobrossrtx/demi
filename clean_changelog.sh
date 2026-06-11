#!/bin/bash
# Clean ANSI escape sequences from changelog files

input_file=${1:-"CHANGELOG.md"}
temp_file="${input_file}.tmp"

if [[ ! -f ${input_file} ]]; then
	echo "Error: File ${input_file} not found"
	exit 1
fi

echo "Cleaning ANSI escape sequences from ${input_file}..."

# Remove ANSI escape sequences and other control characters
sed -E 's/\x1B\[[0-9;]*[JKmsu]//g' "${input_file}" >"${temp_file}"

# Replace the original file
mv "${temp_file}" "${input_file}"

echo "Cleaned ${input_file} successfully"
